# M9b 大规模可视化前处理(父任务 / 里程碑总纲)

> 架构演进主轴(M8a→M8b-1→M8b-2→M9a→M9b)的**收官里程碑**。父任务只立总纲、范围切分、依赖序、全局验收线与对已冻 Source 1.0 的约束;具体实现落 4 个子任务。

## Goal

在已冻 **libXQ / Source 1.0**(M9a 完成)之上,把 XQ 从"小规模能跑"推到"**真大规模(单 Surface 2000万三角 / 单 Tet Mesh 1000万 tet / 单活动资产)可视化前处理可用**"。核心是打掉 scale-probe 实测的两条独立瓶颈线,并补齐 M9a 显式留的懒驻留基座缺口。

**两条独立瓶颈线(验收指标必须分开,别混)**:
- **线1 — GPU 上传峰值 + 耗时**:scale-probe 实测 large(20M 三角)addSurface 上传 23.5s / 进程峰值 3786 MiB。**真实根因 = `vtkPolyDataNormals`(AutoOrient+Consistency)在 20M 三角上的 deep copy + mapper 一次性整块上 GPU**(不是逐元素风暴——那已被 M9a 消除;旧 probe REPORT.md:47 的归因是 M9a 重写前的描述,作废)。**只能靠 LOD/降采样/渐进上传打**。
- **线2 — reader 常驻 + 瞬时双份**:reader 整块 load 几何 blob → 建 handle → payload 持整块 vector(常驻一份);解码期还有瞬时 ~2× 双份。**靠几何 mmap 懒驻留源 + payload 去 vector 化打**,与线1 目标不同。

## Background(调研已备齐的依据)

本里程碑规划期做了 5 路并行调研 + 对抗式综合(存档 `research-synthesis.json`),关键校准结论直接采纳:
- **瓶颈归因校准**:M9a 后 `build_surface`/`build_volume` 已是 `vtkCellArray::SetData`+`SetCells` 整块批量。M9b **不优化已批量的 build_cells**,优化点在上传层(LOD/分块/decimation)+ normals deep copy。
- **内存峰值分线**(见上):3786 MiB 几乎全是 VTK copy-on-upload,mmap acquire 仅 274 MiB。去 vector 化/mmap 源**不降这个峰值**,只降 reader 常驻/瞬时份。
- **VTK 9.3 能力实编核实**(头存在性已查):`vtkHardwareSelector` 依赖真实 GL 上下文 → **headless/离屏 ctest 不可测**;VTK **无显存 byte-budget 公共 API**,显存只能 CPU 侧 LOD/抽稀控量;`vtkCompositePolyDataMapper2` 与 `CompositePolyDataMapper` 9.3 并存,须锁一个防升级破裂。
- **writer 不产几何 .merkle**(实证 grep RC=1):几何 mmap 源默认完整性走 **FullVerify**(不依赖 sidecar),SegmentedMerkle + 几何 sidecar 写路径作显式后续项——否则老工程几何 asset 会静默 acquire 失败。
- **faceId 三重浪费**:`ResidentSurfaceSource::acquire_triangles` 每次 O(N) 物化 faceId(handle 无 vector 访问器)+ `build_surface` 逐元素 `SetValue` 灌 `vtkFloatArray`(注意是 **float 非 int**,喂 LUT 着色,不能简单 memcpy 成 int 数组——LUT 映射语义要先确认)。borrow-faceId 在 A 里解决前两份。

## Scope

### In scope(本里程碑 = P0+P1,4 子任务)
- **A(P0 基座)** 生产级 `MappedGeometrySource`(io,磁盘 mmap 几何源,照 `MappedVoxelSource` 范式)+ `GeometryResourceManager::acquireGeometrySource` 工厂(services,与 voxel 对称)+ `TriangleLease` borrow-faceId 零拷贝变体(core,纯追加静态工厂 + 第二 keepalive 成员)。
- **B(P0)** LOD/降采样上传:`vtkActor`→`vtkLODActor`,decimation 多级低模(离线/异步,20M 单线程预处理不卡主线程)+ CPU 侧按显存预算选 LOD 级。含 **F 前置**:CMake 补 `find_package(VTK COMPONENTS ... RenderingLOD)` + `vtk_module_autoinit`。
- **C(P1,依赖 B)** 渐进/分块上传:多 actor/多 source 分块边建边显(**不在单 vtkPolyData append**;VTK 无真增量),still/interactive 切级。
- **E(P1,依赖 A)** payload 去 vector 化:**并存惰性路径**——payload 增可选 assetId 经 manager 惰性取 `IGeometrySource`,与现 resident handle 路径并存,消费者渐进迁移;**不物理删 handle vector 字段、不破 clone 语义**(可回退)。

