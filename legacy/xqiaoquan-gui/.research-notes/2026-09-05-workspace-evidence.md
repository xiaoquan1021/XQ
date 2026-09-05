# XQIAOQUAN-gui 工作区证据笔记

日期：2026-09-05（Asia/Shanghai）  
范围：`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui`；只读勘察代码、配置、测试与版本状态。  
未执行测试、构建、安装、下载或重计算。

## 状态与骨架

- `git status --short` 显示大量既有修改/删除及新增文件，涉及 `.trellis`、`XQ/CMakeLists.txt`、GUI、ITK 适配器、测试和第三方目录；本次未触碰。最近 5 commit：`8fddca4 feat: audit vascular dependency foundation`、`746cd66 feat: add Shell A end-to-end acceptance gate`、`2094434 chore(task): archive 07-10-google-earth-shell-a`、`134ec65 feat: make Flow execution capability optional`、`3a7fcf4 chore(task): archive 07-10-shell-a-flow-smoke`。
- 项目根 `AGENTS.md` 规定 Trellis 工作流，且禁止 Claude/Claude 子代理；适用于本目录。核心 README 只有 16 行，定位为“Medical image processing application - integrated rebuild with clean architecture”，列出 `src/core`、测试和不依赖 MITK/BlueBerry/CTK 的架构原则（`XQ/README.md:1-16`）。该 README 属奠基文档，未作完整代总结。
- 最多三份奠基文档候选（已定位、未完整阅读）：`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\README.md`（16 行）、`...\.trellis\spec\XQ\architecture\flow-capabilities.md`（未改前行数需以当前文件计数核验）、`...\.trellis\spec\XQ\core\source-interface.md`（同上）。

## GUI 到核心算法数据流

- `XQWorkflowSession` 构造 ITK 预处理器、ROI prior reader、自动分割器、中心线骨架化器，并在 `attachControllers` 注入 `SegmentationController`、`PathModuleController`、`CenterlineBController`；流控制器仅在 `XQ_ENABLE_FLOW` 且 capability 可用时创建（`XQ/src/app/XQWorkflowSession.cpp:34-40,59-100`）。
- GUI 建有 Threshold、Region grow、Level set 三个截面方法按钮；level-set 预览从当前重切片灰度和 seed 读取，滑条映射 200–1200 次迭代，再调用 `levelSetSegmenter_->segment`，注释明确其为 SimVascular ITK vascular two-phase level set（`XQ/src/app/XQMainWindow.cpp:4043-4088`）。另有“Automatic vessel segmentation”页并调用 `SegmentationController::prepareAutomaticVesselSegmentation`（`XQ/src/ui/panels/XQStageWidgets.cpp:739-872`）。
- 自动血管分割控制器硬编码 profile 输入：CT image/source → `IVascularPreprocessor::run(..., portalVenousCtPreprocessProfileV1())` → 两个 ROI 文件（Organ/CoarseVessel，来源字符串 `TotalSegmentator` 2.15.0）→ `IAutomaticVesselSegmenter::run(..., portalVenousCtAutomaticSegmentationProfileV2())` → mask node + source relation（`XQ/src/ui/controllers/SegmentationController.cpp:71-141`）。
- 中心线 B 链路为 mask node → `ICenterlineSkeletonizer3D::run` → `CenterlineBGraph::build`（服务代码 217 行以后；服务输出 path/profile/snapshot/geometry smoke）→ controller 以 command stack 原子提交。controller 先校验 project open、源 domain、stale、asset fingerprint 和 payload，再在 compute 前后做 source guard（`XQ/src/ui/controllers/CenterlineBController.cpp:109-150,153-186,274-420`）。

## 数学/生物模型与契约

