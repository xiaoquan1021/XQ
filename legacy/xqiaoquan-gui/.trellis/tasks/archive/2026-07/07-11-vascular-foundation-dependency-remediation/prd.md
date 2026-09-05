# 血管地基：依赖生产基线整改

> Parent: `07-11-vascular-imaging-foundation`
>
> Status: `completed`
>
> 本 child 只把已审计的依赖结论变成可复现的产品构建基线，不交付 CTA 分割、中心线质量或真实网格质量。

## Goal

消除依赖审计发现的主机环境渗入，使 XQ 能从隔离的 Windows x64 Release 配方构建、链接和运行：Qt 不再依赖主机 Anaconda `zstd.dll`，ITK/VTK 只按精确版本和显式组件进入目标，产品链接命令与 PE 运行时闭包均不包含 Python、MITK、Slicer、CTK、BlueBerry 或其它未锁定运行时。

## Confirmed Facts

- 已归档审计：`.trellis/tasks/archive/2026-07/07-11-vascular-foundation-dependency-audit`。
- 明确可采用的 C++ 基线是 VTK 9.3.0、ITK 5.4.0、GDCM 3.0.10、Qt 6.7.0，以及按需启用的 MMG 5.3.9。
- 当前安装的 `Qt6Core.dll` 依赖主机 Anaconda `zstd.dll`；现有 Externals 配方已在提交 `4f79c20` 中加入 `-no-feature-zstd`，但安装树早于该修复。
- `XQ/CMakeLists.txt` 仍使用宽泛 `find_package(ITK 5.4 REQUIRED)`、`ITK_USE_FILE` 和 `${ITK_LIBRARIES}`，导致未使用的 VTK Python targets 与主机 `python312.lib` 出现在产品链接命令。
- 已复现安装版 `ITKVtkGlue.cmake` 会内部再次执行无 components 的 `find_package(VTK)`，覆盖产品先前的显式 `VTK_LIBRARIES`；即使 ITK 本身按 components 查找，也必须保护第一次 VTK 结果。
- 显式组件探针、ITK/VTK bridge、ITKThickness3D、vtkvmtk 和 TetGen/MMG ON/ON 技术探针已通过；这不等于当前产品依赖基线已通过。
- TetGen 源码自标识 1.5，采用 AGPL-3.0-or-later/commercial 双许可；没有商业许可或 AGPL-compatible 分发决策时只能用于明确的 research-only 构建。
- Externals 工作区存在用户未提交修改；本 child 不修改、清理、提交或覆盖该工作区，只使用已确认未改变 Qt 分支语义的配方，并写入全新的 platform build/install 目录。

## Requirements

### R1 - 隔离重建 Qt

- 使用 `-Platform windows-x64-vascular` 在全新的 `build/` 与 `install/` 树中只构建 Qt 6.7.0；不得覆盖 `windows-x64` 现有安装。
- 构建前运行 `tests/test_qt_windows_no_host_zstd.ps1`，并确认 Qt recipe 仍包含 `-no-feature-zstd`。
- 构建后检查 Qt cache、`Qt6Core.dll` 导入表和递归 PE 闭包；不得出现 `zstd.dll` 或指向 Anaconda 的 zstd 配置。
- 记录 Qt source identity、recipe commit、Qt recipe block hash、安装 DLL hash和最终 package 路径。

### R2 - 精确且有界的 XQ package discovery

- Qt、VTK、ITK 使用精确版本要求；VTK 保持显式 C++ components，ITK 改为显式 components。
- ITK component 集必须覆盖当前二维 level-set/DICOM 路径和后续血管地基已审计能力：LevelSets、ImageGradient、3D diffusion、Hessian objectness、morphology、connected components、distance map、ITKVtkGlue、ITKIOGDCM。
- ITKVtkGlue capability discovery 前后必须冻结/恢复产品显式 VTK target set，防止其安装配置把 all-VTK/Python targets 写回后续目标。
- 删除产品目录级 `ITK_USE_FILE` 依赖；目标只链接其真实使用的 ITK imported targets，不使用宽泛 `${ITK_LIBRARIES}`。
- `xq_adapter_itk` 与 `xq_adapter_dicom` 的公开 API 仍只暴露 XQ 自有类型，不能把 ITK/VTK/GDCM 类型带入 public headers。

### R3 - 构建隔离与失败可诊断