### Out of scope(本里程碑后)
- **D 大网格拾取**(单点拾面):域闭环(`boundaryFaceById`/`faceById`)已就绪,缺"屏幕点→cell→faceId"前半段。**CPU `vtkStaticCellLocator` 起步**(离屏可测;20M 建树时间/内存需 probe 先量);GPU `vtkHardwareSelector` 待 CPU 不达标再评估。范围严格收敛单点拾面,框选留后。降为本里程碑后续子任务或 M9b' 增量。
- **裁剪/视锥剔除**:`vtkClipPolyData`/`vtkExtractGeometry` 能力齐备但**无 UI 牵引**,贸然做是过度工程,显式后置。
- **几何 sidecar 写路径 + SegmentedMerkle 几何完整性**:A 默认 FullVerify,sidecar 写路径作显式后续项。
- **物理删 payload handle vector + 消费者全量迁 `IGeometrySource&`**:E 走并存路径,不做物理删除。
- 不复用 `XQ/probe/` 弃码(参考可,生产重写)。

## 依赖序与并行性
```
F(CMake,折进 B 前置) ─► B(LOD) ─► C(渐进上传)
A(mmap源+工厂+borrow-faceId) ─► E(去vector化)
```
- **A 与 B 可并行**:A 是 io/services 纯增量基座;B 是 visualization 消费 `IGeometrySource`,现 `ResidentSurfaceSource` 即可喂,不必等 A 落地。
- **C 依赖 B**(分块策略基于 LOD 决策);**E 依赖 A**(惰性取 source 需 mmap 源+工厂就绪),改动面最大放最后,配全量 ctest。
- **cell→faceId 映射契约**:当前 cellId↔faceId 依赖隐式"cell 顺序==三角下标"(build_cells 不重排)。**LOD/分块一旦改上传顺序或抽稀三角,下标相等失效**——B/C(及未来 D)设计期必须共同约定**显式 cell→faceId 映射表**,不再依赖下标相等。此为父任务级硬契约,写进各子 design。

## Constraints(父任务级,所有子任务遵守)
- **不破已冻 Source 1.0 公共签名**:`IGeometrySource` 3 个纯虚(`IGeometrySource.h:19-26`)**绝不能加纯虚**(会破所有实现者+mock);LOD/分块逻辑封在 .cpp 或 SourceViews 之上的新组件。新增一律"加不破"(borrow-faceId 新静态工厂、GeometrySourceSpec、manager 新方法、renderer 新可选入口)。static_assert 仍成立。
- **分层不变**:`MappedGeometrySource`→io、`acquireGeometrySource`→services、LOD/渐进/拾取→visualization;io/core 永无 `vtk*`;VTK 只在 visualization/adapters 私有实现。
- **VTK mapper 锁定**:分块多 actor 若用 composite mapper,锁 `vtkCompositePolyDataMapper2` 或 `CompositePolyDataMapper` 之一,全里程碑统一,防 9.3→后续升级破裂。
- **RenderStats 契约**:`pointCount` 保持报**源点数**(语义稳定、headless 可断言);LOD 实际上传量若需暴露,**新增字段**不改旧义。多 actor 分块下 `ok`/`actorCount` 定义在各子 design 明确。
- **decimation 离线/异步**:20M 单线程预处理会卡主线程,B 必须异步或后台。
- 验收铁律:Release + 全量 ctest + 假绿抽查;副作用调用(`acquire_*`/locator build)不进 assert。

