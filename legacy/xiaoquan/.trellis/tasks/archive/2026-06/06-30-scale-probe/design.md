# scale-probe 探针设计(design.md)

> 丢弃式 spike。目标 = 产出「接口形状结论」,代码可弃。基于 4 路并行勘察(证据见下,均带 file:line)。
> 父任务准则:成本/返工不计,可走错路,方向不能偏。

## 0. 一句话定调

用**真 Win32 mmap 后备存储** + **真规模合成负载(20M 三角 / 10M tet / 大体素卷)**,把 M8b-1 的 `IVoxelSource`/`IGeometrySource`/`ReadLease` 在它最终必须支撑的四个最难场景下**打穿**,把暴露的形状缺陷写成「冻结前必须改的接口签名建议」。探针绝不改 `src/core/source/`,接口若需改由 M8b-1 收口。

## 1. 勘察坐实的关键事实(design 的地基)

1. **blob 无头 + 本机小端 + Point3 紧致 AoS** → mmap 后可零拷贝 `reinterpret_cast`:F64×3 blob 字节布局 == `Point3{double x,y,z}`(24B,无填充)连续数组 == M8b-1 `ReadSpan<Point3>`;I32×3/×4 == `std::array<int,3/4>`。**仅小端平台成立**(BlobStore encode 是显式 per-byte LE,x86 上 == 内存原生字节)。证据:`BlobStore.cpp:85-118`、`GeometryTypes.h:8-12`、AC10 已硬依赖(`test_source_interface.cpp:442-485`)。
2. **`BlobStore::put` 大规模会爆内存**:`vector<double> points`(240MB) + `encode` 产出 bytes(240MB)同时驻留,无分块/流式 put。`Sha256` 类本身支持流式 `update()`,但 `put`/`publish` 走一次性 `hashHex(整 vector)`。证据:`BlobStore.cpp:85-99,122-133,165`;`Sha256.h:17-26`。
3. **`readVerified` 整文件读进堆 + 整块 SHA-256 重算** → 与 mmap「只缺页可见块」结构性对立;`get` 逐元素 `readLE` 解码(非 memcpy),产出 `vector<double/int>` 形状与 `VoxelView` 期望的 `uint8_t` 字节 span 不同 → mmap 路径**必须绕开 `BlobStore::get`**。证据:`BlobStore.cpp:222-265,267-325`。
4. **全仓零 Win32 映射先例**(`CreateFileMapping`/`MapViewOfFile`/`windows.h` 全无)→ mmap 读路径是纯新代码。证据:rg 全树 No matches。
5. **M8b-1 lease 是单线程 move-only RAII,无任何锁**,借出寿命只靠 `keepalive`(`shared_ptr<const void>`)的 use_count 维系;**没有 evictor 可见的 pin/force-evict 钩子**。证据:`ReadLease.h:24-145`;全仓 grep mutex/thread 零命中。
6. **`MappedVoxelSource` 接入成本极小**:实现 `IVoxelSource` 4 个虚函数,把 {HANDLE, 映射基址, 长度} 包进自定义 deleter(`UnmapViewOfFile`+`CloseHandle`)的 `shared_ptr<const void>` 当 keepalive,`view.bytes = ReadSpan(base, size)`,走 `VoxelLease::borrow`——接口侧零改动,`scalarAt` 解码复用。证据:`ResidentVoxelSource.cpp:30-45`、`SourceViews.h:23-96`。
7. **CMake 有成熟「默认 OFF 可选块」惯例**(ONNX/TETGEN/MMG):`option(...)` + `if()` 包住全部目标,OFF 档主线 byte-for-byte 不变。证据:`CMakeLists.txt:493-592`。
8. **全仓无计时/内存测量基建**(chrono/QElapsedTimer/GetProcessMemoryInfo 零命中)→ 探针自带:`std::chrono::steady_clock` + Win32 `GetProcessMemoryInfo(PeakWorkingSetSize)`(链 psapi)。
9. **VTK 上传唯一路径** = `XQSceneRenderer::addSurface/addVolumeMesh`(内部 `build_surface/build_volume` 是 .cpp 私有匿名函数);已知热点:逐三角 `InsertNextCell`、每 tet 一个 `vtkNew<vtkTetra>` 堆分配。证据:`XQSceneRenderer.cpp:134-192,328-409`。
10. **faceId 非零拷贝**:`acquire_triangles` 每次 O(N) 物化 `vector<int>`(handle 无 faceId vector 访问器),20M 三角 = 每次 acquire 额外 80MB 分配。证据:`ResidentSurfaceSource.cpp:44-51`。

