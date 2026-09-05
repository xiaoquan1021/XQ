# XQRenderScene 常驻渲染管线契约(07-04-render-arch-rebuild)

> 来源:`07-04-render-arch-rebuild`(B1~B5,2026-07-05 用户真机签收)。
> 产品交互渲染的唯一入口。改本层前先读 index.md 的「渲染架构定论」。

## Scenario: 产品渲染入口与挂载链

### 1. Scope / Trigger
- Trigger:任何触碰四视图渲染、切片、节点显隐、窗宽窗位、十字线的改动。
- 渲染链:`XQRenderScene`(纯 VTK,无 Qt)→ `XQSliceViewWidget`×3 / `XQVolumeViewWidget`(QVTKOpenGLNativeWidget 挂载)→ `XQMprWidget`(2×2 网格)→ `XQMainWindow::syncRenderScene`(app 层增量同步)。
- `XQSceneRenderer` 是 **test-only**(离屏 RGBA 契约靶,test_scene_renderer / test_scene_renderer_progressive / test_surface_lod);产品代码禁止引用(CMakeLists.txt 与其 .h 均有声明)。旧 XQMprView/XQRenderWidget 已于 B5 删除,不可回切。

### 2. Signatures(XQRenderScene.h,VTK-free pimpl)
- `bool setVolume(const XQImageVolume&, const XQMemoryImageBufferHandle*)` — 体数据一次上传建常驻 vtkImageData;重复调用整体替换;非法几何返回 false。
- `void setSliceIndex(int axis, int index)` / `sliceIndex` / `sliceCount` — axis 约定 **0=x/Sagittal, 1=y/Coronal, 2=z/Axial**;越界 clamp;无体数据 no-op。
- `RenderStats upsertNode(const NodeId&, const XQDataNode&)` — 首见建 actor 组;re-upsert 删旧重建(payload 可能已换);不可渲染 payload → ok=false 零痕迹。
- `upsertNodeProgressive(id, IGeometrySource&, GeoKind, spec, onChunk)` — app 层已解析 Source 的渐进路径。
- `removeNode / clearNodes / setNodeVisible / setNodeOpacity / setNodeColor` — 呈现属性归本类所有,存渲染端,payload 重建不丢(re-upsert 保留用户设定,首见才用缺省;Mesh/TetMesh 首见缺省隐藏)。
- 探针(headless 测试用,只读不渲染):`nodeActorCount / hasNode / nodeVisible / uploadedPointCount / nodeSliceCutCount / nodeContourOverlayCount / nodeColor / crosshairLineAxis / crosshairLineColor / seedMarkerVisible`。
- `void* vtkRendererHandle(ViewId)` — widget 挂载用不透明句柄。

### 3. Contracts
- **一次上传**:切片翻页 = 改 mapper/reslice 参数,绝不重建整卷 vtkImageData;窗宽窗位 = 共享 vtkImageProperty(三切片+3D 平面联动)。
- **挂载入口唯一**:`XQVolumeViewWidget::configureDefaultSurfaceFormat()` 必须在 QApplication 构造前调用(main.cpp 与所有 GUI 测试同款);VTK/Qt 的 GL surface format 由它统一。
- **切片相机取景**:`setupSliceCameras` 在 ResetCamera 后设 `camera->SetParallelScale(0.5 * upExtent)`(upExtent = 该视图 up 轴的图像世界尺寸)——纵向精确满窗零边距;横向由窗口宽高比自然裁边(SV 同款观感)。别退回 ResetCamera 默认边距(真机窄条置中缺陷)。
- **路径渲染为细线**:polyline 直连 mapper + `SetLineWidth(2.0)`,**不用 vtkTubeFilter**(包围盒比例半径在长路径下粗过血管模型,且 N 条路径 N 次管状化)。
- **十字线拖拽只抓交点**:`XQSliceViewWidget::hitLine` 仅当两条线都在容差(8px)内才命中(返回 3),单线不可抓;命中光标 CrossCursor(不用 SizeAllCursor)。
- **app 层增量同步**(XQMainWindow::syncRenderScene):`syncedPayloads_`(NodeId→payload 裸指针,只做同一性比较)指纹 diff——指针未变且 hasNode → 跳过重建;visited 集合离场清扫 removeNode。**前提:payload 变更必须换 shared_ptr**(setPayload/clone 均满足);发现原地改 payload 不换指针的路径即为契约违规。可见性 reconcile 每节点照常跑,不随跳过省略。
- **批量解析**:pendingParses_ 队首连续同 kind 合成单 taskRunner 任务(Image 单独),整批一次 refreshSceneTree(143 文件工程从 143 轮全树重建 → ~2 轮)。

