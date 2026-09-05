# XQ GUI 层功能穷尽式盘点报告

> 目标:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`(worktree,分支 `feat/gui-mitk-layout`,以工作树未提交内容为准)
> 范围:`src/app/`、`src/ui/`、`src/visualization/`、`resources/`
> GUI target(CMakeLists):`xq_ui`(40)/`xq_visualization`(107)/`xq_render_widget`(134)/`xq_app_shell`(158)/`xq_app`(191)/`xq_controllers`(251)
> **缺口说明**:无重大缺口。资源层(qss 具体规则、每个 svg)只做概况未逐条列;`SvProjectReader`/各 service 内部实现属非 GUI 层未展开(只标了 GUI 调用点)。

---

## 0. 结论速览

- **架构分层干净**:UI(菜单/工具栏)→ Controller(intent→service→command)→ CommandStack → Scene,六个 stage 可 headless 测试。
- **六个业务阶段 5 个真实接通**(Segmentation/Modeling/Meshing/Flow/AI),**Path 阶段完全 stub**(有 controller 但面板无 run 按钮、参数全 `(void)` 丢弃)。
- **重活全部在 UI 线程同步跑**:全仓 0 处 QThread/QtConcurrent/std::thread(网格/loft/1D 求解都会卡 UI)。
- **无内存管理/驻留/eviction/IGeometrySource**:renderer 直连 payload 逐元素**拷贝**进 VTK;状态栏内存标签是**死显示**("Mem: -- MB")。
- **主要死代码**:`makeDemoVolume()`(全仓无调用)、`XQImageViewer`(仅测试用的早期合成梯度路径)、Data Manager 透明度/颜色/属性控件(永久 disabled)、File/工具栏 Open Workspace + Save(无 connect)。

---

## 1. 主窗口结构(`src/app/XQMainWindow.cpp`)

### 1.1 菜单栏(`buildMenus` cpp:674)

| 菜单 | Action | 快捷键 | 连接 | 槽/状态 |
|---|---|---|---|---|
| **File**(678) | Open Workspace…(679 `openAction_`) | Ctrl+O(681) | **无 connect** | ⚠️ **空壳** |
| | Open Image (.vti)…(682) | — | `openImageFile`(684) | 真实:对话框→`loadImageFromPath`→VtkImageAdapter 解码→MPR + Image 节点入 scene |
| | Open SimVascular Project…(685) | — | `openSvProject`(687) | 真实:→`loadSvProjectFromDirectory`→SvProjectReader→节点合并入 scene |
| | Save Workspace(688 `saveAction_`) | Ctrl+S(690) | **无 connect** | ⚠️ **空壳** |
| | Quit(692) | Ctrl+Q(693) | `QWidget::close`(694) | 真实 |
| **Edit**(700建/`buildEditMenu`1392填) | Undo(1400) | Ctrl+Z(1402) | `undo`(1403) | 真实:`commandStack_->undo()`,仅 attachWorkflow 后存在 |
| | Redo(1405) | Ctrl+Y(1407) | `redo`(1408) | 真实 |
| **View**(703) | Show MPR Crosshair(704,checkable默认on) | — | lambda(708) | 真实:`mprView_->setCrosshairVisible` |
| | Single/Quad View(713,checkable) | — | lambda(716) | 真实:`toggleViewLayout()` |
| | **Language** 子菜单(724) | 中文(728)/English(733) | lambda(737/739) | 真实:`setChineseLanguage`,ActionGroup 互斥 |
| **Tools**(743) | Preferences…(744) | Preferences键(746) | `showPreferencesDialog`(747) | 真实 |
| **Window**(751建/`buildWindowMenu`1014填) | 3 dock toggle(1023/26/29)+工具栏 toggle(1032) | — | Qt内建 | 真实:显隐面板 |
| | Full Screen(1036) | FullScreen键(1039) | lambda(1040) | 真实 |
| | Reset Layout(1048) | — | `resetDockLayout`(1050) | 真实 |
| **Help**(753) | About XQ…(754) | — | `showAboutDialog`(756) | 真实 |

### 1.2 主工具栏(`buildMainToolBar` cpp:759)

| 按钮 | 连接 | 状态 |
|---|---|---|
| Open(768)/Save(770) | **无 connect** | ⚠️ **空壳**(与菜单 Open Workspace/Save 重复呈现) |
| Undo(774)/Redo(777) | `undo`/`redo`(776/779) | 真实 |
| Image(794) | lambda:显 MPR+隐 stageDock(796) | 真实 |
| Path(804)/2D Seg(807)/3D Seg(811)/Model(814)/Mesh(817)/Simulation(820) | `showStagePage(0/1/1/2/3/4)`(806/809/813/816/819/822) | 真实(Path 目标页是 stub;3D Seg 复用 seg 页,注释810) |
| `viewLayoutAction_`(785) | `toggleViewLayout`(788) | 真实,**非工具栏按钮**(独立 action,注释782) |

> AI 阶段(index5)有 stage 页但**无工具栏按钮**(工具栏止于 Simulation)。

### 1.3 Dock 面板(3 个)

| Dock | 内容 | 默认可见 |
|---|---|---|
| **Data Manager**(825) | 搜索框(接 QSortFilterProxyModel,279/851)+场景树(3列,右键删除→`onSceneContextMenu`851)+节点控件区 | 是,左 |
| **Image Navigator**(898) | Loc.(mm)三 locSpin(只读回显)+Axial/Sagittal/Coronal 滑块+spin(接`onNavigatorSliderChanged`284-289)+Time 滑块 | 是,左(与 DataManager 上下分割) |
| **Stages**(`buildStagePanel`407) | stagePanel_(QStackedWidget,6 页) | **否**(671 hide,由工具栏唤出),右 |

Data Manager 节点控件区(855-883)**全部永久 disabled 纯装饰**:opacitySlider_(867 disabled)、colorButton_(872)、propertiesToggle_(882);头文件注释直言(h:196)"disabled until a real renderable node operation is wired"。

### 1.4 状态栏(`buildStatusBar` cpp:997)

| 段 | 内容 | 状态 |
|---|---|---|
| 位置(左) `statusPositionLabel_` | "Ready"/"Slice S:.. C:.. A:.."(1663)/"Seed voxel.."(300) | 真实,随 slider/seed 更新 |
| 内存(右) `statusMemLabel_` | 写死 "Mem: -- MB" | ⚠️ **死显示**:全仓仅 1 处 setText(1384),从不接真实内存 |
| 节点数(右) `statusNodeCountLabel_` | "Nodes: N"(346/1388) | 真实 |

---

## 2. 阶段/工作流(`src/ui/panels/XQStageWidgets.cpp`,`populateStagePanels` 697)

`buildStagePanel`(cpp:407)构造 6 个 provider lambda(420-648)后追加 6 页:

| # | 阶段 | 面板函数 | 主要控件 | run 接线 | 状态 |
|---|---|---|---|---|---|
| 0 | **Path** | `buildPathPage`(220-247) | 仅 hint+status label,**无 run 按钮** | `(void)path;(void)counter;(void)stageChanged`(225-227) | 🔴 **完全 stub**:hint 明写"disabled until real 3D control-point picking";`addPath` 永不触发 |
| 1 | **Segmentation** | `buildSegmentationPage`(251-395) | threshold/regionGrow radio、name、lower/upper spin、Pick Seed、run | run(291)→`threshold()`(354)/`regionGrow()`(376);pickSeed→`seedPickingSetter`(325) | 🟢 接通;⚠️ `aiSegment` 无 UI 入口 |
| 2 | **Modeling** | `buildModelingPage`(399-467) | name、source spin、capEnds、run(Loft) | run(427)→`loft(intent)`(459) | 🟢 接通 |
| 3 | **Meshing** | `buildMeshingPage`(471-541) | name、source spin、surface/volume radio、run | run(501)→`buildSurfaceMesh`(525)/`buildVolumeMesh`(532) | 🟢 接通 |
| 4 | **Flow** | `buildFlowPage`(545-619) | name、case spin、run(Solve) | run(570)→`solve`(604);首次失败 widget 层扩 20000 步重试(605-611) | 🟢 接通 |
| 5 | **AI** | `buildAiPage`(623-693) | name、flow spin、mu/ffr/refP spin、run | run(662)→`analyzeFlow`(677) | 🟢 接通 |

各页 null-guard:controller/provider 为 null 时整组禁用(seg308/model440/mesh509/flow583/ai670)——**未 attachWorkflow 或无活动图像时控件全灰**。

---

## 3. Controllers(`src/ui/controllers/`)

统一模式:持 `(XQScene*, XQCommandStack*)`;方法调 service 静态方法拿 command,`stack_->push`。**全部真实接通,无 stub。**

| Controller | 方法 | 调用 service | push | 被谁用 |
|---|---|---|---|---|
| **PathController** | `addPath`(cpp:16) | `PathService::createPathCommand`(22) | ✅(29) | ⚠️ **Path 页把指针 `(void)` 丢弃,无实际调用点** |
| **SegmentationController** | `threshold`(34)/`regionGrow`(51)/`aiSegment`(68);`commitMask`(17) | `SegmentationService::thresholdMask`(43)/`regionGrowMask`(60)/`AiService::segment`(78)/`createMaskNodeCommand`(25) | ✅(30) | seg 页 threshold(354)/regionGrow(376);**aiSegment 无 UI 调用** |
| **ModelingController** | `loft`(17) | `ContourLoftInputBuilder::buildLoftInput`(29)、`ModelingService::loftSurface`(34)/`capModel`(42)/`createModelNodeCommand`(49) | ✅(55) | model 页(459) |
| **MeshingController** | `buildSurfaceMesh`(43)/`buildVolumeMesh`(63) | `SurfaceMeshService::buildSurfaceMeshCommand`(53)/`VolumeMeshService::buildVolumeMeshCommand`(73,传 `mesher_`) | ✅(59/80) | mesh 页(525/532) |
| **FlowController** | `solve`(16) | `BoundaryConditionService::validateAndBind`(23)、`FlowSolver1D::solve`(30)/`buildFlowResultCommand`(36) | ✅(42) | flow 页(604) |
| **AiController** | `analyzeFlow`(19) | `FlowMetricsService::analyzeFlowCommand`(35) | ✅(42) | ai 页(677) |

- 六个由 `attachWorkflow`(cpp:385-395)创建;volume mesh kernel 依 `XQ_ENABLE_MMG`/`XQ_ENABLE_TETGEN` 选 `TetGenThenMmg`/`TetGenTetMesher`,否则 null(fallback star-shaped)。
- `xq_controllers` 纯 C++(无 Qt/VTK,CMake251),intent→command→scene 全链 headless 可测。

---

## 4. 可视化(`src/visualization/`)

| 类 | 职责 | 谁用谁 |
|---|---|---|
| **XQMprView**(QWidget) | MITK 风格 2×2:MPR 三格离屏渲染→QLabel,3D 格内嵌 XQRenderWidget | MainWindow 中央;每格一个 XQSceneRenderer(cpp:45),3D 格是 XQRenderWidget(cpp:143) |
| **XQRenderWidget**(QWidget) | 交互 3D,封装 QVTKOpenGLNativeWidget+值持 XQSceneRenderer | MPR 3D 格;`renderWidget_=&mprView_->volumeView()`(231) |
| **XQSceneRenderer**(VTK,pimpl) | 单场景多 actor 装配+离屏渲染;`addImageSlice/addPath/addSurface/addVolumeMesh/addFlowResult/addSegmentationMask` | MprView 三格+RenderWidget 持有;`onSceneSelectionChanged` 派发(1913-1959) |
| **XQImageViewer**(VTK,pimpl) | 早期**合成梯度**离屏渲染器(忽略真实 buffer,cpp:89-94) | ⚠️ 仅 `showImage()`(355)用,showImage 仅 test_main_window 调 → **生产 UI 无入口** |
| **XQDemoVolume**(struct+工厂) | 纯演示合成体数据 | struct 被复用为 `activeImage_` 容器;`makeDemoVolume()` **全仓无调用** |

**渲染时机(全同步)**:MPR 三格 `renderAxis`→`renderOffscreenToRgba`→VTK `Render()`(SceneRenderer 560),由 setImage(734)/setSlice(779)/crosshair(826)/seed 点击(886)/layout(811)触发;3D 格 `render()`→`renderWindow()->Render()`(RenderWidget 64),由 `onSceneSelectionChanged` 末 `renderWidget_->render()`(1965)+show/resize 触发。

**resize 合并 ✅ 有**:MprView 70ms singleShot QTimer 标 dirty 轴+`flushResizeRenders`(504/552-563/589-593/898-901);RenderWidget `initialRenderQueued_` flag+`singleShot(0)`(81-93)。注意外部主动 render() 同步无去抖。

**几何进 VTK = 逐元素拷贝,无借用/无 IGeometrySource**:image `*scalar=buffer->scalarAt`(99/117-128)、surface 逐点 SetPoint+逐三角 InsertNextCell+faceId(139-155)、volume 逐点+逐 tet(168-189)、mask 逐前景 InsertNextPoint(491-505)。无 SetArray/ShallowCopy;`onSceneSelectionChanged` `static_pointer_cast` 直取 payload(1918-1959)。

**LOD/分块上传:无。** rg IGeometrySource/acquire/resident/evict/lease/budget 在 GUI 层全 0 命中。

---

## 5. 死代码/演示/无用功能候选

| # | 目标 | 位置 | 判定依据 |
|---|---|---|---|
| 1 | **`makeDemoVolume()`** 工厂 | XQDemoVolume.cpp:29-127 | src+tests **零调用点**;struct 仅作 activeImage_ 容器 |
| 2 | **`XQImageViewer`** 整类(277行) | XQImageViewer.{h,cpp} | 仅 showImage(355)用,showImage 仅 test_main_window 调;渲染合成梯度非真实数据 |
| 3 | **File Open Workspace** | 679 `openAction_` | 无 connect,空壳 |
| 4 | **File Save Workspace** | 688 `saveAction_` | 无 connect,空壳 |
| 5 | **工具栏 Open/Save** | 768/770 | 无 connect(与#3/#4 重复) |
| 6-8 | **Data Manager 透明度滑块/Color 钮/Properties 折叠钮** | 863-868/870-872/878-882 | `setEnabled(false)`,无 connect,占位 |
| 9 | **状态栏内存标签** | 1005/1384 | 写死 "Mem: -- MB",无真实更新 |
| 10 | **Path stage 页** | XQStageWidgets.cpp:220-247 | 无 run 按钮,参数全 `(void)`,明文 disabled |
| 11 | **3D Seg 工具栏钮** | 811-813 | 复用 seg 页,无专属 3D 功能(注释810) |
| 12 | **SegmentationController::aiSegment** | SegmentationController.cpp:68 | 实现完整但无 UI 入口,仅测试调 |
| 13 | **Time 滑块/spin** | 967-982 | range 恒(0,0)+disabled,无 4D 数据驱动 |
| 14 | **Loc.(mm) locSpin×3** | 914-923 | disabled,只读回显不可编辑驱动 |

> 未发现场景树 "Widgets" 演示节点(启动为空 project,XQSceneModel 纯映射真实 scene)。

---

## 6. 线程模型

- **全仓 0 处** QThread/QtConcurrent/std::thread/std::async/QFutureWatcher/moveToThread。
- **重活全在 UI 线程同步,会阻塞 UI 的点**:Meshing 体网格(TetGen/MMG,MeshingController.cpp:73,mesh 页 532)**最可能长阻塞**;Modeling loft(34/42,页459);Flow 1D 求解含 20000 步重试(30,页604/610);AI 分析(35,页677);加载 loadImageFromPath(1480)/loadSvProjectFromDirectory(1553);渲染 render()(1965)。
- 唯一异步手法是 `QTimer::singleShot` 做布局/渲染去抖(401/1166/RenderWidget87),**不移计算出 UI 线程**。

---

## 7. 内存相关

- **无内存显示**:`statusMemLabel_` 唯一 setText 写死 "Mem: -- MB"(1384)。
- **无驻留/eviction/budget/IGeometrySource 入口**(GUI 层 rg 全 0)。
- **renderer 拿几何 = 直连 payload**:`onSceneSelectionChanged` `static_pointer_cast<XQ*Payload>` → `renderer.addSurface/...`(1918-1959),renderer 内部**逐元素拷贝**进 VTK;每次选中先 `clear()`(1914)再全量重建,无缓存/LOD/分块/驻留预算。

---

## 8. 构建/测试

GUI target 见抬头。GUI 测试:

| 测试 | link | offscreen | 断言强弱 |
|---|---|---|---|
| **test_main_window** | xq_app_shell(766) | offscreen(945) | **强**:树行数==节点数(141)、rgba==w*h*4(151)、真实.vti sliceCount 精确 100×512×512(311)、SV 工程各域节点数精确(328-347)、path add→undo→redo(365-380)、pathAction→弹 stageDock(246)、click→seedPicked(286);**弱**:controller/mpr/action 仅非空(197/202/241) |
| **test_i18n_resources** | xq_app_shell(788) | offscreen(945) | **强**:.qm 存在、&File→文件、Axial→轴位、En/Zh 切换菜单变化(86-96);**弱**:translator 非空(44) |
| **test_ui_font_glyphs** | Qt6::Gui/Widgets(782) | offscreen(921) | **强**:msyh 对 轴/中/文/A/x 有真 glyph(63-69) |
| **test_workflow_integration** | controllers+io+adapter_vtk(478) | 无(纯C++) | **强**:真实0007跑完6阶段,每push+1(449)、全量undo回初态(462)、redo(476) |
| **test_workflow_controllers** | xq_controllers(469) | 无 | **强**:6 controller intent→类型+关系数+undo/redo对称+错误路径 |
| **test_scene_model** | xq_ui+Qt6::Core(738) | 无(QCoreApp) | **强**:行数==节点数、列头、DisplayName/DomainType/stale 精确、refresh 反映新节点 |
| **test_image_viewer** | xq_visualization(744) | VTK offscreen | ⚠️ **全弱**:仅 ok+width==256+height==192,**不查像素**(对象是死代码 XQImageViewer) |
| **test_scene_renderer** | xq_visualization(754) | VTK offscreen | 中:actorCount/pointCount 精确(mask==前景数);⚠️ **像素弱**:全 `any_non_black`(≥1 非黑即过,104/139/171/204/251),null buffer 仅断言不崩(262) |

**易假绿点**:test_image_viewer(纯尺寸+死代码路径)、test_scene_renderer 的 `any_non_black`(单像素即过)、test_main_window controller/mpr 非空断言。

**resources 概况**:`xq_resources.qrc` 23 条(图标+xq.qss+i18n/xq_zh_CN.qm);`icons/` 21 个 svg;`xq.qss` 单一 light theme(main.cpp:61 加载);`xq_zh_CN.ts` 149 条 `<message>`,默认中文运行时可切;字体 msyh.ttc 由 main.cpp installUiFont 注册(非 qrc)。

---

### 附:两个对话框(均真实接通)
- **XQPreferencesDialog**:4 项偏好(单视图/十字线/切片步长/状态栏坐标)落 QSettings("XQ"/"XQ"),Accept→`applyPreferences` 应用到 MprView。
- **XQAboutDialog**:静态信息;版本 Qt 6.7.0/VTK 9.3.0/Build Release 为**写死字符串**(46-48),非运行时探测。

---

**盘点完成。** 关键定性:GUI 骨架/工作流/i18n/命令栈是真材实料且有强测试;死代码带(makeDemoVolume、XQImageViewer、Data Manager 装饰控件、Open/Save 空壳、Path 阶段)、内存显示死值、几何全量拷贝无 LOD/驻留、重活全在 UI 线程同步——是后续清理与性能改造的重点。