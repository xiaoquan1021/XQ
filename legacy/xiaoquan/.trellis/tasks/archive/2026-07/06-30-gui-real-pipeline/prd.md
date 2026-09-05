# GUI 接真实影像管线

> 子任务 `06-30-gui-real-pipeline`。与父任务 `06-29-xq-rebuild` 的 M8b-1 架构主轴**并行**(父 prd「UI 并行工作」明确允许)。
> 本任务把已存在的真实管线(M0–M7)接到前端,让用户能打开真实医学图像、跑通管线、看到真结果。**不造新算法、不碰大规模几何 handle、不动渲染器内核。**

## Goal

让 XQ 前端从"合成 demo 壳"变成"能跑真实影像管线的工作台":用户打开真实 `.vti` 图像或 SimVascular 项目 → 中央 MPR 显示真实切片 → 通过 stage 面板跑 路径/分割/建模/网格/血流/AI → 结果节点进场景树、选中后渲染到 3D 视图。沿途修掉阻碍"看见"的两个 GUI 壳缺陷(中文字体 tofu、四视图第四格)。

## Continuation Decision (2026-07-01)

用户确认"架构重构继续"收敛到已有 in-progress 任务 `06-30-gui-real-pipeline`,不另开 M9b' 大规模增强任务。本轮目标是把已完成的架构主线落成可用 GUI 工作台:优先补齐真实 GUI 端到端链路、真机可视化确认和必要壳缺陷,使 XQ 重构从"架构完成"推进到"用户可运行真实样例并看到结果"。

AC3 允许使用已加载 SimVascular 项目中的现成 path / contour / flow 输入来跑通真实链路;本轮不实现 GUI 里的 3D Path 控制点手动拾取/编辑。R2b 的 MPR 切片 seed 点选仍在范围内,因为它是区域生长分割的最小必要输入。

## Background / 已确认事实(代码证据)

- **真实管线后端已存在且端到端跑通**:`XQ/tests/app/WorkflowIntegrationTest.cpp` 用真实数据(`0007 aorta` `.vti`、主动脉 `.ctgr`、入流 `.flow`)跑通 `image→path→segmentation→modeling→meshing→flow→AI`。后端不是空壳。
- **真实读取器现成**:`VtkImageAdapter::loadVtiWithBuffer(path, outImage, outBuffer)`(带体素 buffer,分割算法需要真实体素值);SimVascular 项目读取器 `SvProjectReader` / `PTHPathReader` / `CTGRContourReader`(`XQ/src/io/project/`)。**无 DICOM 支持**(不在本任务范围)。
- **渲染能力全部现成**:`XQSceneRenderer`(`XQ/src/visualization/XQSceneRenderer.h`)已实现 `addImageSlice / addPath / addSurface / addVolumeMesh / addFlowResult / addSegmentationMask` —— 6 种几何全可渲染。
- **选中-dispatch 现成**:`XQMainWindow::onSceneSelectionChanged`(`XQMainWindow.cpp:275–305`)已能按域 dispatch 渲染 SurfaceModel / Mesh / Path / SegmentationMask / FlowResult。
- **controller→service 现成**:`PathController::addPath` → `PathService::createPathCommand` → 命令栈(`XQ/src/ui/controllers/PathController.cpp`)。其余 controller 同构。
- **缺口只在输入端接线**:
  - 主窗口无"打开 .vti / 打开 SV 项目"入口;中央 MPR 现在 load 的是 `makeDemoVolume()` 合成体(`XQMainWindow::loadDemoVolume`)。
  - 三个 stage 页(Segmentation/Modeling/Flow)灰着"待接线":它们的 Intent 需要从**当前选中的真实图像/轮廓/算例节点**取数据,这个取数接线没做(`XQStageWidgets.cpp` 里 `run->setEnabled(false)` + "wiring to follow")。
- **GUI 壳遗留缺陷(用户可见,本任务前置修复)**:
  - 中文字体 tofu:`app.setFont(QFont("Microsoft YaHei",9))` + qss `font-family` 仅靠 family 名匹配,offscreen/某些后端不扫系统字体 → 全方框。根因待真机确认;根治方向:`QFontDatabase::addApplicationFont(":/fonts/msyh.ttc")` 或系统路径显式加载。
  - 四视图第四格(3D 黄框)曾白底,已加 `#000000` 兜底(`XQMprView.cpp styleVolumeFrame`),像素确认纯黑;但用户报"仍缺第四格",需真机复核是布局问题还是用户看的是旧图(`xq_ui_zh.png` 是改字体前那版)。

## 架构约束(不可违反,来自父任务 prd 铁律)

- 新增 UI **不得**直接依赖 payload vector 或具体大规模 `GeometryHandle`;UI 只经 **application commands + `NodeId` + `AssetId`** 操作状态。
- **不写大规模渲染消费者**(2000万三角/1000万tet 那条路径等 M8b-1/M9a 接口冻结后再做)。本任务只用**现有小规模渲染路径**——它一直是合规的。
- 依赖方向不变:`app → services → adapters → io → core`。
- 不动 `XQSceneRenderer` 内核、不动 M0–M7 算法。

## Requirements

### R1 — 打开真实图像入口
- File 菜单加"打开图像(.vti)"+"打开 SimVascular 项目":文件对话框 → 调 `loadVtiWithBuffer` / `SvProjectReader` → 把图像/路径/轮廓/波形作为带 payload 的节点 seed 进场景树(经 command,保持可撤销)。
- 加载成功后,中央 MPR 用真实图像替换合成 demo(`setImage` 真实 volume+buffer),三轴切片真实显示。
- 加载失败给明确错误提示(reader 的 LoadStatus → 用户可读消息),不崩、不静默。

