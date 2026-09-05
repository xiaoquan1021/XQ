# XIAOQUAN 工作区探索证据（2026-09-05）

## 范围与状态

- 探索范围：`C:\Users\OCEAN\Desktop\XIAOQUAN` 根、`XQ` 主 C++ 原型、`XQ1` MITK/BlueBerry 插件树、`XQrebuild` 重建树；绕过 `build*`、`.git`、`Externals` 和影像/网格大文件。
- 根 `AGENTS.md` 要求 Windows、先读配置/提交、最小改动；本次只读并只新建本笔记。根 git status 有未提交 `.trellis/tasks/...` 文档（保留）。根最近 5 commit：`039c51d docs: P3-4 区域生长执行简报 + P3-5 水平集交接文档`、`fdaff1c docs: P3-3 断面阈值执行简报 + P3-4 水平集/区域生长交接文档`、`b0b3ebf docs: P3-3 断面阈值自动分割交接文档`、`eb34e99 docs: P3-2 执行简报 + SV 三阶段(Path/Seg/Mesh)方案调研`、`4da9366 docs: 归档父任务 07-06-along-path-contouring 规划文档(P3 六批计划)`。`XQ1` 最近：`bf143f4 Merge feature/windows-blueberry-port into main`、`cf13c12 Add Windows run command entrypoint`、`7cd9900 Port legacy BlueBerry workbench to Windows`。`XQrebuild` 最近：`1c56820 feat: XQ-M2B-001 SimVascular 项目发现 reader(SvProjectReader)+ L1 fixture 验收`、`0bb45a7 feat: XQ-M3-UI-3 最小集成 app 外壳`、`71ad1f6 feat: XQ-M3-UI-2 offscreen 图像 viewer 适配器`。
- 未执行构建、测试、重计算；因此运行态和性能未验证。

## 事实证据

### 用途、对象、算法

- `XQ/src/services/flow/FlowSolver1D.h:18-31` 明确是无 Qt/VTK/ITK/CFD/Python 依赖的 CGS 单一维血流 reduced-order solver；状态 `(A,Q)`，质量方程 `dA/dt+dQ/dx=0`、动量方程含 Poiseuille 摩擦，出口为三元 RCR Windkessel；`solve` 输出按 segment 的 Q/P/A 时间序列（同文件 `:78-80`）。
- `XQ/src/services/flow/FlowSolver1D.cpp:21-22,273-333` 使用显式 Lax-Friedrichs、弹性壁 beta=`1.0e6`、CFL safety=`0.9`；初始/入口峰值速度估计，超 CFL 返回 `CflViolation`，记录 observed max CFL。
- `XQ/src/services/flow/FlowSolver1D.h:83-103` 有两个可闭式核验路径：变截面稳态 Poiseuille 梯形积分；RCR backward Euler，并以 `P(t)=Q0(Rp+Rd)+(P0-Q0(Rp+Rd)) exp(-t/(Rd*C))` 比较。
- `XQ/src/services/segmentation/SegmentationService.h:39-76` 是纯域服务；threshold（闭区间标量阈值）、6-connected region grow、largest component、注入式 AI backend；错误用 Status，不抛异常。`SegmentationService.cpp:110-144,146-231` 可核验阈值和 flood-fill 实现。
- `XQ/src/services/path/CenterlineFrameService.h:10-17` 将路径重采样 frame 作为 contour placement、oblique slicing、loft 单一来源，采用 rotation-minimizing/parallel transport 防 normal 翻转。`XQ/src/services/path/PathService.h:18-24,47-79` 路径命令支持创建/控制点编辑/重采样并保持 source 绑定。
- `XQ/src/services/meshing/VolumeMeshService.h:19-35` 当前默认是 centroid star tetrahedralization：每表面三角形和质心成 tet；明确承认只对 centroid 星形域正确，弯曲主动脉会出现 inverted/degenerate tets；TetGen/MMG 是 future work。`XQ/src/services/meshing/VolumeMeshService.h:70-81` 可注入 `ITetMesher`，closed 2-manifold 检查、边界 face metadata 传递。

### 3D 关联与集成边界

