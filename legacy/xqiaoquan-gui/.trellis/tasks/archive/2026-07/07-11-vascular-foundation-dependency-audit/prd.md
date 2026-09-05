# 血管地基：依赖 ABI 与许可审计

> Parent: `07-11-vascular-imaging-foundation`
>
> Status: `planning`
>
> 本 child 只在证据充分后把依赖从“已安装”提升为“可采用”。它不交付分割质量，也不把 probe 通过冒充父任务完成。

## Goal

为后续三维 ITK 血管管线选择一套可复现、ABI 一致、许可结论明确的 Windows C++ 依赖基线，并用真实编译/链接/运行探针证明关键模块确实可用。

审计对象：

- VTK 9.3.0
- ITK 5.4.0
- GDCM 3.0.10
- ITKVtkGlue 及所需 ITK modules
- vtkvmtk 最小 C++ 中心线模块（候选）
- 真实三维 skeletonization fallback 依赖（候选）
- TetGen 实际版本/本地修改/许可
- MMG 5.3.9

## Confirmed Baseline

- `Externals/externals.manifest` 锁定 VTK 9.3.0、ITK 5.4.0、GDCM 3.0.10、MMG 5.3.9，但未登记 vtkvmtk 或 TetGen。
- `XQ/CMakeLists.txt` 使用 C++17；VTK 按 COMPONENTS 查找，ITK 当前使用宽泛 `find_package(ITK 5.4 REQUIRED)` + `ITK_USE_FILE`。
- ITK 安装包含 vesselness、形态学、距离图和 ITK-VTK bridge 相关头/模块；未发现 `BinaryThinningImageFilter3D`/Thickness3D。
- vtkvmtk 当前未安装、未进 manifest、无 XQ adapter。
- TetGen header 报告 1.5，CMake 注释称 1.5.1，仓内 LICENSE 为 AGPLv3；必须核对来源与分发影响。
- TetGen/MMG adapter 已存在但默认 OFF，现有 kernel 测试以程序生成弯管为主。
- 产品运行时禁止 Python、MITK/Slicer/CTK 整机和 vmtk SuperBuild。

## Requirements

### R1 - 单一可追溯依赖清单

- 对每个依赖记录官方名称、精确版本/commit、下载源、source/install 路径、checksum、license、local patch、CMake package/target 和 runtime DLL。
- manifest、安装目录、CMake cache、二进制版本和源码声明不一致时，以核对结果明确修正，不能选一个方便的说法。
- 未采用的已安装 MITK/Python 等依赖也要记录为“存在但禁止进入产品 runtime”，防止传递依赖误入。

### R2 - Windows ABI 一致性

- 核对 x64、MSVC toolset、C++17、CRT、iterator/debug level、Release/Debug、静/动态库形态。
- 所有实际采用库必须与 XQ 的 Release 配置可链接；禁止 Debug/Release 或 `/MT`/`/MD` 混链。
- 记录 imported target 的真实 location/configuration，不以目录名推断 ABI。

### R3 - ITK 模块与最小执行探针

- 将后续所需能力映射到明确 ITK modules/components：DICOM IO、3D diffusion、Hessian objectness/multiscale、morphology/components、distance map、ITKVtkGlue。
- 编译并运行最小 C++ 探针，证明各能力真实实例化/执行；仅有头文件不能通过。
- 确认 ITKVtkGlue 使用同一 VTK 9.3.0，不被另一套 VTK package 覆盖。
- 明确真实 3D skeleton fallback 的来源；2D thinning 不能当作 3D 能力。

### R4 - vtkvmtk 有界可行性探针

- 从官方/可信上游锁定最小 C++ source revision、license 和 VTK 9.3 兼容修复来源。
- 禁止 SuperBuild、Python、第二套 ITK/VTK。
- 在同一 MSVC/VTK 9.3 下编译最小中心线模块并运行；至少覆盖一个真实闭合血管表面或权威 SimVascular 样例。
- 若出现不可复现 crash、ABI 失配、许可不接受或 patch 风险过高，记录证据并拒绝该 backend；不无限修补。

### R5 - TetGen/MMG 许可和真实 ON 构建

- 查清 TetGen 精确版本、本地 SimVascular patch、LICENSE 适用范围和目标研究/分发方式的兼容性。
- 查清 MMG 5.3.9 headers/lib/dll 是否来自同一构建及其 LGPL obligations。
- 用 `XQ_ENABLE_TETGEN=ON`、`XQ_ENABLE_MMG=ON` 完成独立 Release configure/build/test，确认应用实际选择该 kernel。
- 许可未关闭时可做技术 probe，但结论必须标为研究/评估，不得标为可分发生产后端。

### R6 - 可复现构建和失败可诊断

- 形成不依赖隐式 PATH/注册表的 configure/build/test 配方。
- 新 build tree 与默认 build tree 隔离；全量 CTest 串行运行。
- 缺依赖、错版本、错 ABI 或 DLL 缺失必须在 configure/load 阶段清晰失败，禁止静默回退到 legacy/star backend 后报告绿色。

### R7 - 架构与运行时禁令

- 外部类型只进 adapter/private implementation，公开 API 零第三方类型。
- 最终应用依赖扫描不得引入 Python runtime、MITK/Slicer/CTK workbench 或 vmtk SuperBuild 产物。
- 本 child 不修改科学算法通过线，也不选择真实 CTA 数据集。

## Acceptance Criteria

- [ ] AC1：生成完整 dependency lock/audit report，每个依赖的版本、source、hash、license、patch、CMake target、DLL 和采用/拒绝结论可追溯。
- [ ] AC2：记录并验证 x64/MSVC/C++17/CRT/Release ABI；无已知混链项被标为通过。
- [ ] AC3：ITK 3D diffusion、multiscale Hessian objectness、component/morphology、distance map、ITKVtkGlue 最小 probe 在干净 Release build 中真实编译并执行。
- [ ] AC4：确认 ITKVtkGlue 和 XQ 使用同一 VTK 9.3.0；倾斜 direction 的 bridge probe 数值守恒。
- [ ] AC5：vtkvmtk probe 得出二选一的书面结论：`accepted`（真实 surface 运行稳定）或 `rejected`（附可复现证据和 fallback 入口）；没有“装了以后再看”。
- [ ] AC6：三维 skeleton fallback 的具体依赖和 license 被选定并可编译，或明确记录为当前阻塞；2D filter 不得通过。
- [ ] AC7：TetGen 精确版本/patch/license 结论明确；MMG 构建一致性明确；ON/ON Release build/test 实际运行对应 adapter。
- [ ] AC8：可复现命令从新 build tree 执行成功；错误依赖配置产生稳定失败，不静默选 legacy backend。
- [ ] AC9：依赖/符号扫描证明产品目标未引入 Python runtime、MITK/Slicer/CTK 整机，公开头未泄漏第三方类型。
- [ ] AC10：完成记录只声称“依赖地基已审计/可构建”，不声称真实 CTA 分割、中心线质量或父任务完成。

## Out of Scope

- 获取/验证最终 CTA 数据集与金标准。
- 编写完整三维预处理、分割、中心线或网格产品服务。
- 选择科学指标阈值。
- Flow、Darcy、CTC、ONNX 或多尺度调度。

## Blocking Decisions Owned by This Child

1. vtkvmtk 是否可采用。
2. 真实 3D skeleton fallback 具体来源。
3. TetGen 在目标研究/分发方式下是否可采用。
4. 后续 vascular build 使用的精确 modules、targets 和 runtime 配方。
