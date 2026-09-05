# 现状渲染代码盘点(推倒重写的对象与可抄清单)

> 2026-07-04 主审亲读结论,行号基于 feat/gui-v2 @ a3715e5。
> 新接手者不必重新盘点,直接以此为准;若行号漂移以符号名搜索。

## 废弃路径(产品运行时不得再经过)

### XQMprView(src/visualization/XQMprView.cpp,905 行)——全部废弃
- 每切片格 = QFrame + QLabel,`Impl::renderAxis`(:184-237)每次滑块变化:
  `panel.renderer.clear()` → `addImageSlice`(整卷拷贝,见下)→
  `renderOffscreenToRgba`(每帧建 GL 上下文,见下)→ QImage→copy→QPixmap→setPixmap。
- 十字线/种子标记是**画在 pixmap 上**的(drawCrosshair/drawSeedMarker)。
- 3 个 SlicePanel 各持一个独立 XQSceneRenderer 实例。
- 可保留的**概念**(不是代码):axis 映射(0=x→Sagittal,1=y→Coronal,2=z→Axial)、
  seedPicked(i,j,k) 信号 + 窗口侧 pickMode_ 路由(Seed/PathPoint 互斥,
  XQMainWindow.cpp:310-330 + PathPageHooks 机制)、LayoutMode Quad/Single、
  retranslateAxisNames 的 changeEvent 先例。

### XQSceneRenderer 交互路径(src/visualization/XQSceneRenderer.cpp,1155 行)
- `build_image`(:98-150):整卷逐体素虚调用 scalarAt 拷进新 vtkImageData。
  **每次 addImageSlice 都全量重建**——这是卡顿主因之一。
- `renderOffscreenToRgba`(:964-1010):每帧 `vtkNew<vtkRenderWindow>` +
  SetOffScreenRendering + 整帧回读。**每帧创建/销毁 GL 上下文**——卡顿主因之二。
- `addImageSlice`(:398-439):vtkImageSliceMapper 用法本身可参考,但输入必须改成
  常驻 vtkImageData(一次构建,窗口级缓存),不能每帧 build_image。

## 可抄代码(payload→actor 构建,省一半工作量)

XQSceneRenderer.cpp 内以下函数的 **mapper/property 设置逻辑**直接可抄进新场景合成器
(注意:抄逻辑,归属新类;actor 生命周期改为常驻+显隐,不再 clear-and-rebuild):
- `addPath`(:441-):samplePoints 优先、控制点回退,vtkPolyLine;
- `addSurface(+LodOptions)`:vtkPolyData + vtkLODActor(LOD 分档 SurfaceLodBuilder);
- `addSurfaceProgressive(ChunkUploadSpec)`:分块上传 + spec.lod(:706 唯一消费点);
- `addVolumeMesh`/`addVolumeMeshProgressive`:vtkUnstructuredGrid;
- `addFlowResult`/`addSegmentationMask`:各自的着色/查找表设置;
- `build_image` 的 apply_geometry(origin/spacing/direction→vtkImageData)正确,可抄;
  逐体素拷贝循环若体数据源是 XQMemoryImageBufferHandle 连续 float,可改 memcpy 快路径
  (M8b-1 结论:消费者要整块连续视图,逐元素虚调用是风险点)。

## 保留在用(与渲染解耦,勿动)

- `XQRenderWidget::configureDefaultSurfaceFormat`(QVTKOpenGLNativeWidget::defaultFormat,
  必须在 QApplication 前调)——新四视图每个 view 都要这个前置。
- GeometryResourceManager 惰性源 + 预算(attachGeometryResources 传导 QSettings
  memory/geometryBudgetMiB)。resolveLazyGeometrySource 的 SurfaceOnly/Full 模式。
- XQTaskRunner + setWorkflowBusy 忙态矩阵(XQMainWindow.cpp:1999-)。
- 工作区打开/保存(openWorkspaceFromPath/saveWorkspaceFile,save-as blob 复制)。
- XQSceneModel(ui/):模型本身保留,M3 给它加 CheckStateRole 可见性列接渲染。

## 已知联动点(M4 拆 XQMainWindow 时的搬迁清单)

XQMainWindow.cpp 中与渲染耦合的成员/函数(拆分对象):
- onSceneSelectionChanged(:2097-):现"选中单节点渲染"入口,整段换掉;
- useVolume / loadImageFromPath / activeImage_(XQDemoVolume 容器)/ activeImageNodeId_;
- 导航器滑块 onNavigatorSliderChanged / stepAxialSlice / sliceStep_;
- pickMode_ / pathDraftPoints_ / pathDraftChanged_ / pathPickToggleSetter_(拾取路由);
- kDefaultLodOptions(匿名 ns const,enabled+interactive+2M budget);
- renderWidget_ = &mprView_->volumeView()(3D 格从 MPR 借 widget 的手法,新架构重定)。

## 测试现状与处置

- test_scene_renderer / test_scene_renderer_progressive / test_surface_lod /
  test_chunk_plan:测的是旧 SceneRenderer 离屏路径与 LOD/分块离散不变量。
  离散不变量测试(chunk_plan、surface_lod)保留;离屏渲染断言的部分随重写重做或
  降级为 test-only 路径。
- test_main_window / test_app_startup / test_path_stage:结构断言居多,新架构下
  大部分可迁移(objectName 尽量保持:xqMprAxial 等)。
- **新增测试的原则**:验收点放交互模型的离散不变量(如"勾选 N 个可见节点→场景 actor
  数为 N""滑块 idx→mapper SliceNumber==idx"),不做像素断言(假绿)。流畅度/观感
  只认真机。
