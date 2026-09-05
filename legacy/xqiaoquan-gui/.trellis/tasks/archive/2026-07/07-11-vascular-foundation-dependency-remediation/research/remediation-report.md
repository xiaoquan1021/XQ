# 血管地基依赖生产基线整改报告

日期：2026-07-11

任务：`07-11-vascular-foundation-dependency-remediation`

## 结论

本 child 的 Windows x64 Release 依赖生产基线已经通过：XQ 使用隔离的 QtBase
6.7.0、精确的 VTK 9.3.0 / ITK 5.4.0 / tinyxml2 8.0.0 package 发现和有界
target 链接；默认产品构建图与递归 PE 闭包中均没有 Python、MITK、Slicer、
CTK、BlueBerry、vmtk shared runtime、旧 Qt 或 zstd 污染。

TetGen/MMG ON/ON 分支也通过技术构建与测试，但 TetGen 仍是
`accepted-research-only`。本报告不批准其专有产品分发，也不声称真实 CTA
分割、中心线质量、血管网格质量或父任务完成。

## 1. 隔离 QtBase 6.7.0

### 1.1 路径与身份

| 项目 | 值 |
| --- | --- |
| Externals repository | `C:\Users\OCEAN\Desktop\XIAOQUAN\Externals` |
| Externals commit | `4f79c20504f42eb389791a71d089664b0287f465` |
| Qt source | `src/qt-everywhere-src-6.7.0` |
| Qt source commit | `2c1ae6def2d3e65337e8331f3cdc08bff874181a` |
| Qt source tree | `8393a732899f54d7450b96cee117d9157a2f12cb` |
| 成功 build tree | `build/windows-x64-vascular/QtBaseClean` |
| 成功 install prefix | `install/windows-x64-vascular/qt-6.7.0` |
| 最终 package dir | `install/windows-x64-vascular/qt-6.7.0/lib/cmake/Qt6` |
| 保留的完整 Qt 失败树 | `build/windows-x64-vascular/Qt` |

完整 Qt 超集曾在非产品必需模块阶段失败；该失败树被保留，没有删除或伪装成
成功。壳子实际需要的 QtBase 闭包改用净化环境、相同 Qt 源和独立 prefix 构建，
并以 `QT_BUILD_SUBMODULES=qtbase` 明确收口。

Qt recipe 回归：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tests/test_qt_windows_no_host_zstd.ps1
```

结果：exit `0`。当前工作树的 Qt switch clause 与 commit `4f79c205...` 中的
clause 相同，未被 Externals 中其它未提交工作改写。

- 构建时记录的 recipe block SHA-256：
  `DD12A8E56A360CD11E44A3E90258737566B57D52BCBD1F1FC9919323315E49AD`
- 收尾复核的可复现规范化哈希（完整 Qt switch clause，LF，UTF-8）：
  `6BD683B24D676F68893A20BA41DC96D91351CD5F2A9D273299915F490EC2EF27`

两者的序列化口径不同；身份判断以 recipe commit、clause 等同性和 recipe test
共同为准。

### 1.2 构建与检查命令

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File XQ/probes/vascular_foundation/build_isolated_qtbase.ps1 `
  -ExternalsRoot C:\Users\OCEAN\Desktop\XIAOQUAN\Externals `
  -Platform windows-x64-vascular `
  -Jobs 8

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File XQ/probes/vascular_foundation/check_isolated_qtbase.ps1 `
  -BuildDir C:\Users\OCEAN\Desktop\XIAOQUAN\Externals\build\windows-x64-vascular\QtBaseClean `
  -QtRoot C:\Users\OCEAN\Desktop\XIAOQUAN\Externals\install\windows-x64-vascular\qt-6.7.0 `
  -LegacyQtRoot C:\Users\OCEAN\Desktop\XIAOQUAN\Externals\install\windows-x64\qt-6.7.0 `
  -Dumpbin "C:\software\Visual Studio\Visual Studio2022\Community\VC\Tools\MSVC\14.43.34808\bin\Hostx64\x64\dumpbin.exe"
