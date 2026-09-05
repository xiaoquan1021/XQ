# XQ 架构演进路线图(ROADMAP)

> 父任务 `06-29-xq-rebuild` 的里程碑总纲。M0~M7 主线已完成(端到端 0007 跑通),本路线图记录
> **从首版前端 → 大规模可视化前处理 + 可复用函数库(libXQ)** 的架构演进主轴。
> 由 2026-06-29 brainstorm 敲定。每条里程碑落地为独立 trellis 子任务。

## 主轴原则(不可动摇的顺序)

> **先让资产身份和存档正确 → 再让加载与释放正确 → 然后让渲染器依赖正确 → 最后做真正的大规模可视化。**

- 架构演进是**主轴**,严格串行 M8a → M8b-1 → M8b-2 → M9a → M9b。
- **接口先行、驻留后置**:M8b-1 只改"消费者依赖什么"(接口 + 兼容适配器,数据驻留行为不变);
  M8b-2 才改"数据由谁持有、何时加载/释放"。
- **UI 可全程并行**,但有硬约束(见末节):新增 UI **不得加深对旧 payload vector / 具体 GeometryHandle 的耦合**。
- 依赖方向不变:`app → services → adapters → io → core`;库优先 / 无治理层 / 外部库当 kernel。

> **2026-06-30 准则修订(决策:成本/返工不计,执行可走错路,但最终架构方向不能偏):**
> 原"接口先行/驻留后置"的理由是"降返工"——既然成本被排除,该理由失效,且它带来**方向性风险**:
> M8b-1 在"小规模 + resident vector"世界里定义 `IGeometrySource`/`ReadLease`,M9a 把消费者全迁过去,
> 等 M9b 上 2000万规模才发现接口形状根本错了(lease 该并发读锁 / acquire 该分块 / 完整性验证强制
> all-or-nothing / AoS `Point3` 该 SoA)——届时消费者已迁在错接口上,**方向已偏**。
> **新准则:先证后冻 + 拿最难场景定型。** 衡量标准只有一个——让最终态(M9b 之后)正确的概率最大化,
> 哪怕中途推倒重来。三条硬规则:
> 1. **真负载 + 真最难存储探针前移到"冻接口"之前**:冻 Source/libXQ 候选前,先造 ① 20M 三角 / 10M tet
>    合成负载,② **真的** `MappedVoxelSource`(mmap + verify + 并发 evict harness),把它们喂过 M8b-1 接口,
>    在最难场景下"打穿"接口(lease 并发语义 / acquire 粒度 / 完整性 vs 懒映射 / AoS-SoA / faceId 物化代价),
>    打穿什么就回灌进接口形状,**然后才冻**。这是丢弃式 spike,成本不计正好能干。
> 2. **M8b-1 维持原样继续**:它是纯增量、解耦渲染器、candidate-not-frozen,实现方向对——留着,但它只是
>    "最简特例",**不是接口的设计依据**;不与 M8b-2 合并(纯增量解耦值得独立、无漂移)。
> 3. **冻结门 = 探针存活**:libXQ / Source 1.0 只在"真规模 + 真 mmap + 并发 evict 喂过接口且接口活下来
>    (或被改到活下来)"之后才冻,**且必须在 M9a 迁移消费者之前**,不拖到 M9b。
>
> **✅ scale-probe 探针已完成(2026-06-30,commit cabe074 = `06-30-scale-probe`,代码可弃)。**
> 真规模(20M 三角/10M tet/512³ 体素)+ 真 Win32 mmap 喂过 M8b-1 接口,四类缺陷打穿结论(详见 `XQ/probe/REPORT.md`):
> - **可冻**(形状验证正确):`ReadSpan` 零拷贝契约、`acquire_*` 取块签名、AoS `Point3` reinterpret(AC10)、越界 invalid-lease、`IVoxelSource`/`IGeometrySource` 虚函数集。
> - **冻前必改**(打穿):`ReadLease` 并发语义——harness 实测「强制 unmap → borrow view 悬空 AV」,单线程 move-only RAII 挡不住非 refcount 驱逐;须升级为并发读锁/pin(协作式 use_count gating 实测安全)。
> - **AoS 不必改**:reinterpret 零拷贝成立,大规模瓶颈在 VTK 逐元素上传(addSurface 10M 点 23.5s)而非布局,与 Source 形状无关 → 不进冻结门(归 M9a 上传核重写 / M9b)。

