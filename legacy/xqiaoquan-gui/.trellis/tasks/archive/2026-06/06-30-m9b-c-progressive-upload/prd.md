# M9b-C 渐进/分块上传(多 actor 边建边显 + still/interactive 切级)

## Goal

打掉 scale-probe 实测的「20M 三角 `addSurface` 一次性上传 23.5s 期间界面全程无响应、无任何中间反馈」体验墙。VTK 无真增量(`vtkPolyData` 不能高效 append),故走**多 actor / 多 source 分块边建边显**:把单个大几何按三角/tet 数阈值切成若干块,每块独立构 `vtkPolyData`/actor、逐块 `AddActor` 进 `vtkRenderer`,**首块尽快可见**;每块上传后可回调上层刷新进度。still/interactive 复用 B 的 LOD actor + `DesiredUpdateRate` 切级。本子任务**不引入单一 `vtkPolyData` 的增量 append**(VTK 9.3 无高效路径,设计前提)。

## Background

- **瓶颈定位(父任务校准)**:M9a 后 `build_surface`/`build_volume` 已是 `vtkCellArray::SetData` + `SetCells` 整块批量(`XQSceneRenderer.cpp:166-237`),逐元素风暴已消除。剩下的是**单 actor 一次性整块上传**:整个 20M 三角的 points/cells/faceId + `vtkPolyDataNormals` deep copy 在一次 `addSurface`(`cpp:373-425`)内完成,期间无中间产物可显、无进度。
- **VTK 无真增量**:`vtkPolyData` 没有高效追加 cell/point 的公共路径;一次性塞满才能 render。唯一可"边建边显"的手段是**多个独立 actor**——renderer 已是多 actor 场景模型(`XQSceneRenderer` Impl 持一个 `vtkRenderer`,`AddActor` 累加,`clear()` = `RemoveAllViewProps`,`cpp:275-283`)。
- **faceId 着色是 float + LUT**:`build_surface` 把 faceId 灌进 `vtkFloatArray` 喂 `vtkLookupTable`,LUT 的 `TableRange`/`ScalarRange` 取自该块 faceId 的 `GetRange`(`cpp:403-416`)。**分块后若每块各自算 range,跨块颜色会漂移** —— 必须用全几何统一 range。
- **cell→faceId 隐式契约(父级硬约束)**:当前 cellId↔faceId 依赖「cell 顺序 == 三角下标」(`build_cells` 不重排)。分块按三角区间切 + 每块重排点索引后,全局下标相等失效 → 每块 actor 必须**显式承载自己的 cell→faceId 段偏移**。
- **copy-on-upload 守恒(M9a 决策 / memory `renderer-source-copy-on-upload-not-borrow`)**:renderer 不持 lease,数据在 `build_*` 内一次性拷进 VTK 自有数组,lease 即释放。分块下每块同样 copy-on-upload,**不得**把 source span/lease 借用给 actor 后跨块存活。
- **依赖 B**:分块的切分阈值与每块的 LOD 级由 B 的 LOD/decimation 决策驱动;C 在 B 的 `vtkLODActor` 之上做"多块 + 渐进 + still/interactive 联动",不重复实现 LOD。
- **headless 验收形态**:`test_scene_renderer.cpp` 已确立 headless 范式(`renderOffscreenToRgba` + `any_non_black`,断言 `RenderStats.actorCount`/`pointCount`,`cpp:75-107`)。分块"边建边显"在离屏靠**逐块回调计数 + 中途快照非黑**验证,不依赖真实交互窗口。

## Scope

### In scope
- **渐进上传入口(visualization,加不破)**:`XQSceneRenderer` 新增可选入口 `addSurfaceProgressive` / `addVolumeMeshProgressive`(收 `const IGeometrySource&` + `ChunkUploadSpec` + 逐块进度回调),与现 `addSurface`/`addVolumeMesh`(`cpp:373-468`)并存;旧入口语义不动。
- **分块计划(visualization 内部组件)**:`ChunkPlan` 把源的三角/tet 区间按阈值切成 N 块,记录每块的 cell 全局区间 `[begin, end)`(= 该块 cell→faceId 段偏移)。切分阈值取自 `ChunkUploadSpec`,默认值与 B 的 LOD 决策对齐。
- **逐块构建 + 加 actor**:每块从 source 的连续 span 切出本块 cells、收集本块引用的点并重映射为局部索引、切出本块 faceId 段,copy-on-upload 进独立 `vtkPolyData` → 独立 actor(B 的 `vtkLODActor`)→ `AddActor`。首块加完即可 render 可见。
- **多 actor stats 语义**:`RenderStats` 纯追加字段 `chunkCount` / `completedChunkCount`(不改 `ok`/`actorCount`/`pointCount` 旧义);`pointCount` 仍报**源总点数**;`actorCount` 报本次渐进新增的块 actor 数。
- **全局 faceId LUT range**:渐进上传前算一次全几何 faceId range,所有块 mapper 共用,保证跨块着色一致。
- **still/interactive 切级联动(与 B 协同)**:在 `XQRenderWidget` 渲染循环/交互器接入点(`XQRenderWidget.cpp:41-47` render + 交互器)接 `DesiredUpdateRate`,interactive 时低 LOD、still 时高 LOD,作用于全部块 actor。