### R2 — stage 面板接真实数据
- 把当前选中节点(图像/轮廓/算例)作为 stage Intent 的数据源:Segmentation 从选中图像取 volume+buffer;Modeling 从选中轮廓组取数据;Flow 从选中算例取数据。接通后这三页从灰态"待接线"变为可执行。
- 执行经现有 controller→service→命令栈;结果作为新节点进场景树(可撤销)。
- 不伪造指针:取不到必需输入时,按钮保持禁用 + 明确提示"请先选中 X"。

### R2b — 分割两种模式(阈值 + 区域生长)
- **阈值分割**:选中图像 → 填上/下阈值 → `SegmentationController::threshold`(`SegmentationService::thresholdMask`)→ 真掩膜节点入树。纯表单,无需切片交互。
- **区域生长**:`SegmentationController::regionGrow`(`SegmentationService::regionGrowMask`)需要**种子体素坐标**。要在 MPR 切片上**鼠标点选**一个像素 → 转换为体素 (i,j,k) 坐标 → 填入 RegionGrowIntent.seed。涉及:切片 QLabel 的鼠标事件捕获、点击像素→切片内坐标→体素索引的映射(考虑切片缩放/居中偏移/当前轴与 slice index)。
- 种子点选要给用户反馈(在切片上标记选中位置);seed 不满足阈值时按 service 的 `SeedNotInThreshold` 给明确提示。

### R3 — 结果渲染(复用现有)
- 选中结果节点(分割/曲面/体网格/血流)→ 复用 `onSceneSelectionChanged` 现有 dispatch 渲到 3D 格。本要求基本现成,需确认每种结果节点携带的 payload 与 dispatch 分支匹配、真实跑出的产物能被选中渲染。

### R4 — 中文字体根治
- 汉字与字母都必须正常渲染(非 tofu),中/英切换都正确。
- 根治用显式字体加载(`addApplicationFont` 加载 `msyh.ttc`,或确认系统 family 在目标运行环境命中),不只靠 qss family 名。
- 验证手段不得只靠离屏直方图(历史教训:看不出 tofu);需真机目视或字形可用性程序化检查。

### R5 — 四视图第四格
- 四格(Axial/Sagittal/Coronal/3D)在 Quad 模式必须全部可见、各占一格、无缺格无重叠。
- 真机复核当前状态:若已修(3D 格黑底兜底)则确认收尾;若仍缺,定位是布局 span 残留还是 VTK 控件尺寸问题并修。

## Out of Scope

- DICOM 读取(无现成支持,需新写适配器)。
- 大规模可视化(LOD/分块/渐进上传/大网格拾取)——属父任务 M9b。
- 分割掩膜/轮廓**叠加到 MPR 二维切片**(本期只在 3D 格渲染结果;切片叠加为后续增强)。**例外**:区域生长的种子点选会用到切片鼠标拾取(R2b),但只做"点选一个体素"的最小拾取,不做掩膜/轮廓的切片叠加显示。
- 新算法 / M0–M7 算法升级。
- 渲染器内核改写(属 M9a 上传核重写)。
- 3D 视图中的交互式拾取/控制点编辑(path 控制点 picking 仍"wiring to follow")。**仅** R2b 的 MPR 切片种子点选在范围内。

## Acceptance Criteria

- [ ] AC1:GUI 打开真实 `.vti`(`C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/Images/OSMSC0090-cm.vti`,32MB 主动脉 CT,已确认在盘),中央 MPR 三视图显示该图像的真实切片(非合成 demo)。
- [ ] AC2:GUI 打开 SimVascular 项目,图像+路径+轮廓+波形节点出现在场景树。
- [ ] AC3:从 GUI 触发 路径→分割→建模→网格→血流→AI 中至少一条真实链路(用真实选中输入,经 command/service),结果节点进场景树且可撤销。
- [ ] AC3b:分割两种模式都能从 GUI 跑出真掩膜——阈值(纯表单)+ 区域生长(在 MPR 切片点选种子体素,seed 正确映射到 (i,j,k))。
- [ ] AC4:选中结果节点,3D 格渲染出对应几何(曲面/体网格/血流之一),非空白。
- [ ] AC5:中文字体真机目视正常(无方框),中/英切换文字均正确;字体可用性有可复现的验证(非仅直方图)。
- [ ] AC6:Quad 模式四格全部可见、布局正确。
- [ ] AC7:构建 Release 绿 + 全量 ctest 绿(现有 49 个不退化);新增接线若可测则补测试。
- [ ] AC8:不违反架构铁律(无大规模 handle 依赖、UI 只经 command+NodeId+AssetId、不动渲染器内核)——代码评审确认。

## 数据 / 验证环境(已确认在盘)

- 真实样本:`C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/`
  - 图像:`Images/OSMSC0090-cm.vti`(32MB 主动脉 CT)
  - 轮廓:`Segmentations/aorta_final.ctgr`
  - 波形:`flow-files/inflow_1d.flow`
- CMake 已定义 `XQ_TEST_VTI_PATH` / `XQ_CTGR_DIR` / `XQ_FLOW_DIR` 指向上述路径(`XQ/CMakeLists.txt`)。`WorkflowIntegrationTest` 即用此数据,AC1/AC3 可直接复用。
- GUI 在 `feat/gui-mitk-layout` 分支的 worktree `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`;构建脚本 `build_gui_wt.bat`,offscreen ctest 现 49/49 绿。