## 已建成基线(M0~M7,已归档)

- core 零依赖数据模型 + scene + 命令栈 + 原生存档(结构级)。
- 主线 services:Path / Segmentation / Modeling / Meshing(表面+体网格)/ Flow(BC + FlowSolver1D)/ AI。
- 体网格生产 kernel:TetGen(PLC)+ MMG(两段式),可选 adapter 默认 OFF。
- **首版前端(M7)**:`XQMainWindow`(树 + MPR + 交互 3D + 六阶段面板 + undo/redo)、
  `XQSceneRenderer`(VTK pimpl,头零 VTK,已能渲染影像/path/surface/体网格/flow/segMask)、
  `XQImageViewer`(影像 MPR)、六个薄控制器。**整条工作流 0007 端到端可跑。**
- **现状耦合点(待解)**:`XQSceneRenderer` 直接吃 `XQTriangleSurfaceGeometryHandle&` /
  `XQTetVolumeMeshHandle&`(整块几何常驻)→ 大规模渲染的瓶颈,M9a 迁移到 Source。

## 里程碑

### M8a — 资产身份 + 存档持久化(= `06-28-payload-persistence`,✅ 已完成)

完成:`AssetId`、最小 `AssetRegistry`、资产血缘、Node↔assetId 绑定、版本化主档(schema 1.2)、
规范化二进制 sidecar(`<stem>.assets/blobs/<2>/<sha256>.bin`,显式 endian/type/count/SHA-256)、
External / Derived 资产、实体级 round-trip。
**运行时 payload 与全量 vector 暂不变。** 详见该子任务 `prd.md` / `design.md`。

**验收**(2026-06-30):S1~S4 串行全绿;Release 全量 ctest 49/49;假绿抽查通过;io 零 VTK;
旧 1.1 档向后兼容;AC1~AC10 全部满足。

### M8b-1 — Source 接口定义(消费者依赖什么,✅ 已完成 = `06-30-source-interface`)

落地 `IVoxelSource`、`IGeometrySource`、`ReadLease` 及**批量范围访问**接口
(`acquire_region`/`acquire_slab`/`visit_slabs`/`acquire_points`/`acquire_triangles`/`acquire_tetrahedra`/
`try_acquire_contiguous`;**不要逐元素 point(i)/tet(i) 虚调用**)。
先提供基于现有 vector 的 **Memory/Resident 实现** + **Handle 兼容适配器**。
**本阶段只解决"消费者依赖什么",不改变数据驻留行为。**

> **2026-06-30 评审修订(R1/R2,M8b-1 落地必满足):**
> - **R1 faceId 是一等通道**:`acquire_triangles` 必须随带逐三角 faceId(并行通道或批次字段)。三处活消费者强依赖:渲染器着色(`XQSceneRenderer.cpp:155`)、`SurfaceMeshService.cpp:62` 边界面归并、`TetGenTetMesher.cpp:117` facetmarker。上面接口清单原文漏了它。验收:这三处零改语义跑通。
> - **R2 点驻留契约写死**:几何消费者普遍按"全局顶点索引"随机回查点数组(`tets.point(cell[k])` 等),故索引型几何 Source **点必须全常驻**——`acquire_points` 只承诺全范围连续块(配 `try_acquire_contiguous`);`acquire_region/slab/visit_slabs` **限定体素侧(`IVoxelSource`)**,几何点的分块/LOD 延到 M9b 随算法一起做。
> - **R3 不冻结 lease 失败语义**:M8b-1 交付适配器 + 头文件级契约,标 **candidate-not-frozen**;`ReadLease` 的失败/取消/部分获取分类**等真实后备存储(M8b-2 mmap)存在后再定**——现在冻会冻错(见 M8b-2 修订 §冲突)。

