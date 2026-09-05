# scale-probe 接口打穿报告(REPORT.md)

> 丢弃式 spike 的交付物(R4 + R5)。基于 `XQ/probe/` 在真 Win32 mmap 后备 + 真规模负载下实跑得出。
> 用途:libXQ/Source 冻结门 + M8b-2 §冲突决策的输入。代码可弃,**结论回灌接口形状后探针使命完成**。
> 任务 06-30-scale-probe。实测环境:Windows + MSVC Release,本机实跑。

## 0. 实跑数据(small=1M tris/0.5M tets/128³ vox;large=20M tris/10M tets/512³ vox=128MiB)

| 指标 | small | large |
|---|---|---|
| R1 合成负载生成耗时 / 生成期峰值内存 | 301ms / **25 MiB** | 5968ms / **25 MiB** |
| mmap open+verify(全量)+acquire_whole | 12.4ms | 752ms / 274 MiB |
| VTK addSurface(build-handle / 上传) | 30.9ms / 555ms | 726ms / **23515ms** |
| R5-A 全量验证 acquire_slab(bytesHashed) | 11.5ms / 2 MiB | **805ms / 128 MiB** |
| R5-B 分段 Merkle acquire_slab(bytesHashed) | **5.8ms / 1 MiB** | **5.8ms / 1 MiB** |
| 进程峰值工作集(含 VTK 上传) | 216 MiB | **3786 MiB** |
| AC2 round-trip(voxel/points/tris+faceId/tets) | 全 PASS | 全 PASS |

**关键读数**:
- 生成期内存恒定 25 MiB(流式 writer 成立,20M 规模不爆)。
- R5-B 的 bytesHashed 在 small/large **都是 1 MiB**(只校验触碰的 1 个段),而 R5-A 随 blob 线性涨到 128 MiB——**大规模下惰性校验比全量快 ~128×(805ms→5.8ms)**。这是完整性 vs 流式冲突的量化铁证。
- large 进程峰值 3786 MiB 几乎全来自 VTK 上传链(addSurface 把 10M 点 resident handle 灌进 vtkPoints/vtkCellArray + 下游 filter),不是 mmap 本身(mmap acquire 仅 274 MiB)。

## 1. 四类缺陷的接口形状结论(R4)

### ① 完整性 vs 流式冲突 —— 当前接口能承载,但完整性策略必须换
- **现状**:M8a `BlobStore::readVerified` 整文件读堆 + 整块 SHA(`BlobStore.cpp:222-265`)。探针的 `MappedVoxelSource` 绕开 `get()`,实现了 A(全量验证)与 B(分段 Merkle)两条路。
- **实测**:A 在 large 下 acquire 任意一个 slab 都要 hash 全部 128 MiB(805ms);B 只 hash 触碰的 1 个段(1 MiB,5.8ms),用 `Sha256::update`+picosha2 对 mmap 子段算哈希、无新依赖。
- **接口含义**:`IVoxelSource` 的 `acquire_region/slab` 签名本身**能承载**惰性(它已经是"按 extent 取块")。真正要改的是**完整性模型而非 Source 签名**:M8b-2 必须落地分段 Merkle(段哈希表存 `.merkle` 边车或扩 `BufferRef` schema),否则 `acquire_region` 在大 blob 上被迫 all-or-nothing 全量校验,惰性映射的全部收益被抹掉。
- **给冻结门**:Source acquire 粒度签名可冻;完整性策略**不在 Source 层**,留 M8b-2 决策(推荐分段 Merkle,数据见上)。

### ② reader 拷贝 / 零拷贝读路径 —— 已验证零拷贝可行,接口契约成立
- `BlobStore::get` 逐元素 readLE 解码成 `vector`(`BlobStore.cpp:267-325`),`MappedVoxelSource` 不能复用。探针证明:mmap 原始字节直接当 `ReadSpan<uint8_t>`(voxel)/ `reinterpret_cast<const Point3*>`(几何)喂 M8b-1 lease,**零拷贝、AC2 逐元素比对全 PASS**。
- **接口含义**:M8b-1 的 `ReadSpan`+`borrow`(keepalive 持 mmap)契约对 mmap 后备**天然适配,无需改签名**。M8b-2 的真 reader 应走这条"map+verify+reinterpret-span"路,不要走 `get()`。

### ③ lease 是并发原语,不是单线程 RAII —— 当前接口不足,必须加 pin/读锁
- **实测**(AC4 harness):
  - **A 优雅 evict**(evictor 只在 `use_count==1` 才 unmap):SAFE。证明 `shared_ptr` use_count **足以**做 evict gating——**前提是 evictor 主动honor它**。
  - **B 强制 evict**(evictor 无视 use_count 直接 `UnmapViewOfFile`):前台 borrow view 立即悬空,下次读 `EXCEPTION_ACCESS_VIOLATION`。**缺陷确认**:M8b-1 lease 没有任何 evictor 可见的 pin/锁,挡不住非 refcount 驱动的驱逐(LRU/内存预算主动 evict 正是这种)。
- **接口含义**:M8b-1 已**正确地**把 lease 失败/取消语义标为 candidate-not-frozen——本探针证明这个决定对了。M8b-2 的 `ReadLease` 必须升级为**并发读锁**:
  - 选项 a(推荐):lease 持有的 keepalive 是 evictor 也持有引用的同一对象,evictor 走"`use_count==1` 才回收"的协作式 gating(实测 A 安全)。要求**资源管理器承诺只走 refcount 驱动驱逐**。
  - 选项 b:给接口加显式 `pin()/unpin()` 或 `try_evict()` 钩子,让 evictor 能查询/阻塞活跃读者。
  - 任一都改 lease 签名/语义 → **冻结门前必须定**,否则 M9a 消费者迁在挡不住并发驱逐的 lease 上。