```

最终 cache 证据：

```text
CMAKE_INSTALL_PREFIX=C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64-vascular/qt-6.7.0
CMAKE_HOME_DIRECTORY=C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/src/qt-everywhere-src-6.7.0
FEATURE_zstd=OFF
QT_FEATURE_zstd=OFF
QT_BUILD_SUBMODULES=qtbase
zstd_DIR=zstd_DIR-NOTFOUND
host Anaconda cache hit=0
```

DLL 哈希：

| DLL | SHA-256 |
| --- | --- |
| 旧 `windows-x64/qt-6.7.0/bin/Qt6Core.dll` | `35DEE962158C90E7958F3540070C3E6013A140401D2479D05378097FE11DB629` |
| 新 `windows-x64-vascular/qt-6.7.0/bin/Qt6Core.dll` | `1E28DFCE1891482756B983648C31E0BF251F00E81CF413B8DBC124B52E3145D9` |

旧哈希与整改前记录一致，证明没有覆盖原安装。新 Qt 从 `Qt6Core.dll`、
`Qt6Gui.dll`、`Qt6Widgets.dll`、`Qt6OpenGL.dll`、`Qt6OpenGLWidgets.dll`、
`qwindows.dll`、`qoffscreen.dll` 共 7 个入口递归扫描，禁止依赖和未解析的
非系统依赖均为 `0`；没有 `zstd.dll`。

## 2. XQ package 与 target 收口

### 2.1 精确发现

产品 `XQ/CMakeLists.txt` 当前使用：

```cmake
find_package(Qt6 6.7.0 EXACT CONFIG REQUIRED COMPONENTS Core Widgets)
find_package(VTK 9.3.0 EXACT CONFIG REQUIRED COMPONENTS ...)
find_package(tinyxml2 8.0.0 EXACT CONFIG REQUIRED)
find_package(ITK 5.4.0 EXACT CONFIG REQUIRED COMPONENTS ${_xq_itk_components})
```

显式 ITK capability set：

```text
ITKLevelSets
ITKImageGradient
ITKAnisotropicSmoothing
ITKImageFeature
ITKBinaryMathematicalMorphology
ITKConnectedComponents
ITKDistanceMap
ITKVtkGlue
ITKIOGDCM
```

产品不再调用 `include(${ITK_USE_FILE})`，也不再链接 package-wide
`${ITK_LIBRARIES}`。旧的独立 capability probe 仍可为了探针自身展开 broad
ITK closure；它不是产品 target，最终产品构建图检查仍要求零污染。

### 2.2 ITKVtkGlue 与配置期 Python

安装版 `ITKVtkGlue.cmake` 会内部再次执行 componentless、`REQUIRED` 的
`find_package(VTK)`，从而把 `VTK_LIBRARIES` 覆盖成全部已安装 VTK modules，
其中包括 Python wrapping targets。产品因此在第一次显式 VTK discovery 后
冻结 `_xq_product_vtk_libraries`，ITK discovery 完成后再恢复。

同一安装配置还会触发 VTK package 的 Python3 查找。整改脚本将这个
**仅配置期**依赖锁到：

```text
install/windows-x64/python-3.11.0/python.exe
install/windows-x64/python-3.11.0/include
install/windows-x64/python-3.11.0/libs/python311.lib
```

它不得出现在 XQ link graph 或 PE runtime closure。最终 checker 同时验证这三项
cache identity，并禁止 host `C:\software\anaconda`。

### 2.3 target 映射与 ABI

| 产品 target | 有界依赖 |
| --- | --- |
| `xq_adapter_itk` | `sv_levelset`、`${ITKLevelSets_LIBRARIES}`、`${ITKImageGradient_LIBRARIES}` |
| `xq_adapter_dicom` | `ITKIOGDCM`、`gdcmMSFF` |
| VTK app/render/tests | ITK discovery 前冻结的显式 VTK 9.3.0 C++ set |

`xq_adapter_itk` 与 `xq_adapter_dicom` 的公开头继续只暴露 XQ 自有类型；
`test_arch_boundaries` 通过。MSVC runtime 固定为动态 CRT，生成的 Release
Ninja flags 使用 `-MD`，没有 `/MT` 产品目标。

## 3. 默认产品基线验证

一键命令：

```bat
cmd.exe /d /c XQ\probes\vascular_foundation\run_dependency_remediation.bat
```

显式 roots：隔离 Qt、VTK 9.3.0、ITK 5.4.0、GDCM 3.0.10、HDF5 1.14.3、
tinyxml2 8.0.0 和仅配置期 Python 3.11.0。`CMAKE_PREFIX_PATH` 不再包含过宽的
`install/windows-x64` 总根。

结果：

| 检查 | 结果 |
| --- | --- |
| Fresh Ninja Release build | 306/306 targets built |
| Build graph checker | PASS；Python lib、VTK Python、MITK/Slicer/CTK/BlueBerry、旧 Qt、host Anaconda 命中均为 0 |
| Recursive PE checker | PASS；92 个二进制；禁止 DLL 与未解析非系统 DLL 均为 0 |
| Focused CTest | 8/8 PASS |
| Full Release CTest | 93/93 PASS |
| Negative configure probes | 7/7 PASS |

Focused 集合：`test_dependency_baseline`、`test_arch_boundaries`、
`test_itk_vascular_segmenter`、`test_dicom_series_adapter`、
`test_dicom_fixture_policy`、`test_dicom_import_integration`、
`test_app_startup`、`test_main_window`。

## 4. 负向配置矩阵

| 用例 | 必须失败的原因 | 结果 |
| --- | --- | --- |
| `bad-qt` | 错误 `Qt6_DIR` | PASS，非零退出且命中显式诊断 |
| `bad-vtk` | 错误 `VTK_DIR` | PASS |
| `bad-itk` | 错误 `ITK_DIR` | PASS |
| `empty-qt` | 显式空 `Qt6_DIR` | PASS |
| `tetgen-no-ack` | TetGen ON、未 acknowledgement | PASS |
| `wrong-qt-version` | 请求 Qt 6.7.1 EXACT | PASS |
| `missing-itk-component` | 请求不存在的 ITK component | PASS |

所有用例使用新的隔离 build tree；没有通过注册表、host PATH、Anaconda、旧 Qt
或另一套 VTK/ITK 静默回退。

## 5. TetGen/MMG research-only 验证

一键命令：

```bat
cmd.exe /d /c XQ\probes\vascular_foundation\run_mesh_on_audit.bat
```

配置明确包含：

```text
XQ_ENABLE_TETGEN=ON
XQ_ACKNOWLEDGE_TETGEN_RESEARCH_ONLY=ON
XQ_ENABLE_MMG=ON
```

结果：

| 检查 | 结果 |
| --- | --- |
| Fresh Ninja Release build | 318/318 targets built |
| Build graph checker | PASS |
| Recursive PE checker | PASS；95 个二进制；禁止 DLL 与未解析非系统 DLL 均为 0 |
| Focused CTest | 3/3 PASS：TetGen、MMG、architecture |
| Full Release CTest | 95/95 PASS |

TetGen 源码可证身份是 1.5，不是 1.5.1。acknowledgement 只防止无意启用，
不等于商业许可或 AGPL-compatible 分发批准。

## 6. source-only 依赖锁

- vtkvmtk：SimVascular commit
  `b8c30d7d6194f16246ae9a435514cc55f6f6ab0b`；八个选定 `.cxx` 的有序
  filename/file-hash aggregate：
  `15328E8A7A066E3EBD0477488F9AB7F192DE3610CAA4C142625D608CBF26DF25`。
  当前只是 accepted technical candidate；未来 vendoring 必须携带 VMTK BSD、
  SimVascular 和 VTK notices。
- ITKThickness3D：tag `v5.3.0`，commit
  `36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb`，tree
  `e03750b011699d2989d28700f0ffba6b61d64979`。当前只是 header-only fallback
  candidate，不引入第二套 ITK。

本 child 没有为了“看起来完成”而把这两套尚无生产 adapter 的源码强行链接进 XQ。

## 7. AC1–AC10 验收矩阵

| AC | 状态 | 证据 |
| --- | --- | --- |
| AC1 | PASS | recipe test 通过；独立 QtBase prefix 完成；旧 QtCore 哈希未变 |
| AC2 | PASS | zstd 两项 feature OFF、`zstd_DIR-NOTFOUND`、7 个 Qt 入口闭包无 zstd/Anaconda |
| AC3 | PASS | Qt/VTK/ITK/tinyxml2 精确版本；显式 ITK components；无产品 `ITK_USE_FILE`/`${ITK_LIBRARIES}` |
| AC4 | PASS | ITK/DICOM target-specific closure；相关 focused tests 真编译并运行 |
| AC5 | PASS | 默认与 research build graph 禁止模式命中 0 |
| AC6 | PASS | 默认 92、research 95 个 PE；禁止 runtime 与未解析非系统 DLL 均为 0 |
| AC7 | PASS | 7 个隔离负向 configure 用例稳定非零退出 |
| AC8 | PASS | TetGen 1.5；未 ack 失败；ON/ON focused/full 绿；仍标 research-only |
| AC9 | PASS | 默认 93/93、research 95/95；public-header guard 通过 |
| AC10 | PASS | 本报告记录 identity、命令、hash、link/PE、负向测试和许可限制；无父任务完成夸大 |

## 8. 仍未解决与明确不主张

1. TetGen 专有产品分发仍需商业许可、AGPL-compatible 方案或替代 fill backend。
2. 最终第三方 notice/source-offer bundle 尚未制作。
3. vtkvmtk 与 ITKThickness3D 仍是锁定的 source-only candidates，尚未成为生产 adapter。
4. 真实 CTA 自动分割、中心线质量、半径准确性、拓扑和真实血管网格质量由其它 child 验收。
5. 父任务 `07-11-vascular-imaging-foundation` 仍未完成。

## 9. 工作区保护

- 未修改、清理或提交 Externals 的用户未提交源码。
- 保留 `build/windows-x64-vascular/Qt` 失败树和成功的 `QtBaseClean`/install 产物。
- 未删除 `CHECK-*`、`EXECUTE-*`、`XQ/xq_app_dist/` 或其它 Trellis 任务。
- 未执行 `git add .`、`git clean`、hard reset，也未自动提交本任务。