### Out of scope
- 单一 `vtkPolyData` 的增量 append(VTK 无高效路径,设计前提排除)。
- LOD 级别本身的生成 / decimation(B 负责);C 只消费 B 的 LOD actor。
- 真正的多线程/异步分块(本子任务先做**主线程逐块 + 回调让出**;后台线程化是后续项,因 VTK 对象非线程安全,跨线程上传需专门设计)。
- 几何 mmap 源 / `acquireGeometrySource`(A 负责);C 用 `ResidentSurfaceSource`/`ResidentTetSource` 即可喂 `IGeometrySource&`。
- 拾取 / 裁剪 / 视锥剔除(D 及后续)。
- 不复用 `XQ/probe/` 弃码。

## Constraints

- **不破已冻 Source 1.0 公共签名**:`IGeometrySource` 3 纯虚(`IGeometrySource.h:19-26`)绝不加纯虚;分块逻辑全部封在 visualization 的新组件 / `.cpp` 内,经现有 `acquire_points`/`acquire_triangles`/`acquire_tetrahedra` 取连续 span。`static_assert(sizeof(Point3)==24)`(`cpp:144`)仍成立。
- **分层不变**:渐进/分块逻辑落 visualization;io/core 永无 `vtk*`;`RenderStats` 新字段在 `XQSceneRenderer.h`(VTK-free 头)。
- **RenderStats 契约**:`pointCount` 保持报源总点数(headless 可断言);新增字段不改旧义。多 actor 下 `ok`/`actorCount` 语义在 design 明确并落 AC。
- **VTK mapper 锁定**:若分块用 composite mapper,锁 `vtkCompositePolyDataMapper2` 或 `CompositePolyDataMapper` 之一(全里程碑统一)。本子任务默认**每块独立 `vtkPolyDataMapper` + 独立 actor**(承接现 `build_surface` 管线),不引 composite mapper;若引,在 design 显式锁定。
- **cell→faceId 显式映射(父级硬契约)**:每块 actor 维护自身 cell→faceId 段偏移(块基址 + 局部 cell 下标),不依赖全局下标相等。
- **copy-on-upload 守恒**:每块数据 copy 进 VTK 自有数组后 lease 即释放,actor 不借用 source 指针(无悬垂)。
- **依赖 B**:切分阈值/每块 LOD 级基于 B 的 LOD 决策;B 落地后 C 对齐其 actor 类型与 `DesiredUpdateRate` 接口。
- 验收铁律:Release + 全量 ctest + 假绿抽查;`acquire_*` 等副作用调用不进 assert;decimation 离线/异步(B)。

## Acceptance Criteria