### ④ AoS vs SoA —— 零拷贝 reinterpret 成立;瓶颈在 VTK 上传不在布局
- **实测**:`Point3{double x,y,z}`(24B 无填充)+ x86 小端 → mmap 基址(页对齐,满足 8B 对齐)直接 `reinterpret_cast<const Point3*>` 零拷贝吐 `ReadSpan<Point3>`,AC2 逐值 PASS。`SourceTet`(int[4])同。**AoS 本身不阻碍 mmap 零拷贝**。
- **但**:large 下 VTK addSurface 23.5s + 峰值 3786 MiB,瓶颈是 `XQSceneRenderer` 逐三角 `InsertNextCell` + 每 tet 一个 `vtkNew<vtkTetra>`(`XQSceneRenderer.cpp:134-192`)+ resident handle 物化,**不是 AoS↔SoA**。
- **接口含义**:M8b-1 的 AoS reinterpret 契约(AC10)**可冻**,无需为 mmap 转 SoA。SoA 的真实驱动是 GPU 上传/渲染层(M9b),且要先改 `XQSceneRenderer` 的逐元素上传路径——**与 Source 接口形状无关**,不进冻结门。
- **附带缺陷**:`acquire_triangles` 的 faceId 每次 O(N) 物化(M8b-1 `ResidentSurfaceSource.cpp:44-51`;探针 large 下每次 acquire 多拷 80MB)。即便 faceId 是独立 blob 可零拷贝 borrow,M8b-1 `TriangleLease::borrow` 只收 owned `vector<int>`。**候选改动**:给 `TriangleLease` 增加"借用 faceId span"路径(需 handle/接口提供 faceId 连续访问器)。非阻断,但 M8b-2 大规模渲染前应处理。

## 2. acquire 粒度与失败模式(补充缺口)
- **region/slab 零拷贝缺口**:mmap 下 `acquire_region` 的子块在源里 x-fastest 跨行非连续,探针只能 own 物化拷贝(同 Resident)。`ReadSpan` 无 stride → 无法借出跨行子视图。**接口形状候选**:若大规模 ROI 访问要零拷贝,需 strided-view(带 row/slab pitch)。当前体素消费者(seg/render)多为整卷或整 slab,slab 是连续的可零拷贝;**纯子区域(ONNX roi)才需要 stride**,可留 M8b-2 视 ONNX 接入再定。
- **失败模式**:越界 extent 返回 invalid lease(实测 valid==false),无异常——M8b-1 现契约够用。并发驱逐下的失败(③)才是真缺口。

## 3. R5 完整性策略对比与推荐

| | A 验证一次后信任映射 | B 分段 Merkle |
|---|---|---|
| 保留惰性 | ❌ 全量触碰(large 805ms/128MiB) | ✅ 只校验触碰段(large 5.8ms/1MiB) |
| 大规模代价 | O(blob) 每首次访问 | O(触碰量) |
| 内容寻址完整性 | 整文件单 SHA(= M8a 现状) | 段哈希 + root(需建表) |
| 复用 | `Sha256::hashHex` 直接 | `Sha256::update` + picosha2 子段,无新依赖 |
| 存储 | 无额外 | 段哈希表(`.merkle` 边车 / 扩 `BufferRef`) |

**推荐**:M8b-2 走 **B 分段 Merkle**。大规模(本探针 large 即已 128× 差距)下 A 抹掉 mmap 全部收益;B 实测保持惰性且原语现成。段大小起点 1 MiB(探针默认),M8b-2 按真实 acquire 粒度调。段哈希表存储位置(`.merkle` 边车 vs 扩 schema)是 M8b-2 决策点——探针已证边车可行。

## 4. 冻结门 / M8b-2 输入清单(一句话结论)

- **可冻**(本探针验证形状正确):`ReadSpan` 零拷贝契约、`acquire_*` 取块签名、AoS reinterpret(AC10)、越界 invalid-lease 失败语义、`IVoxelSource`/`IGeometrySource` 虚函数集。
- **冻前必改**(本探针打穿):`ReadLease` 并发语义——必须从单线程 move-only RAII 升级为对抗并发驱逐的读锁/pin(缺陷③,选项 a 协作 gating 已实测安全 / 选项 b 显式 pin)。
- **不在 Source 层、留 M8b-2**:完整性策略(分段 Merkle,缺陷①)、reader 走 map+verify+reinterpret 而非 `get()`(缺陷②)、faceId 借用 span(缺陷④附带)、region strided-view(按 ONNX 接入定)。
- **不进冻结门**:AoS→SoA(瓶颈在 VTK 上传层 M9b,与 Source 无关)。

## 5. AC 达成

AC1 ✅(20M 三角+10M tet blob 流式生成,SHA 内容寻址,生成期内存恒 25MiB)。AC2 ✅(mmap 经接口吐 span 逐元素 == 源,small/large 全 PASS)。AC3 ✅(open-time/内存/VTK 上传基线,Resident vs mmap 对比见 §0)。AC4 ✅(harness A SAFE / B DEFECT CONFIRMED,给出 lease 并发签名建议)。AC5 ✅(本报告 R4 + R5 对比)。AC6 ✅(OFF 档主线 ctest 50/50,src/core/source 与现有消费者零改动)。