## 2. 决策:R1 既喂 IVoxelSource 也喂 IGeometrySource(两路全做)

勘察 open_question 暴露 prd 的张力:R1 文案是三角/tet(几何路),R2 是 MappedVoxelSource(体素路)。**裁决:两路都做**——四类缺陷分布在两条路上,只做一条打不穿:

| 缺陷 | 体素路(MappedVoxelSource) | 几何路(MappedGeometrySource) |
|---|---|---|
| ① 完整性 vs 流式 | ✅ 主战场(acquire_region/slab 触碰子块 vs 全量 SHA) | 次(points 全常驻,R2 约定不分块) |
| ② reader 拷贝/零拷贝读路径 | ✅ map+verify+reinterpret span | ✅ 同 |
| ③ lease 并发 evict | ✅ acquire_whole 借出期间后台 unmap | ✅ acquire_points 借出期间后台 unmap |
| ④ AoS vs SoA | 次(体素本就字节流) | ✅ 主战场(Point3 24B reinterpret + VTK 上传) |

共享底座:**mmap + reinterpret-span 零拷贝读路径**,体素路当 `ReadSpan<uint8_t>`,几何路当 `ReadSpan<Point3>`/`<SourceTet>`。

## 3. 探针架构(目录 `XQ/probe/`,默认 OFF)

R6 隔离:新建顶层 `XQ/probe/`(不进 src/ 污染生产库语义、不进 tests/ 那是断言测试)。CMake `option(XQ_ENABLE_SCALE_PROBE "Build throwaway scale probe" OFF)` + `if()` 块,块内 `add_executable(scale_probe probe/*.cpp)` + `target_link_libraries(PRIVATE xq_core xq_io xq_visualization)` + `vtk_module_autoinit(TARGETS scale_probe MODULES ${VTK_LIBRARIES})`。不 add_test(bench 不进 CI 绿灯门)。

组件:

### 3.1 合成负载生成器(R1)
确定性(固定种子,可复现)。两档规模:小档(冒烟,百万级)、大档(20M 三角 / 10M tet)。
- **几何**:生成扁平 `vector<double> points`(F64×3)、`vector<int> tris`(I32×3)、`faceId`(I32×1)、tet 的 `volPoints`/`tets`(I32×4)。索引必须 fit int32(20M×3≈30M < 2^31 OK)。
- **落盘**:**自建流式 writer**(不复用 `BlobStore::put`,因其全量双份内存峰值)——`std::ofstream` 分块写 `.bin` + `Sha256::update` 增量哈希,产出与 `BlobStore` 逐字节一致的内容寻址 blob(复刻 encode 的 LE 字节序 + 角色名约定 `points/tris/faceId/volPoints/tets`,components 严格 3/3/1/3/4)。手填 `BufferRef`。
- **拓扑形状**:不必是真血管,但要闭合/合法索引(三角索引 in-range、tet 四点不共面可不强求,探针只压字节与接口)。用规则网格(如细分网格管壁)生成可控规模。

### 3.2 真 MappedVoxelSource(R2,实现 IVoxelSource)
- `CreateFileW(GENERIC_READ, OPEN_EXISTING)` → `CreateFileMapping(PAGE_READONLY)` → `MapViewOfFile(FILE_MAP_READ)` 拿只读基址。
- RAII 包 {hFile, hMap, base, size} → 自定义 deleter(`UnmapViewOfFile`+`CloseHandle`×2)→ `shared_ptr<const void>` keepalive。
- `acquire_whole`:`view.bytes = ReadSpan<uint8_t>(base, size)` + 维度 meta → `VoxelLease::borrow(keepalive, view)`。**零拷贝。**
- `acquire_region/slab`:勘察问题——子块 x-fastest 跨行非连续,**探针对比两种实现**:(a) own 物化拷贝(同 Resident);(b) borrow 整卷 + view 暴露 stride(需接口支持,探针记录"接口缺 strided-view"为形状缺陷)。
- **完整性策略**:见 §4。
- 边界:0 字节文件 `MapViewOfFile` 失败需特判;文件尾非整页;deleter 失败无返回通道(记录为缺陷)。