### 4. Validation & Error Matrix
- setVolume 非法几何 → false,场景不变。
- upsertNode 不可渲染 payload(Image/SimulationCase/Unknown/空)→ ok=false,无 actor 残留。
- setNodeVisible/Opacity/Color 未知 id → no-op;nodeVisible 未知 id → false;uploadedPointCount 未知 id → -1。
- 无体数据时 setSliceIndex no-op、sliceIndex=-1、sliceCount=0、worldToSliceIndex=-1。
- syncRenderScene 增量跳过后 payload 实际已变(原地变更)→ 渲染陈旧 = 架构违规,修数据路径不是关 diff。

### 5. Good/Base/Bad Cases
- Good:undo/redo 后未变 payload 的大 surface 不重跑 LOD decimation(uploadedPointCount 不变断言)。
- Base:LodOptions 默认关时 upsertNode 与 M9a 行为等价(test_scene_renderer 基线)。
- Bad:每帧新建 vtkRenderWindow / 离屏回读贴 QLabel / 选中单节点才渲染 —— 全部已证伪禁止。

### 6. Tests Required
- `test_render_scene`:切片相机 ParallelScale(合成卷 8×6×4 → Axial 2.5/Sagittal 1.5/Coronal 1.5,容差 1e-6)、节点探针、十字线轴色。
- `test_main_window`:异步图像落地等待循环(10s pump 先例)、增量 sync(undo/redo 后 uploadedPointCount 不变、全 undo 后 hasNode false)、批量解析任务数(0007:1≤n≤3,下界防接线假绿)。
- `test_i18n_resources`:断言只许指向**活 context**(死 context 断言恒真假绿);GUI 面板串走 xqTr → 手工 ts 块 + lrelease,禁 lupdate(见 core/build-and-test.md)。
- 改 Q_OBJECT 头后必须全新构建(ninja-stale-moc 假崩);流畅度/观感只认真机。

### 7. Wrong vs Correct
#### Wrong
```cpp
// 每次场景变化全量重建:大 surface 重跑 decimation,push/undo 卡顿。
renderScene_->clearNodes();
for (auto& node : renderables) renderScene_->upsertNode(node.id, node);
```
#### Correct
```cpp
// payload 指纹 diff:未变节点跳过,离场节点单独 removeNode。
const XQPayload* fp = node.payload().get();
if (auto it = syncedPayloads_.find(id);
    it != syncedPayloads_.end() && it->second == fp && renderScene_->hasNode(id)) {
    // skip rebuild; visibility reconcile still runs below
} else {
    renderScene_->upsertNode(id, node);
    syncedPayloads_[id] = fp;
}
```

---

## Scenario: 切片拾取交互(种子/路径控制点)与视图标记(07-06-pick-interaction-fix)

> 来源:`07-06-pick-interaction-fix`(用户真机签收)。切片视图上「点一个点」是种子拾取、路径控制点拾取、未来断面手绘共用的底层交互。本节记确定性接管、拾取落点、控制点标记三项契约。

### 1. Scope / Trigger
- 种子/路径控制点拾取都经 `XQMprWidget::setSeedPickingEnabled` → 三个 `XQSliceViewWidget::setSeedPickingEnabled`(唯一入口,`pickMode_` 在 app 层分流 Seed/PathPoint)。