- [x] **AC1 分块边建边显**:对合成大几何(B 同款规模或可承载的最大规模)调 `addSurfaceProgressive`,逐块回调被触发 N 次;**首块回调发生时已有 ≥1 个 actor 且 `completedChunkCount < chunkCount`**,此刻 `renderOffscreenToRgba` 输出非黑(首块可见早于整块完成)。 — `test_scene_renderer_progressive.cpp::test_progressive_first_chunk_visible`(3042 三角 /500 → 7 块,首块回调 `completedChunkCount==1 && chunkCount==7 && actorCount>=1` 且离屏快照非黑);tet 通路 `test_progressive_volume_mesh`(120 tet /30 → 4 块)。
- [x] **AC2 渐进等价整块**:全部块上传完成后,场景渲染与同几何走旧 `addSurface` 一次性上传**语义等价**(可识别同一几何;离屏快照非黑且覆盖一致),跨块 faceId 着色用全局 LUT range **无颜色断层**。 — `test_progressive_equivalent_to_oneshot`(渐进 vs 一次性覆盖率 ≤15% 偏差);全局 faceId range 在 `addSurfaceProgressive` 内一次算出、所有块 mapper 共用(`XQSceneRenderer.cpp:632-641`)。
- [x] **AC3 多 actor stats 语义正确**:渐进完成后 `RenderStats.pointCount == 源总点数`(不因分块缩水)、`actorCount == 实际新增块 actor 数 == chunkCount`、`completedChunkCount == chunkCount`、`ok == true`;任一块构建失败时 `ok == false` 且 `completedChunkCount` 反映已完成数。 — `test_progressive_first_chunk_visible` 断言 `pointCount==srcPts`、`actorCount==chunkCount`、`completedChunkCount==chunkCount`;tet 同。
- [x] **AC4 cell→faceId 段偏移正确**:对每块 actor,块内局部 cell 下标 j 经「块基址 + j」映射回的全局 faceId,与源 `acquire_triangles().view().faceIds[基址+j]` 逐元素一致(可用小规模确定性几何 dump 核验)。 — 段偏移正确性归约为分块划分不变量:`plan_chunks` 必须把 `[0,cellCount)` 精确铺满(连续、无缝、无叠、每 cell 恰覆盖一次),由 `test_chunk_plan.cpp`(header-only,无 VTK)直接断言 + `test_chunk_faceid_segment_offset`(用同一 `plan_chunks` 复现 renderer 的段偏移并逐 cell 核 faceId)双重守卫。**假绿抽查实测**:`ChunkPlan.h` 注入 `begin+1` 丢首 cell → 两测同时转红(`FAIL: 3042/500`、`FAIL: ...covers every cell once (AC4)`),还原 → 57/57 绿。渲染图像测试因丢几 cell 仅亚像素空洞(15% 容差吞掉),故段偏移在源头而非像素上验证。
- [x] **AC5 still/interactive 切级**:交互器 `DesiredUpdateRate` 接入后,interactive 态选低 LOD、still 态选高 LOD(与 B 的 LOD actor 联动);headless 不可测真实交互,以"块 actor 均为 B 的 LOD actor 类型 + `DesiredUpdateRate` 接线存在"为可核证据,真实交互切级手动验证并记录。 — `spec.lod.interactive==true` 时每块走 `mountLodActor`(`XQSceneRenderer.cpp:656-665`)挂 `vtkLODActor`(full 为主 mapper,medium/far 经 `AddLODMapper`);`XQRenderWidget` 经 `QVTKOpenGLNativeWidget` 默认交互器自动驱动 `DesiredUpdateRate`(运动→低级、静止→高级)。headless 无交互器驱动 `AllocatedRenderTime`,故离屏只验 LOD actor 装配(`test_surface_lod::test_interactive_lod_actor_assembles`),真实切级手动验证。
- [x] **AC6 无悬垂(copy-on-upload 守恒)**:渐进上传后释放源 handle / source,再 render 仍正确(承接 M9a AC8,证明各块未借用 source 指针)。 — `test_progressive_no_dangling_after_release`(作用域内 add 完即销毁 surf+src,域外 render 仍非黑);每块 `build_cell_chunk` 把点/连接拷进 VTK 自有数组(`XQSceneRenderer.cpp:263-304`)。
- [x] **AC7 分层 + 签名零违反**:io/core 无 `vtk*`;`IGeometrySource` 公共签名零改动(`static_assert` 成立);`RenderStats` 仅追加字段;Release 全量 ctest 绿 + 假绿抽查通过。 — `RenderStats` 仅追加 `chunkCount`/`completedChunkCount`;`ChunkPlan.h` VTK-free;`IGeometrySource` 3 纯虚未动;Release 全量 57/57 绿,假绿抽查见 AC4。

## Notes
- 依赖:B(LOD/decimation,P0)。A 与 C 无直接依赖(C 用 Resident*Source 即可喂 source)。
- 关联 memory:`renderer-source-copy-on-upload-not-borrow`、`m8b1-consumer-access-patterns`、`xq-surface-winding-not-consistent`、`xq-build-recipe`、`no-sideeffect-in-assert`、`ninja-target-incremental-fakegreen-trap`。
- 父任务硬契约「显式 cell→faceId 映射」「pointCount 报源点数」在本子任务以 AC3/AC4 落地。
- 本子任务为 P1,改动面集中在 visualization,先做 surface 通路打透,tet 通路复用同骨架。