**完成(2026-06-30,commit 46a939d,candidate-not-frozen):**
- 落地 `core/source/`:`ReadSpan<T>`(C++17 自定义只读连续视图)、`IVoxelSource`(`meta/acquire_whole/region/slab`)、`IGeometrySource`(`meta/acquire_points/triangles/tetrahedra`)、`ReadLease`(`VoxelLease`/`GeometryLease<T>`/`TriangleLease`,move-only)、`ResidentVoxel/Surface/TetSource` 三个 Handle 适配器。
- **定调**:每块一次虚调用,返回整块连续只读视图;视图内全 inline 非虚;逐元素虚调用零容忍。借用(零拷贝)vs 物化(faceId/region/slab)二分;`shared_ptr` keepalive 让租约活过 adapter。
- **R1 ✅**:`acquire_triangles` 返回 `TriangleView{triangles + 等长并行 faceIds}`。**R2 ✅**:region/slab 仅 `IVoxelSource`;`acquire_points` 只给全范围连续块。**R3 ✅**:未冻结 lease 失败/取消/部分获取语义。
- **接口名收敛**:原清单的 `visit_slabs`/`try_acquire_contiguous` 未单列为方法——前者由 `acquire_slab(z)` 覆盖,后者由 `GeometryLease::span().data()` 连续指针(可直喂 TetGen `REAL[3n]`)达成,M9a 迁移消费者时用这两者即可。
- **理解依据**:5 子系统消费者依赖矩阵(消费侧 100% 只读,核心诉求整块拿到),证据见 `06-30-source-interface/design.md`。spec → `.trellis/spec/XQ/core/source-interface.md`。
- **验收**:Release 全量 ctest 50/50(原 49 零回归);假绿抽查 region 偏移 / faceId 篡改各触发对应 AC FAIL,exe 时间戳真变;对抗审查 11 条 AC 全 PASS;纯增量(仅动 CMakeLists.txt)。
- **遗留(非阻断,留 M8b-2 接真实存储时补)**:AC6 no-alloc 未独立断言、多分量(components>1)体素路径未测。

### M8b-2 — 运行时内存模型(数据由谁持有 / 何时加载释放,✅ 机制收口 = `06-30-m8b-2-lease-mmap`)

实现 `GeometryResourceManager`、按需整块驻留、内存预算、pin/lease 生命周期、
**SceneNode 去大型 vector 化**、`MappedVoxelSource`(磁盘后备只读 mmap + 规范化体素缓存)。
**本阶段解决"数据由谁持有、何时加载、何时释放"。**

> **✅ 机制收口完成(2026-06-30,3 commit:788ecb4 io 底座 / 63819a6 manager+lease / f81b1fa 收尾)。**
> 全栈机制就绪、53/53 ctest 绿(新增 test_merkle_sidecar / test_mapped_voxel_source / GeometryResourceManagerTest)。本里程碑落地:
> - **§冲突取舍已定 = 分段 Merkle(R4 最高优先项已结)**:`MerkleSidecar`(`.merkle` 边车 + 单层 root,schema 1.3)落地,`MappedVoxelSource` 走 **SegmentedMerkle 惰性逐段 verify + memoize**(只验触碰段,128× 惰性差距兑现);FullVerify 路径并存但 anchor(`BufferRef.sha256` 比对)留 M9a。
> - **新零拷贝 read 路径已建**:`MmapBlob`(Win32 CreateFileW/MapViewOfFile 只读 RAII)+ `MappedVoxelSource::acquire_whole` reinterpret 零拷贝 borrow(keepalive=mmap),不复用 `BlobStore::get` 的逐元素解码。
> - **lease = 对抗并发驱逐的读锁(冻结候选)**:`GeometryResourceManager` 每块自管 pin(`shared_ptr<int>`),`evictToBudgetLocked` 仅在 `use_count()==1` 回收(pin 住跳过),acquire/evict 共享一把锁防 TOCTOU;scale-probe 已证 5× 并发稳定、协作式 gating 安全(强制 unmap=悬垂 AV)。
> - **分层守恒**:`MmapBlob`/`MerkleSidecar`/`MappedVoxelSource` 进 `xq_io`,`GeometryResourceManager` 进 services 并 `PRIVATE xq_io`,core 零外部依赖不变。
> - **范围收窄(已确认)**:真实体素/几何**链接入** manager(SceneNode 去 vector 化、image 体素链路)移交 **M9a**——image source 的体素从不经 M8a blob,prd 原 AC6/AC7 的"open-time 物化"是伪前提,机制层不接真实数据源,只验机制全栈就绪。