- ITK 预处理算法标识为 `itk.curvature-anisotropic-diffusion.multiscale-hessian-objectness`，版本 `xq.itk-vascular-preprocess.v1`；验证 LPS、正间距、方向行列式、单分量、标量类型、源元数据与 voxel bytes，且 diffusion timestep 受 spacing 稳定上限约束（`XQ/src/adapters/itk/ItkVascularPreprocessor.cpp:434-538`）。随后执行 curvature anisotropic diffusion、1D objectness、多尺度 Hessian vesselness，sigma 以 mm 传入并保留几何（`:612-667`）。
- 自动分割实现是物理 ROI 膨胀 + signed distance 初始化 + gradient magnitude/reciprocal edge potential + SimVascular/ITK GAC level set；记录迭代数、RMS change、收敛、profile/上游 fingerprints（`XQ/src/adapters/itk/ItkAutomaticVesselSegmenter.cpp:1023-1168,1317-1433`）。测试源码虽构造三根合成管、检查几何/指纹/重复确定性/取消/错误路径，但本轮未执行，不能称通过（`XQ/tests/adapters/itk/test_itk_automatic_vessel_segmenter.cpp:344-622`）。
- 中心线骨架化明确使用 `ITKThickness3D.BinaryThinningImageFilter3D`，锁定版本 `v5.3.0@36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb`；半径来自 `itk::SignedMaurerDistanceMapImageFilter`，启用 `UseImageSpacing(true)`、非平方距离、inside positive，要求 UInt8 二值 mask、LPS、骨架半径正且有限（`XQ/src/adapters/itk/ItkCenterlineSkeletonizer3D.cpp:143-160,163-272`）。
- Vessel profile 采样把半径转面积 `pi*r^2`，单位 LPS/mm/mm²，样本质量固定 `Accepted`，并做 validator、path/profile SHA-256 fingerprint、snapshot 与 geometry smoke（`XQ/src/services/path/CenterlineBService.cpp:277-353`）。这提供可复现 lineage，但“Accepted”是实现赋值，未见独立质量估计证据。
- 1D flow solver 明确包含 steady Poiseuille `dP/dx=-(8*pi*mu/A^2)Q`、三元 RCR Backward Euler 和弹性壁 Lax-Friedrichs；`beta=1e6` 是手工固定值，CFL 违反直接失败，面积非正时用 `A0*1e-3` floor（`XQ/src/services/flow/FlowSolver1D.cpp:74-107,110-155,195-205,243-249,276-318`）。数学接受门存在，但生理参数校准/网格收敛/实验数据验证未从代码证实。

## 软件关联、未实现能力与科研不确定性

- CMake 注释表明 ITK 适配器承载 SimVascular vascular two-phase level set，公共 core/services 不链接 ITK；VTK 用于渲染/数据适配，TetGen 为可选研究构建。TetGen adapter 以三角面 PLC、face marker、`tetrahedralize("pq..a..nnQ")` 生成四面体与边界 face 记录（`XQ/CMakeLists.txt:159-203,1253-1267`; `XQ/src/adapters/tetgen/TetGenTetMesher.cpp:1-9,73-128,159-208`）。README 明确无 MITK/BlueBerry/CTK；未发现直接 3D Slicer 插件 API。
- ONNX 后端是 GUI/接口可见但核心未实现的明确例子：`segment`、`identify`、`predict` 在 ONNX 开启或关闭分支均返回空/诊断，注释为 `TECHNICAL DEBT ... implement real ... inference`（`XQ/src/adapters/onnx/OnnxBackend.cpp:40-70,89-113`）。
- 中心线 B 的骨架是体素 thinning 后基于图的主路径选择；代码证据未显示与金标准 centerline、分支拓扑或人工标注的比较。自动分割已有合成测试和 lineage，但真实病例外部数据、跨设备/spacing 泛化、参数校准、独立分割指标与不确定性量化均未在本次代码范围证实。血管流动模型固定 wall `beta`、简化 1D 轴向均匀网格且用数值 floor，需将稳定性、守恒、收敛及临床参数作为独立研究核验。