### 2. Signatures(新增)
- `XQSliceViewWidget`:`void setSeedPickingEnabled(bool)`;probe `bool pickStyleActive() const`;信号 `voxelPicked(i,j,k)` / `pickOutOfBounds()`。头保持 **VTK-free**:两个交互 style 藏在 `.cpp` 的浅 pimpl `struct StyleHolder`(`std::unique_ptr<StyleHolder>`),**不得**把 `vtkSmartPointer` 放进头。
- `XQRenderScene`:`setPathControlPoints(const std::vector<std::array<double,3>>& world)` / `clearPathControlPoints()`;probe `pathControlPointCount()`(总数)、`pathControlVisibleCount(int axis)`(该 2D 视图过滤后可见数)。接口用 `std::array/vector` 保持头 VTK-free。

### 3. Contracts
- **拾取模式显式接管交互(根治红点时有时无)**:进拾取模式把 VTK 交互器 style 从 `vtkInteractorStyleImage` 换成空 `vtkInteractorStyle`(不做 window/level),退出还原。**不靠 eventFilter「抢在交互器前」的时序**(native OpenGL 窗口 press 路由顺序不保证=红点时灵时不灵,memory `xq-seed-pick-eventfilter-vtk-race`)。两 style 须被 widget 稳定持有(浅 pimpl),裸 void*+仅交互器持一引用会让切走的 style 被回收。
- **拾取落点 pin 面外维到当前切片**:`displayToWorld` 只解面内两维,第三维(该视图切片法向轴 = `axis_`)回来的是相机焦平面深度**不是当前切片**。拾取分支必须 `world[axis_] = sliceWorldCoord(axis_)` 把它 pin 到当前切片,否则非轴位面所有点塌到体数据该轴最远端(0082:x 钳到 7.84)。与 `hitLine` 对十字线的 per-line pin 同源。
- **控制点标记按切片距离过滤**:glyph3D 每视图一 actor 承载 N 点(色 0.2,0.7,0.9 区分种子红球);`setPathControlPoints` 存全量世界点 `pathControlWorld_`,每 2D 视图只显示到其切片平面距离 ≤ `spacing[planeAxis]` 的点(贴切片才亮),3D 视图全显示;`setSliceIndex` 末尾重算(照 `rebuildContourOverlaysForAxis` 先例)。volume reload / clear 清 `pathControlWorld_`。
- **体外拾取给反馈**:点落体数据外 `worldToVoxelIndex` 失败 → 发 `pickOutOfBounds`(仍消费事件),app 层状态栏提示,不静默丢弃。

### 4. Validation & Error Matrix
- 无体数据 / offscreen 无交互器:`setSeedPickingEnabled` 判空跳过 SetInteractorStyle;`pickStyleActive` headless 读跟随开关的成员布尔(`pickStyleEngaged_`)。
- 无 marker 基础设施(未 build,如无 volume):`setPathControlPoints` 是 no-op,count 保持 0(报告 count 却不渲染=状态漂移)。
- `pathControlVisibleCount` 非法 axis → -1。

### 6. Tests Required
- `test_slice_view_pick_style`:`setSeedPickingEnabled(true/false)` → `pickStyleActive()` 随之翻转(三轴)。假绿:篡改 SetInteractorStyle 恒 image → 断言转红。
- `test_render_scene` §21:控制点 count 增删、no-volume no-op、**切片距离过滤**(z=1 slice 时 z=1 点显示、z=3 滤除,切 z=3 反转 `pathControlVisibleCount==1`)、volume reload 清零。假绿:篡改 count / 篡改过滤条件均须转红。
- 真机第一门禁(memory `gui-task-green-tests-not-done`):红点连点必出、控制点贴切片显隐、拾取落点在当前切片、双击列表跳转、退出还原 window/level。**派用户真机前必核 exe 时间戳新过所有改动源**(memory `gui-realmachine-test-verify-exe-timestamp`)。