> **2026-06-30 评审修订(R4,M8b-2 必须新增的硬范围):**
> - **§冲突 完整性 vs 流式(最高优先,5 人漏、红队抓到)**:M8a 已落地的 blob 身份 = **整文件 SHA-256、每次 read 全量重算**(`io/blob/BlobStore.h:12-20,40`),与 M9b 的 LOD/分块/懒映射**结构性冲突**(全量验证抹掉懒加载,不验证抹掉内容寻址)。**取舍必须在本里程碑显式决策**(验证一次后信任映射 / 还是分段 Merkle),别拖到 M9b 才发现无解。
>   - **scale-probe 已给数据(推荐分段 Merkle)**:large(128MiB blob)下 acquire 单 slab,策略 A 全量验证 hash 128MiB/805ms,策略 B 分段 Merkle 只 hash 触碰段 1MiB/5.8ms = **128× 惰性差距**;B 用 `Sha256::update`+picosha2 对 mmap 子段算哈希、无新依赖。段哈希表存储位置(`.merkle` 边车 vs 扩 `BufferRef` schema)是本里程碑待定决策点,探针已证边车可行。
> - **新 read 路径**:`BlobStore::get` 当前返回 `std::vector` 且**逐元素 LE 解码、非 memcpy**(`BlobStore.h:23,63-65`)。`MappedVoxelSource` 无法复用 `get()`,需新增 "map+verify+reinterpret-span" 零拷贝读路径——这是没排进任何里程碑的临界路径工作。reader 端拷贝(盘→vector→handle)才是 open-time 真墙(渲染端拷贝之外)。
> - **lease 是并发原语**:后台 load/evict + M9b 渐进上传 → 后台线程释放 blob 时渲染线程持有视图。`ReadLease` 必须是**对抗并发驱逐的读锁(引用计数、线程安全、可移动)**,不是单线程 RAII。这会回头影响 M8b-1 签名,故 M8b-1 才不冻结 lease 语义。
> - **分层(唯一无坑维度)**:`MappedVoxelSource` 放 **`xq_io`**(已有 BlobStore→core 依赖倒置范式,`BlobStore.h:4`);`GeometryResourceManager` 放 **services**,**绝不进 core**;Source 契约锁死裸 typed span,永不出现 `vtk*` → io 永不需要 VTK。
> - **次要**:连通性索引是 `int`/`I32`(`XQTetVolumeMeshHandle.h:23`、`BufferRef.h:17`),10M 点在 2.1B 上限内安全,但 soup/非共享几何是潜在墙,留一条 caveat。

### M9a — 渲染器/网格服务迁移到 Source(✅ 已完成 = `06-30-m9a-source-migration`)

将 renderer、solver 及相关 services 从直接收 `XQTriangleSurfaceGeometryHandle` /
`XQTetVolumeMeshHandle`,迁移为**通过 Source + 带生命周期的视图**取数据。
**先保持现有渲染能力不退化**(等价替换,不加新特性)。

> **2026-06-30 评审修订(R5,M9a 纠偏):**
> - **删除"solver 迁移 Source"——这是幽灵工作项**:`FlowSolver1D` 吃标量 `SolverInput`,`addFlowResult`(`XQSceneRenderer.cpp:411-469`)不带几何,**flow 全链路不碰几何 handle**(`BoundaryConditionService.cpp:74` 只用 `boundaryFaceById`)。上面"solver 迁移"原文是伪命题,会让"完成"标准失真,予以删除。M9a 客户 = 渲染器 + 网格服务/适配器两家。
> - **"等价替换"措辞要改成"重写上传核"**:逐 tet `vtkNew<vtkTetra>`(`XQSceneRenderer.cpp:183`,10M 次分配)、逐元素 `SetPoint`/`InsertNextCell` 是先于内存爆掉的分配风暴。原文"等价替换不退化"会原样保留这个风暴——M9a 必须重写上传路径(批量 SoA → VTK/GPU),不是逐元素等价搬运。

