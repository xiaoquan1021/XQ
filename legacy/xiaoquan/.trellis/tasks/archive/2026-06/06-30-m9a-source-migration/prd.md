# M9a 渲染器/网格服务迁移到 Source(不退化 + 重写上传核)

## Goal

把 **renderer + 网格服务/适配器两家消费者**从"直接收 `XQTriangleSurfaceGeometryHandle` / `XQTetVolumeMeshHandle` 并逐元素读"迁移为"经 `IGeometrySource` + 带生命周期的 lease 视图取数据",并**重写 renderer 的 VTK 上传核**(消除逐元素 `SetPoint` / `InsertNextCell` / per-tet `vtkNew<vtkTetra>` 分配风暴,改为批量 SoA → VTK 9.3 `SetArray`/`SetData`/`SetCells`)。

**先保持现有渲染/网格能力不退化**(等价替换,不加新特性),同时把探针实测的瓶颈(10M 点 surface 上传 23.5s / 3786 MiB,`probe/REPORT.md`)在 renderer 侧打掉。

几何数据后端走**驻留源**:消费者面向 `const IGeometrySource&` 编程,调用点用 `ResidentSurfaceSource` / `ResidentTetSource` 包现有 handle(数据仍驻留、零拷贝借用)。生产级 mmap 几何源 + 几何资源管理器工厂(assetId→source/预算/驱逐)是已知缺口,**留 M9b**(真大负载 / LOD 时才需)。

完成本里程碑后,Source 接口签名 + eviction-gating lease 契约已被真实消费者(渲染 + 网格 + 持久化)在 resident 路径下喂过且不退化,**libXQ / Source 1.0 候选可正式冻结**(冻结门已于 M8b-2 通过,本里程碑提供消费者侧存活证据)。

## Background(调研已备齐的迁移依据)

三路并行调研(本会话)结论,本里程碑直接采纳:

- **Source 层就绪度**:`IVoxelSource`/`IGeometrySource` + `ResidentSurfaceSource`/`ResidentTetSource`/`ResidentVoxelSource` 已落地。零拷贝连续指针成立:`acquire_points().span().data()` = `const Point3*` = `double[3n]`;`acquire_tetrahedra().span()` = `SourceTet`(`array<int,4>`) = `int[4n]`;`acquire_triangles().view().triangles` = `SourceTriangle`(`array<int,3>`) = `int[3n]`,`.faceIds` 是等长并行 `ReadSpan<int>`。单次 acquire 一次虚调用,视图内访问非虚 inline。
- **renderer 是验证过的瓶颈 + 上传核重写面**:`XQSceneRenderer.cpp` 的 `build_surface`(:134-163)逐点 `SetPoint` + 逐三角 `InsertNextCell` + 逐三角 `triangleFaceId(i)`;`build_volume`(:165-192)**每 tet 一个 `vtkNew<vtkTetra>` 堆分配**(:183)+ 逐 cell `InsertNextCell`。VTK 9.3 已确认,`vtkDoubleArray::SetArray`+`vtkPoints::SetData` / `vtkCellArray::SetData(offsets,connectivity)` / `vtkUnstructuredGrid::SetCells(VTK_TETRA,…)` 全可用,**当前一个都没用**。`addFlowResult`(:411-469)无几何 → **出范围**。
- **网格服务/适配器**:`ITetMesher`(`ITetMesher.h:54`)是注入缝,硬依赖 `const XQTriangleSurfaceGeometryHandle&`。`TetGenTetMesher`(:93-118)是连续 span 的**首要受益者**:逐元素 copy 进 `new REAL[3n]` / facets / `facetmarkerlist`。`VolumeMeshService`(:41-226)、`SurfaceMeshService`(:37-81)、`ModelingService`(:49-191)、io `XQProjectWriter`(:720-828)各有逐元素读/flatten 循环。
- **明确出范围**:flow 全链路不碰几何(`FlowSolver1D` 吃标量、`addFlowResult` 无几何)→ 删幽灵工作项;MMG 适配器(`MmgVolumeRemesher` 内核逐元素 set,无 bulk 收益、且吃上一阶段产出 handle 非 project Source)→ 不迁;`path/`(只用 `Point3` 数学,不碰 handle)→ 不迁。

## Scope