- `XQ/CMakeLists.txt:241-278` 将 services/controllers 设为纯 C++，注释写明 services 无 Qt/VTK/ITK/plugins，controllers 仅 `xq_core+xq_services`；VTK/Qt 只在 adapters/visualization/app shell。
- `XQ/CMakeLists.txt:80-120,148-169` 实际依赖 VTK 9、Qt6；VTK adapters 读 MDL/MSH/VTI。`XQ/CMakeLists.txt:517-529` 端到端测试注释为真实 0007：image -> path -> seg -> model -> mesh -> flow -> ai，支持 undo/redo。
- `XQ1/Code/Source/ImagingWorkbench/Plugins/PluginList.cmake:2-15` 已列 core/datamanager/workspace、preprocess、centerline、lumenanalysis、volumesegmentation、anatomymodeling、volumemeshing、flowanalysis、reducedflow、multiphysics 等插件；但实现是 MITK/BlueBerry，不是 3D Slicer。`XQ1/.../org.xq.imaging.volumesegmentation/src/internal/xq_MitkSegmentationView.cxx:1-25,380-590` 使用 `QmitkAbstractView`、`mitk::Image`、LabelSetImage，并有 threshold/region-grow/level-set UI。
- 全树搜索未发现 `vtkSlicer*`、`qSlicer*` 或 Slicer module/CLI 结构；故直接集成 Slicer 尚未存在，需另做 adapter/模块包装（推断）。

### 验证、数据、完成度

- `XQ/tests/services/flow/FlowSolver1DTest.cpp:~30-120,~130-250`（以文件中注释分段为准）测试 Poiseuille、RCR 闭式误差、CFL violation、transient consistency、finite/positive area、payload deep-copy、command undo；测试输出断言 Poiseuille 相对误差 `<1e-2`、RCR pointwise `<1e-2`。
- `XQ/tests/services/flow/FlowIntegrationTest.cpp:73-149,179-182` 读取真实 `0007_H_AO_H/Segmentations/aorta_final.ctgr` 与 `flow-files/inflow_1d.flow`，从轮廓 shoelace 面积构造 A0，RCR 固定 `[106,0.00068483,1784]`，运行 2 cycles；检查 converged、finite Q/P/A、压力数量级。测试代码存在，但本轮未运行。
- `XQ/CMakeLists.txt:540-564` ONNX backend 默认 OFF，需 onnxruntime；`:568-600` TetGen 默认 OFF；`:605-641` MMG 默认 OFF 且要求 TetGen，运行还需 `mmg3d.dll`。因此 AI 和高质量体网格目前是可选/未默认可复现能力。
- `XQ/src/services/meshing/VolumeMeshService.h:25-32` 是最明确科学缺口：星形剖分对弯曲主动脉退化；真实 TetGen/MMG adapter 尚未默认启用。`XQ1` 侧仅有 MITK UI/插件清单；未找到实证 benchmark 或临床验证协议。

## 对选题的约束性推断（供主代理判断）

1. 最具科学贡献潜力的接口在中心线 frame + 轮廓/面积 + 1D 血流耦合：数据链已可从真实 0007 轮廓/波形走通，但现有 solver 是一阶显式、beta 固定、RCR 简化，适合研究网格/时间步收敛、参数反演/不确定性量化、分叉/柔性壁扩展；GUI 只是承载层。
2. 体网格质量/拓扑是明确缺口，可研究带边界标签保持的质量约束 tetrahedralization 或误差驱动自适应；必须以 inverted/degenerate tet 率、质量分布、守恒/压力误差等量化，不能把接入 TetGen/MMG 本身当创新。
3. 直接做 Slicer 插件的可行集成路线是复用纯 C++ domain services，通过 MRML/VTK image/segmentation/model/mesh/markups 与 undo/redo adapter；当前证据只支持“已有 MITK 版本、无 Slicer 版本”，尚未验证 Slicer API、编译或运行。
4. segmentation threshold/region-grow 已是基础能力；科学选题应转向血管几何不确定性传播、中心线-截面 frame 稳定性、从分割到流量/压力结果的可追溯误差，而非再包一层 GUI。

## 待主代理亲读的奠基文档（最多 3）

- `C:\Users\OCEAN\Desktop\XIAOQUAN\AGENTS.md`（约 70 行）：项目/协作约束，已定位并读指令部分。
- `C:\Users\OCEAN\Desktop\XIAOQUAN\XQ\README.md`（体量未细读）：XQ 原型定位、构建/运行说明；仅定位，按要求由主代理完整阅读。
- `C:\Users\OCEAN\Desktop\XIAOQUAN\XQrebuild\README.md`（体量未细读）：重建版架构/验收定位；仅定位，按要求由主代理完整阅读。

## 未覆盖/存疑

- 未运行测试，不能声称通过；未审计所有 XQ1 插件（仅清单和 segmentation 代表实现）；未对真实影像内容或患者身份作任何输出。
- `XQ`、`XQ1`、`XQrebuild` 各自是嵌套 git 仓库；根 status 的未提交任务文档显示工作区有人改动，未触碰。