> **✅ 完成(2026-06-30,3 commit:阶段1 renderer 上传核 / 阶段2 ITetMesher+TetGen / 阶段3 网格服务+io writer)。** 53/53 + kernel(TetGen+MMG)55/55 ctest 绿;假绿抽查(篡改 TetGen 点 span → test_tetgen/mmg 双红)证 acquire 数据真流过。
> - **renderer**:`build_surface`/`build_volume` 改吃 `const IGeometrySource&`,VTK 批量上传(`vtkDoubleArray`+memcpy / `vtkCellArray::SetData` / `SetCells(VTK_TETRA)`),消除逐点 `SetPoint`/逐三角 `InsertNextCell`/per-tet `vtkNew<vtkTetra>` 风暴。**copy-on-upload 不零拷贝借用**:管线 filter 惰性执行 + renderer 长持 actor 无处存 lease,借用必悬垂(memory `renderer-source-copy-on-upload-not-borrow`)。
> - **网格服务/适配器**:`ITetMesher::tetrahedralize` 改吃 `const IGeometrySource&`;TetGenTetMesher pointlist 走 `acquire_points().span()` memcpy 喂 `REAL[3n]`、facetmarker 走 faceId 并行 span;VolumeMesh/SurfaceMesh/Modeling 逐元素循环改 span;TetGenThenMmg 转发。
> - **io writer**:`XQProjectWriter` flatten 改 Source span + 整块 memcpy,round-trip 字节守恒(content-addressed SHA 不变)。
> - **出范围(查实)**:flow 不碰几何(伪工作项已删);MMG 适配器内核逐元素、吃产出 handle 非 project Source;`path/` 只用 Point3 数学——三者不迁。
> - **接口零改动**:Source 公共签名全程未动(static_assert 成立);faceId 借用变体留待真大规模成热点再加。**仍缺(M9b)**:生产 `MappedGeometrySource` + 几何资源管理器工厂(`acquireGeometrySource`)。

### M9b — 真正的大规模可视化前处理(🚧 规划完成,拆父+4子任务 = `06-30-m9b-large-scale-viz`)

在 Source + ResourceManager 之上实现:LOD、降采样、分块、渐进式 GPU 上传、裁剪、
显存预算、大网格拾取加速。这是"大规模可视化前处理"的最终交付。

> **🚧 规划完成(2026-06-30,5 路并行调研 + 对抗式综合 + 4 路并行起草;调研存档 `research-synthesis.json`)。** 拆**父任务 + 4 子任务**,本里程碑范围 = P0+P1:
> - **两条独立瓶颈线分账**(验收指标分开):线1 GPU 上传峰值(large 20M 三角 23.5s/3786MiB,真因 = `vtkPolyDataNormals` deep copy + 整块上传,**非**逐元素风暴——那已 M9a 消除,旧 REPORT.md:47 归因作废)→ 靠 LOD/降采样/渐进上传;线2 reader 常驻 + 瞬时双份 → 靠几何 mmap 懒驻留 + payload 去 vector 化。
> - **A(P0 基座)** 生产 `MappedGeometrySource`(io)+ `acquireGeometrySource` 工厂(services,ResidentBlock 泛化承载 voxel|geometry)+ `TriangleLease` borrow-faceId 零拷贝变体(core,第二 keepalive + 新静态工厂,原 borrow 不改)。默认 FullVerify(writer 不产几何 .merkle)。
> - **B(P0)** LOD/降采样:`vtkLODActor`(交互)+ 固定级直挂(headless 可测,因 vtkLODActor 无 interactor 不可测)双路径;decimation 离线/异步;分 faceId region decimate 保 faceId(QuadricDecimation 不传 cell data);含 CMake `VTK::RenderingLOD` 前置。
> - **C(P1,依赖 B)** 渐进/分块上传:多 actor 边建边显(VTK 无真增量,不在单 vtkPolyData append),全局 LUT range 防跨块色漂,still/interactive 切级。
> - **E(P1,依赖 A)** payload 去 vector 化并存惰性路径:payload 加可选 assetId 经 manager 惰性取源,与 resident handle 并存(不破 clone、向后兼容)。**分层硬约束**:`xq_services` PRIVATE 链 io,reader(io)不能调 manager(services)→ 惰性解析落 services,reader 只记 assetId。
> - **父级硬契约**:cell→faceId 改用**显式映射表**(LOD/分块改顺序/抽稀后下标相等失效);RenderStats.pointCount 报源点数(headless 可断言),上传量另加字段;不破已冻 Source 1.0 公共签名(全"加不破")。
> - **出范围(后续)**:D 大网格拾取(CPU vtkStaticCellLocator 起步,GPU vtkHardwareSelector headless 不可测)、裁剪/视锥剔除(无 UI 牵引)、几何 sidecar 写路径 + SegmentedMerkle、payload handle vector 物理删除。