### In scope
- **renderer 迁移 + 上传核重写**(visualization):`addSurface`/`addVolumeMesh` 改为收 `const IGeometrySource&`(或在入口包 Resident*Source);`build_surface`/`build_volume` 重写为批量上传——点用 `vtkDoubleArray::SetArray` 包 `Point3*` 的 `double[3n]` + `vtkPoints::SetData`;surface cell 用 `vtkCellArray::SetData(offsets,connectivity)`(int32 连接 + 生成 `i*3` offsets);tet 用同法 + `SetCells(VTK_TETRA,…)` 消除 per-tet `vtkTetra`;faceId 用 `vtkFloatArray` 批量。
- **网格服务/适配器迁移**(services/adapters):`ITetMesher` 注入缝重塑为接受 `const IGeometrySource&`(或 lease),`TetGenTetMesher` 改走连续 span 喂 TetGen(`reinterpret_cast<const REAL*>(points.data())`,加 `static_assert(sizeof(Point3)==24)`);faceId/facetmarker 走 `TriangleView::faceIds` 连续 span。`VolumeMeshService`/`SurfaceMeshService`/`ModelingService` 输入侧从 handle 改为经 Source 取连续 span(逐元素循环改 span 访问)。
- **io writer flatten 迁移**(io/project):`XQProjectWriter` 的 points/triangles/tets/faceId flatten 循环改为经 `acquire_*().span()` 批量取 + bulk 拷贝;faceId 直接用并行 span(不再逐元素 `triangleFaceId(i)`)。
- **lease 生命周期落定**:renderer 零拷贝 `SetArray(save=1)` 借用指针必须活到 VTK 不再读(mapper 持 polydata/grid 期间);本里程碑确定"借用 lease 随 actor 生命周期保持 vs 上传后拷贝"取舍并实现(resident 路径下二者都可,选其一并写进 design)。
- **零回归验证**:现有渲染/网格输出在迁移前后等价(像素/网格快照或元素级对比);现有 Release 全量 ctest 仍绿(53/53 起步,新增本里程碑迁移测试)。

### Out of scope(明确划归 M9b/后续)
- **生产级 `MappedGeometrySource`(磁盘 mmap + 完整性)→ M9b**:当前只有弃码探针 `XQ/probe/MappedGeometrySource.h`,生产几何 mmap 源不在本里程碑建。
- **几何资源管理器工厂(`acquireGeometrySource`:assetId→source / 预算 / pin / 驱逐)→ M9b**:体素侧已有 `acquireVoxelSource`,几何侧等价物是缺口,真大负载/驻留管理时才需。
- **LOD / 分块 / 渐进上传 → M9b**:本里程碑只做等价批量上传,不做分级。
- **`TriangleLease` faceId 借用变体(borrow-faceId)→ 按需**:当前 `ResidentSurfaceSource::acquire_triangles` 每次 O(N) 物化 faceId vector;若迁移后 faceId 成热点再加 borrow 变体(20M 三角才显著,resident 路径可接受)。
- **MMG 适配器 / `path/` / flow**:不迁(见 Background)。
- **SceneNode 去大型 vector 化 / image 体素链接入 manager**:若不阻塞 renderer 不退化则留 M9b;本里程碑只迁消费者读路径,不改 payload 持有结构(除非 design 判定必须)。
- 不复用 `XQ/probe/` 弃码。

## Constraints
- **分层**(ROADMAP 锁死):renderer/services/adapters 消费 `IGeometrySource*` 抽象;Source 契约锁死裸 typed span,**io/core 永不出现 `vtk*`**;VTK 只在 visualization/adapters 私有实现。依赖方向不变(app→services→adapters→io→core)。
- **接口零退化**:M8b-1 `IGeometrySource`/`IVoxelSource`/`ReadLease` 公共签名**不改**(static_assert 仍成立);本里程碑是消费者迁移,不是接口改版。若迁移中发现接口缺口(如 faceId 借用),先记录、能用现有签名达成就不动签名。
- **渲染不退化**:迁移前后渲染输出等价(`vtkPolyDataNormals`/`vtkGeometryFilter`/`vtkExtractEdges` 管线、faceId 着色、空几何 guard `pointCount==0||triangleCount==0` → 映射到 `meta()` count / `span().empty()`)。上传核重写是性能改进,不得改变可见结果。
- **lease 生命周期安全**:零拷贝借用的 VTK 数组指针不得在 actor/mapper 仍引用时失效(resident 路径下 keepalive=底层 vector 的 shared_ptr,天然安全;但 `SetArray(save=1)` 的借用语义要显式处理)。
- **最小改动**:逐元素循环改批量 span 是等价替换,不加没要求的兜底/降级分支(CLAUDE.md 铁律);TetGen 仍需自己的 facet 对象(逐 facet 构造保留),只把 pointlist + facetmarker 改批量。
- 验收按铁律:Release + 全量 ctest + 假绿抽查;副作用调用(如 `acquire_*`/`read`)不进 assert(`/DNDEBUG` 删掉=假绿 segfault)。

