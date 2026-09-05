# Design: 依赖生产基线整改

## 1. Boundary

整改分成两个互不混淆的边界：

```text
Externals source + committed Qt recipe
  -> isolated build/windows-x64-vascular/Qt
  -> isolated install/windows-x64-vascular/qt-6.7.0
  -> explicit Qt6_DIR

audited existing VTK/ITK/GDCM/tinyxml2 prefixes
  -> exact find_package components
  -> target-specific imported targets
  -> clean XQ Release link and runtime closure
```

Externals 只产生新构建产物；XQ 仓库只修改自身 CMake、验证脚本/测试和整改报告。旧安装树是只读对照物。

## 2. Qt Isolation Design

- 平台名固定为 `windows-x64-vascular`，因此 `env_variables.ps1` 自动把 build/install 路径隔离。
- 仅执行 `-Target Qt`，不重建或复制 MITK/Python 等整套依赖。
- XQ configure 显式传入新 Qt 的 `Qt6_DIR`，其余已审计依赖仍来自现有 `windows-x64` 各自版本目录。
- 相同 Qt 6.7.0 ABI 允许它继续与当前 VTK `GUISupportQt` 配合；最终 clean build 和 GUI tests 是兼容性的实际证明。
- 不用“把 Anaconda zstd.dll 拷到发布目录”作为修复，因为那会保留未锁定 host ingress。

## 3. XQ Package Contract

### 3.1 Exact package discovery

目标形态：

```cmake
find_package(Qt6 6.7.0 EXACT CONFIG REQUIRED COMPONENTS Core Widgets)
find_package(VTK 9.3.0 EXACT CONFIG REQUIRED COMPONENTS ...)
find_package(ITK 5.4.0 EXACT CONFIG REQUIRED COMPONENTS
    ITKLevelSets
    ITKImageGradient
    ITKAnisotropicSmoothing
    ITKImageFeature
    ITKBinaryMathematicalMorphology
    ITKConnectedComponents
    ITKDistanceMap
    ITKVtkGlue
    ITKIOGDCM)
```

最终 component 名以实际 ITK 5.4 package configure 结果为准，但不得退回无 components 的全包发现。

安装版 `ITKVtkGlue.cmake` 自身会调用无 components 的 `find_package(VTK)`。因此在第一次显式 VTK discovery 后立即冻结 `VTK_LIBRARIES`，ITK discovery 完成后恢复该集合。否则后续仍使用 `${VTK_LIBRARIES}` 的既有目标会重新链接整套 VTK/Python graph，即使最终 PE 恰好没有 Python import，也不算构建隔离通过。

### 3.2 Target-specific linking

- ITK 5.4 的 `ITKLevelSets`、`ITKImageGradient` 等 header-only modules 没有同名 imported target；不能假造 target 名。
- `xq_adapter_itk`：只使用 `${ITKLevelSets_LIBRARIES}` 与 `${ITKImageGradient_LIBRARIES}` 两个 module-specific closure；其展开项是实际 imported targets。
- `xq_adapter_dicom`：链接 `ITKIOGDCM` 与 `gdcmMSFF`。
- 后续 3D adapter 只在实际出现时链接 diffusion/objectness/morphology/components/distance/bridge targets。
- `ITKVtkGlue` 可以在 package capability set 中被发现，但在尚无产品 bridge target 时不通过 `${ITK_LIBRARIES}` 被全局吸入链接。

这使“已安装/可用组件”和“当前 target 实际链接组件”成为两个可审计层次。

## 4. Strict Configure Evidence

产品开发仍可通过标准 CMake package mechanisms 配置；本 child 的生产基线脚本必须显式传入：

- `Qt6_DIR=<new isolated Qt>/lib/cmake/Qt6`
- `VTK_DIR=<locked VTK>/lib/cmake/vtk-9.3`
- `ITK_DIR=<locked ITK>/lib/cmake/ITK-5.4`
- `tinyxml2_DIR=<locked tinyxml2>/lib/cmake/tinyxml2`
- `CMAKE_PREFIX_PATH` 只包含已审计 roots，顺序以新 Qt 为先。

负向用例使用新的空 build trees。错误显式 package path 必须在调用 package discovery 前或期间失败；不得把 host PATH/registry 的成功当作可复现。

## 5. Link and Runtime Verification

验证分两层：

1. Build graph：扫描 `build.ninja`、link response files 和 CMake target graph，确认没有 `python*.lib`、VTK Python、MITK/Slicer/CTK/BlueBerry。
2. Runtime graph：从 `xq_app.exe`、ITK/DICOM tests、Qt/VTK GUI tests 递归执行 `dumpbin /dependents`，所有非系统 DLL 必须解析到显式 prefixes；禁止 zstd 和 forbidden runtimes。

链接扫描不能被“linker 最后裁掉了”替代；PE 扫描也不能替代 build graph 扫描。

## 6. Source-only Dependency Locks

vtkvmtk 与 ITKThickness3D 继续以审计报告中的 commit/tree/file hashes 为权威。整改报告引用并复核 lock，不提前把未使用源复制进产品。实际 adapter child 在 vendoring 时必须：

- 验证 exact commit/file aggregate；
- 携带对应 license/NOTICE；
- 使用同一 VTK/ITK package；
- 禁止动态下载 HEAD、Python 或第二套 superbuild。

## 7. TetGen Boundary

保留 `XQ_ENABLE_TETGEN` 作为技术研究开关，但增加一个默认 OFF 的显式 acknowledgement。组合矩阵：

| TetGen | acknowledgement | Result |
| --- | --- | --- |
| OFF | any | 普通产品基线，不编译 TetGen |
| ON | OFF | configure 失败并说明 AGPL/commercial 边界 |
| ON | ON | 允许 research-only 技术构建；MMG 仍要求 TetGen ON |

该 acknowledgement 不是法律许可，只防止无意启用并保持完成表述诚实。

## 8. Rollback

- 删除新的 XQ remediation build tree即可回滚构建实验；不触碰旧 build trees。
- 新 Qt prefix 是独立目录；失败时保留日志或仅在确认绝对路径后由用户决定清理。
- XQ CMake 修改保持小步：先 package/target 收口，再加入 license gate和验证脚本；每步都能通过 focused configure/build 判定。
- 不修改 Externals 已有源码或用户脏文件，因此无需 reset/checkout。