> **2026-06-30 评审修订(R6)+ 准则修订对齐(M9b 启动门控):**
> - **真负载不再等到 M9b——已前移到冻结门之前(见主轴准则修订第 1 条)**。M9b 启动时,真规模合成负载(20M 三角/10M tet)+ 真 `MappedVoxelSource` 探针**应已存在**(冻结门要求);M9b 是在它们之上做 LOD/分块/渐进上传的真实交付,而非到这里才第一次造大负载。§冲突(完整性 vs 流式)与 lease-vs-evict 并发模型**必须已在 M8b-2 + 冻结门探针中定好**,M9b 只消费结论、不在这里现发现。原文遗留的瓶颈点(AoS `Point3` 24 字节/点、VTK 上传、reader 拷贝)即探针要打穿的对象。

## UI 并行工作(M8a~M9 全程可做)

允许并行改进:**MPR 三视图联动、Scene 交互、面板易用性、任务进度、取消、错误提示、现有规模拾取**。

**硬约束**:
- 新增 UI **不得**直接依赖 payload vector 或具体 `GeometryHandle`。
- UI 只通过 **application commands + `NodeId` + `AssetId`** 操作应用状态。
- 大规模渲染层(renderer 吃什么)**必须等 M8b-1/M9a**,不在未冻结接口上写渲染消费者。

## libXQ(可复用函数库)对外 API 排期

| 阶段 | libXQ 动作 |
|---|---|
| M8a 后 | 只整理**内部模块边界**,**不冻结**公共 API |
| M8b-1 后 | 形成 **Asset / Source API 候选**,由 XQ 主程序**内部先行使用**(自用验证),仍 **candidate-not-frozen** |
| **✅ libXQ / Source 1.0 已冻(2026-06-30,M9a 消费者存活证据齐)** | 冻结门已通过(scale-probe ✅ cabe074 真规模+真 mmap+并发 evict 喂过接口存活;M8b-2 ✅ 788ecb4/63819a6/f81b1fa lease=对抗并发驱逐读锁定稿)。**M9a ✅ 补齐消费者存活证据**:renderer/网格服务/适配器/persistence 全部经 `IGeometrySource`/`ReadLease` 边界工作且**不退化**(53/53 + kernel 55/55,round-trip 字节守恒,Source 公共签名零改动 static_assert 成立)→ **Source 接口签名 + eviction-gating lease 契约正式冻为 libXQ/Source 1.0**。后续改动按"加不删、签名不破"演进;真大规模 faceId 借用变体 / mmap 几何源 / 几何工厂留 M9b 增量,不破已冻签名。 |

> 铁律:**内存所有权与 Source 抽象在「真规模 + 真最难存储」探针下验证前,不提前承诺稳定公共接口。**
> (原"M8b-2 + M9a 验证后再冻"已被上面冻结门取代:验证标的从 resident vector 升级为真规模 + 真 mmap,
> 且前移到 M9a 迁移之前——避免把消费者迁在未经真场景验证的接口上。)

## 与 brainstorm 决策的对应(可追溯)

- 资产模型四类(External/Managed/Derived/Cache)、assetId 独立稳定、血缘落 Asset 层 → M8a。
- IVoxelSource/IGeometrySource + ReadLease + 批量范围视图 → M8b-1。
- 按需整块驻留 + GeometryResourceManager + 去 vector 化 + mmap → M8b-2。
- 渲染器/求解器迁移 Source、目标量级(单 Surface 2000万三角 / 单 Tet Mesh 1000万 tet / 单活动资产)→ M9a/M9b。