## Acceptance Criteria
- [x] **AC1 renderer 经 Source 取数不退化**:`addSurface`/`addVolumeMesh` 经 `IGeometrySource`(Resident*Source 包 handle)取 points/triangles/tets/faceId,渲染输出(像素或网格元素)与迁移前等价;空几何 guard 经 `meta()`/`span().empty()` 正确短路。
- [x] **AC2 surface 上传核批量化**:`build_surface` 改用 `vtkDoubleArray::SetArray`(Point3 span 零拷贝)+ `vtkPoints::SetData` + `vtkCellArray::SetData(offsets,connectivity)` + `vtkFloatArray` faceId;**无逐点 `SetPoint`、无逐三角 `InsertNextCell`**(代码层可核)。
- [x] **AC3 tet 上传核消除分配风暴**:`build_volume` 改用批量 cell array + `SetCells(VTK_TETRA,…)`,**无 per-tet `vtkNew<vtkTetra>`**(代码层可核);tet 渲染输出等价。
- [x] **AC4 TetGen 走连续 span**:`TetGenTetMesher` 经 `acquire_points().span().data()` 喂 TetGen pointlist(`static_assert(sizeof(Point3)==24)` 在 cast 点),faceId/facetmarker 走 `TriangleView::faceIds` 连续 span;四面体化结果与迁移前等价(顶点/连接/faceId 守恒)。
- [x] **AC5 ITetMesher 注入缝重塑**:`ITetMesher::tetrahedralize` 不再硬依赖 `const XQTriangleSurfaceGeometryHandle&`,改为 `const IGeometrySource&`(或 lease);TetGen + 其调用方(VolumeMeshService)同步更新,所有实现/mock/stub 一起补。
- [x] **AC6 网格服务 + io writer 迁移**:`VolumeMeshService`/`SurfaceMeshService`/`ModelingService` 输入侧逐元素循环改经 Source span;`XQProjectWriter` flatten 改批量 span + 并行 faceId span;输出 round-trip 与迁移前等价。
- [x] **AC7 分层零违反**:io/core 无新增 `vtk*` 依赖;Source 公共签名零改动(static_assert 成立);新增/改动的 link 边方向合法无环。Release 全量 ctest 绿。
- [x] **AC8 lease 生命周期安全**:renderer 零拷贝借用路径下,VTK 对象引用借用指针期间数据全程有效(并发/重建/清理不悬垂);design 选定的"借用随 actor vs 上传后拷贝"策略落地且测试覆盖。
  - **落地 = copy-on-upload**:管线 filter 惰性执行 + renderer 长持 actor/无处存 lease → 选拷贝进 VTK 自有数组,lease 即时释放、无悬垂借用(AC8 天然满足)。test_scene_renderer 验证 add 后正确渲染。

## Notes
- 复杂里程碑,`task.py start` 前补 design.md(架构 + 关键决策:① renderer lease 生命周期策略 borrow-to-actor vs copy-on-upload;② ITetMesher 缝形态 IGeometrySource& vs 传 lease;③ 上传核 VTK 9.3 SetData/SetArray 的 offsets 生成 + int32-vs-vtkIdType 连接处理)与 implement.md(分阶段:renderer 上传核 → 网格服务/TetGen → io writer)。
- 依赖:M8b-1(46a939d)+ scale-probe(cabe074)+ M8b-2(机制收口,已归档)已完成。探针代码(`XQ/probe/`)是弃码参考,生产实现写在 visualization/services/adapters/io,不复用 probe 目录。
- 关联 memory:`m8b1-consumer-access-patterns`(消费者访问粒度矩阵,本迁移的依据)、`xq-build-recipe`、`xq-surface-winding-not-consistent`(体网格法线别假设同向)、`no-sideeffect-in-assert`、`tetgen-plc-volume-mesh`(faceId 守恒走 trifacemarkerlist)。
- 完成后:Source 接口被真实消费者喂过且不退化 → libXQ/Source 1.0 候选可正式冻结(冻结门 M8b-2 已通过,本里程碑补消费者存活证据)。