### 3.3 真 MappedGeometrySource(实现 IGeometrySource)
- 同 mmap 底座,`acquire_points` → `ReadSpan<Point3>(reinterpret_cast<const Point3*>(base), n)`(**零拷贝 AoS reinterpret,打穿缺陷④**);`acquire_tetrahedra` 同理 `<SourceTet>`。
- `acquire_triangles`:tris 可零拷贝 borrow,但 faceId 走 M8b-1 物化路 → **实测 20M 三角每次 acquire 80MB 物化代价**(打穿缺陷,数据回灌"faceId 是否该给 handle/接口加 vector 访问器")。

### 3.4 并发 evict harness(R3)
后台线程在前台持有 lease(borrow view)期间尝试 unmap。两个实验:
- **A 优雅 evict**:evictor 只在 `use_count==1`(无 live lease)才 unmap → 验证 shared_ptr 寿命是否足够做 gating(预期✅,记录)。
- **B 强制 evict**:evictor 无视 use_count 主动 unmap → 制造 borrow view 悬空/访问违例 → 证明"接口缺并发 pin/读锁",回灌 lease 并发签名建议(缺陷③核心)。

### 3.5 基线测量(AC3)
`std::chrono::steady_clock` 夹三段:open-time(map+verify)、acquire、VTK 上传(`addSurface`/`addVolumeMesh`)。内存峰值 `GetProcessMemoryInfo`。对比 **Resident 全驻留 vs mmap 借用** 两组数字。

## 4. 完整性策略候选对比(R5,探针实测两条)

| 策略 | 做法 | 保留惰性? | 代价 | 复用 |
|---|---|---|---|---|
| **A 验证一次后信任映射** | map 后首次访问前对全 blob 跑一次 `Sha256::hashHex` | ❌(全量触碰,抵消 mmap) | open-time O(N) | 直接 `Sha256` |
| **B 分段 Merkle** | blob 切固定块(如 1MB),存段哈希表 + root;只对 `acquire` 实际触碰的块按需算段哈希比对 | ✅(只校验触碰段) | 写期建表 + 读期按段 O(触碰量) | `Sha256::update` + picosha2 `process(裸指针区间)` 算任意子段,无新依赖(`Sha256.h:17-39`、`picosha2.h:182`) |

探针对两条都测 open-time + 内存 + 是否真惰性(用 `GetProcessMemoryInfo` working set 看是否只涨触碰量),给 M8b-2 §冲突决策数据。段哈希表存储位置(`.merkle` 边车 vs 扩 `BufferRef` schema)是 M8b-2 决策,探针先用边车文件验证可行性。

## 5. 接口打穿报告(R4,探针最终交付物)

`probe/REPORT.md`(或 journal),针对四类缺陷各给:**当前 M8b-1 接口能否承载 / 需要怎么改签名**,含:
1. **lease 并发语义**:harness B 结论 → `ReadLease` 是否需引用计数读锁/pin/try-evict 钩子,给候选签名。
2. **acquire 粒度与失败模式**:region/slab 零拷贝是否可能、strided-view 缺口、完整性策略如何反向决定 acquire 粒度(all-or-nothing vs 段)。
3. **完整性策略**:A vs B 实测数据 + 推荐。
4. **AoS vs SoA**:Point3 24B reinterpret 零拷贝实测(成立)+ VTK 上传耗时 → 是否需 SoA(给数据,不下结论由冻结门定)。

## 6. 严格不做(R6 + Out of Scope)

- 不改 `src/core/source/` 任何接口实现、不改任何现有消费者/测试。
- 不冻结任何接口(只产"该怎么冻"结论)。
- 不做 M8b-2 生产级 GeometryResourceManager/内存预算/真去 vector 化;不做 M9b LOD/分块/渐进上传;不迁移真实消费者。
- 默认 CMake OFF,Release 全量 ctest 仍 50/50 绿(AC6)。

## 7. 验收映射

AC1=生成器产 20M 三角+10M tet blob(SHA 校验)。AC2=Mapped*Source mmap 后经接口吐 span 逐元素 == 源。AC3=open-time/内存峰值/VTK 上传基线数字。AC4=harness A/B 结论(lease 并发是否安全 + 需要的签名)。AC5=R4 报告 + R5 对比。AC6=主线零污染,ctest 绿。