## Acceptance Criteria(父任务级,= 各子任务 AC 汇总线)
- [x] **AC-A 几何 mmap 源 + 工厂可用**(A 已归档):`MappedGeometrySource` 经 `IGeometrySource` map+verify(FullVerify)+reinterpret 零拷贝吐 points/tris/tets span,逐元素 == 源;`acquireGeometrySource(assetId, spec)` 返回带 pin 的 handle,命中缓存不重映射,超预算 LRU 驱逐只回收 `use_count==1`;`TriangleLease` borrow-faceId 零拷贝(无 O(N) 物化)。
- [x] **AC-B LOD 降上传量 + 交互**(B 已归档):开 LOD 后 GPU 上传量较 M9a 全量**显著下降**(far 级 large 20M 三角实测 960 点 vs 10M);`vtkLODActor` 交互 still/motion 装配成功;cell→faceId 显式映射保级。**进程峰值工作集减半未达成**——实测 host 完整物化(`build_surface`)是峰值主导、decimation 在其下游只砍上传量(full=3422 / far=3102 MiB,−9%);**host 峰值降级移交几何源/源头流式降采**(须在物化前从 `IGeometrySource` 流式降采,超 B 范围)。详见 B 子任务 prd + spec `XQ/visualization/lod-and-upload.md`。
- [x] **AC-C 渐进上传**(C 已归档):分块边建边显,首块可见时间 << 整块完成时间;still/interactive 切级正确;无悬垂(copy-on-upload 守恒)。`addSurfaceProgressive`/`addVolumeMeshProgressive` 按 cell 区间分块多 actor,首块回调即 ≥1 actor 且 `completedChunkCount<chunkCount` 离屏非黑;全局 faceId LUT range 跨块无断层;`interactive` 时每块挂 `vtkLODActor`(headless 不可测切级,真交互手动验)。段偏移正确性归约为 `plan_chunks` 划分不变量,`test_chunk_plan`(header-only)直断 + 假绿抽查注入 `begin+1` 真转红。详见 C 子任务 prd + spec `XQ/visualization/lod-and-upload.md`。
- [x] **AC-E payload 去 vector 化并存**(E 已归档):payload 可选 assetId 路径经 manager 惰性取 source,reader 不再强制整块常驻;resident handle 旧路径并存不退化;clone 语义不破;全量 ctest 绿。payload `geometryAssetId`(加不破)+ reader `XQProjectReadOptions{lazyGeometry}` opt-in(io 只标记不物化)+ services `resolveLazyGeometrySource`(从 BufferRef 推 spec 调 acquireGeometrySource);AC4 等价(eager handle==lazy mapped source 逐元素全等)、AC3 删 blob 反证 lazy 不读、AC2 clone 三分支全验。详见 E 子任务 prd + spec `XQ/core/source-interface.md`。
- [x] **AC-全局 分层 + 签名零违反**:io/core 无 `vtk*`;Source 1.0 公共签名零改动(static_assert 成立);新增 link 边方向合法无环;Release 全量 ctest 绿 + 大规模性能脚本通过。 — 四子任务全程 Source 1.0(meta + 3 acquire 纯虚)未动;`ChunkPlan.h`/payload/resolver 均 VTK-free 在合法层;`services→io` PRIVATE 无反向边;Release 全量 60/60 绿(E 收口计数)+ B 的大规模 probe 实测(20M 三角各上传模式独立进程)。
- [x] **AC-契约 cell→faceId 显式映射**:LOD/分块改变上传顺序/抽稀后,拾取/着色仍能正确 cell→faceId(显式映射表,不依赖下标相等)。 — B 的 `CellFaceIdMap`(分级保 faceId,medium 分 region 重建表、far 平色 valid=false);C 每块 actor 显式承载段偏移 `faceIds[range.begin+j]`,`plan_chunks` 划分不变量(`test_chunk_plan`)守正确性,不依赖全局下标相等。拾取(D)留后续但映射契约已就位。

## Notes
- 复杂里程碑,**拆父 + 4 子任务**(A/B/C/E)。各子任务独立 prd/design/implement + 独立验收 + 独立 commit;父任务跟踪汇总线。
- 依赖:M8a/M8b-1/scale-probe/M8b-2/M9a 全部已完成归档。Source 1.0 已冻。
- 调研存档:`research-synthesis.json`(本目录,5 路调研 + 对抗式综合全文)。
- 关联 memory:`renderer-source-copy-on-upload-not-borrow`、`m8b1-consumer-access-patterns`、`m8a-wholefile-sha-vs-streaming`、`evict-harness-pin-before-evict-race`、`xq-build-recipe`、`tetgen-plc-volume-mesh`、`xq-surface-winding-not-consistent`。
- 拍板决策(2026-06-30):① 完整性默认 FullVerify(几何 sidecar 写路径后续);② stats 报源点数;③ 范围 P0+P1(A/B/C/E),D 拾取/裁剪后续;④ E 走并存惰性路径不破 clone;⑤ D 若做 CPU locator 起步。
- 完成后:架构演进主轴 M8a→M9b 全部里程碑达成,XQ 整体重构(库优先 libXQ + 大规模可视化前处理)交付。