- 新产品构建树必须显式传入新 Qt package 目录和已审计的 VTK/ITK/GDCM/tinyxml2 roots。
- 错误 Qt/VTK/ITK package 目录、错误版本或缺失组件必须在 configure 阶段稳定失败，不得回退到注册表、Anaconda、另一套 VTK/ITK 或旧 Qt。
- 最终 `build.ninja`/link command 不得包含 `python*.lib`、VTK Python wrapping targets、MITK、Slicer、CTK 或 BlueBerry 路径。
- 最终 XQ executable/test 的递归 PE 闭包不得包含上述运行时，也不得再包含 `zstd.dll`。

### R4 - source-only 血管依赖锁继续有效

- vtkvmtk 继续锁定 SimVascular `b8c30d7d6194f16246ae9a435514cc55f6f6ab0b` 的八文件 C++ closure，并保留 VMTK BSD notice 要求。
- ITKThickness3D 继续锁定 `v5.3.0` / `36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb`，不得隐式下载 HEAD 或引入第二套 ITK。
- 本 child 只固化版本、hash、license 和未来接入边界；不在尚无生产 adapter 的情况下把未使用源码强行链接进 XQ。

### R5 - TetGen research-only 边界

- 修正代码/构建说明中把 TetGen 写成 1.5.1 的错误，统一为源码可证的 1.5。
- `XQ_ENABLE_TETGEN=ON` 必须伴随显式 research-license acknowledgement；未确认时 configure 失败，不能让普通产品构建无意启用 AGPL/commercial kernel。
- ON/ON 审计脚本显式传入该 acknowledgement，并继续证明 TetGen -> MMG 技术分支真实执行；该结果不得表述为可分发生产许可已解决。

### R6 - 回归与证据

- 在新的 XQ Release build tree 完成 configure、build、聚焦 dependency tests 和全量 CTest。
- 依赖验证必须串行运行，不与其它 build tree 的全量 CTest 并发。
- 产出 `research/remediation-report.md`，记录命令、路径、hash、link/PE 扫描结果、负向测试和仍未解决的许可限制。

### R7 - 工作区保护

- 不修改或覆盖现有 Externals 安装树、用户未提交文件、`XQ/xq_app_dist/`、`CHECK-*`、`EXECUTE-*` 或其它 Trellis 任务。
- 不使用 `git add .`、`git clean`、hard reset 或递归清理未知目录。

## Acceptance Criteria

- [x] AC1：Qt recipe 回归测试通过；全新 `windows-x64-vascular/qt-6.7.0` 安装完成，旧 `windows-x64` 安装未改变。
- [x] AC2：新 `Qt6Core.dll` 的 cache/导入/递归闭包证明 `FEATURE_zstd=OFF`（或等价禁用结果），无 Anaconda zstd 路径且不导入 `zstd.dll`。
- [x] AC3：XQ 使用 Qt 6.7.0、VTK 9.3.0 EXACT、ITK 5.4.0 EXACT 与显式 ITK components；产品代码不再调用 `ITK_USE_FILE` 或链接 `${ITK_LIBRARIES}`。
- [x] AC4：`xq_adapter_itk`、`xq_adapter_dicom` 只链接所需 imported targets，当前 level-set 与 DICOM tests 真实编译运行。
- [x] AC5：全新产品 link command 中 `python*.lib`、VTK Python、MITK/Slicer/CTK/BlueBerry 命中数为零。
- [x] AC6：最终 app/关键 tests 的递归 PE 闭包中 Python/MITK/Slicer/CTK/BlueBerry/vmtk shared runtime/zstd 命中数为零，所有非系统 DLL 均能从锁定 prefixes 解析。
- [x] AC7：错误 package 目录/版本/组件的隔离 configure 用例稳定非零退出，且没有 host fallback。
- [x] AC8：TetGen 版本说明为 1.5；未确认 research acknowledgement 时 ON configure 失败，确认后 TetGen/MMG focused tests 通过且报告仍标记 research-only。
- [x] AC9：新 Release build 的 dependency-focused tests 与全量 CTest 全绿；架构 public-header guard 继续通过。
- [x] AC10：remediation report 与依赖锁记录完整，并明确不声称 CTA 分割、中心线质量、真实网格质量或父任务完成。

## Out of Scope

- 获取或验收真实 CTA 数据集。
- 实现 3D vesselness、自动分割、ITK/VTK 产品 bridge、中心线 adapter 或真实血管网格服务。
- 解决 TetGen 商业许可、选择替代 fill backend 或制作最终第三方 notice bundle。
- 修改 MITK/BlueBerry、Python、Slicer、CTK 或 vmtk SuperBuild。

## Planning Decision

采用“隔离新 Qt prefix + XQ 显式 package/target 收口”的方式，不原地修补 DLL、不复制主机 `zstd.dll`、不覆盖旧安装，也不为绕过链接污染而删除已安装的 Python/VTK wrapper 文件。
