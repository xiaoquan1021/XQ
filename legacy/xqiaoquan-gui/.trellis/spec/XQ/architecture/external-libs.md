# 外部库规则与依赖基线(external libs)

> 来源:`plan/00-architecture.md`、`plan/09-dependencies.md`。版本以 `Externals/externals.manifest` 为准。

---

## 外部库只能是 kernel / codec / 渲染后端

外部库出现在 **adapter 或 service 的私有实现**里,或作为一次性的可视化产物
(disposable visualization product)。**不得出现在公开业务 API 中。**

公开 API 暴露 XQ 自有的 value / handle / payload 类型(如 `XQImageVolume`、
`XQTriangleSurfaceGeometryHandle`、`XQMesh`)。外部原生对象只能在私有实现或 backend adapter 内部持有。

| 库 | 角色 | 出现位置 |
|---|---|---|
| Qt6 | 桌面 UI 框架 | 仅 `app/`(及 services 之上的 UI 适配) |
| VTK | 渲染 + 几何 + vtp/vtu/vti 读写 | `adapters/vtk`、`app` 渲染;不进 core |
| ITK | 影像处理/重采样/形态学 | `adapters/itk`、分割服务私有实现 |
| GDCM | DICOM 读取 | `adapters/gdcm`、影像 io 私有实现 |
| ONNX Runtime | AI 推理后端 | `adapters/onnx`、`services/ai` 私有实现;**无 Python 依赖** |
| OCCT / MMG | CAD / 重网格(暂留) | 引入后置于 `adapters`;首版不引入 |

## 依赖基线版本(Externals 锁定)

| 依赖 | 版本 | 角色 |
|---|---|---|
| Qt6 | 6.7.0 | 桌面 UI |
| VTK | 9.3.0 | 渲染 + 几何 + vtp/vtu/vti |
| ITK | 5.4.0 | 影像处理/重采样/形态学 |
| GDCM | 3.0.10 | DICOM 读取(不引入 DCMTK) |
| TinyXML2 | 8.0.0 | XML 解析(.svproj/.pth/.ctgr/.mdl/原生存档) |
| HDF5 | 1.14.3 | 大数据/网格存储(按需) |
| FreeType | 2.13.0 | 字体(VTK/Qt 传递依赖) |
| ONNX Runtime | 待定(CPU 版起步) | AI 推理;**Externals 当前没有,需新增到 manifest** |

## 暂留 / 弃用

- **暂留(首版不引入)**:OCCT 7.6.0(建模)、MMG 5.3.9(网格)——首版用 VTK / XQ 自有三角化;
  需 CAD 级精度或高质量重网格时再引入 `adapters/occt`、`adapters/mmg`。
  - MMG adapter 依赖 TetGen adapter(两段式:TetGen 填充 → MMG 优化),属 adapters 层内的合法有向依赖。
- **明确弃用**:MITK 作为插件 workbench 架构弃用(其库能力语义仍是参考来源,见 `reference-sources.md`)、
  BlueBerry / CTK、SWIG / Python(首版不做 Python 绑定,AI 走 C++ 内置 ONNX)。

## 规则

- manifest 是版本权威;实现期精确锁(`dependencies.lock.json`)在实现工程内另行创建。
- 引入"暂留/新增"依赖前,先确认它只进 `adapters` / 私有实现,不污染公开 API。
- 依赖变更会使下游服务与测试过期,变更时同步更新相关 plan 文档与测试。

## Scenario: MMG Windows 可选内核的发现与运行时边界

### 1. Scope / Trigger

- Trigger: 修改 `XQ_ENABLE_MMG`、MMG 查找逻辑、MMG adapter 链接或任何会
  启动 MMG-linked test/app 的 CTest 环境。
- Purpose: 禁止从 host 默认路径误取 MMG,禁止 header/lib/DLL 混前缀,并在
  configure 阶段暴露不完整安装。

### 2. Signatures

```cmake
option(XQ_ENABLE_MMG "Build MMG3D volume remesh adapter" OFF)
find_path(MMG_INCLUDE_DIR mmg/mmg3d/libmmg3d.h ... NO_DEFAULT_PATH)
find_library(MMG3D_LIBRARY mmg3d ... NO_DEFAULT_PATH)
```

- Required Windows runtime: `<MMG prefix>/bin/mmg3d.dll`.
- Required fill-stage switch: `XQ_ENABLE_TETGEN=ON`.
- Runtime test: `test_mmg_volume_mesh`.

### 3. Contracts

- `XQ_ENABLE_MMG=OFF` 不发现、不链接、不部署 MMG。
- ON 时只从显式 `CMAKE_PREFIX_PATH`/cache 输入查找,不得回退到注册表、host
  PATH 或其它默认 prefix。
- 验证 adapter 实际 include 的 `mmg/mmg3d/libmmg3d.h`,不能只验证 umbrella
  header `mmg/libmmg.h`。
- header 与 import library 经 `file(REAL_PATH)` 后必须属于同一 prefix;Windows
  下同 prefix 必须同时存在 `bin/mmg3d.dll`。
- `test_mmg_volume_mesh` 及链接 app shell 的 GUI tests 必须在 CTest
  `ENVIRONMENT` 中显式前置 MMG `bin`;外部 shell 的 PATH 不算测试契约。
- 审计身份写作“official tag v5.3.9, embedded runtime version 5.3.8”。官方
  tag 自身含旧 `5.3.8` 宏,不能仅凭 runtime string 判成混装;采用结论仍须由
  source/hash lock 证明。
- MMG 是动态 LGPL kernel。当前 TetGen fill stage 自标识 1.5 且为
  AGPL-3.0-or-later/commercial,未取得商业许可或 AGPL-compatible 分发方案前只
  能标为 research-only。

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| MMG ON, TetGen OFF | configure fails: `XQ_ENABLE_MMG requires XQ_ENABLE_TETGEN=ON` |
| real MMG3D header or import library missing | configure fails: `XQ_ENABLE_MMG=ON but MMG not found` |
| header and library resolve to different prefixes | configure fails and prints both prefixes |
| Windows import library exists but `bin/mmg3d.dll` is absent | configure fails with the expected DLL path |
| complete single-prefix install | configure, Release link, MMG test and full CTest execute |
| process exits `0xc0000135` | treat as a DLL PATH defect, never as backend success/fallback |

### 5. Good/Base/Bad Cases

- Good: explicit locked prefix contains the consumed header, import library and
  DLL; ON/ON Release tests execute the TetGen-to-MMG branch.
- Base: both switches OFF; the default build has no MMG/TetGen dependency.
- Bad: a generic header from prefix A plus `mmg3d.lib` from prefix B, or a test
  that passes only because a host MMG DLL happens to be on PATH.

### 6. Tests Required

- Run `XQ/probes/vascular_foundation/run_mesh_on_audit.bat`; assert focused MMG,
  TetGen and architecture tests pass, followed by full Release CTest.
- Configure isolated negative trees for MMG-without-TetGen, missing real API
  header, mixed prefixes and missing runtime DLL; assert non-zero configure and
  the diagnostics above.
- Recursively inspect the final PE imports; assert the selected `mmg3d.dll` is
  from the locked prefix and no Python/MITK/Slicer/CTK runtime enters the closure.

### 7. Wrong vs Correct

#### Wrong

```cmake
find_path(MMG_INCLUDE_DIR mmg/libmmg.h)
find_library(MMG3D_LIBRARY mmg3d)
```

This accepts host defaults, an unused umbrella header and an import library
without proving its runtime DLL.

#### Correct

```cmake
find_path(MMG_INCLUDE_DIR mmg/mmg3d/libmmg3d.h
    PATHS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES include NO_DEFAULT_PATH)
find_library(MMG3D_LIBRARY mmg3d
    PATHS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES lib NO_DEFAULT_PATH)
# Then require one real prefix and <prefix>/bin/mmg3d.dll on Windows.
```

## Scenario: Windows 依赖生产基线的隔离发现与闭包

### 1. Scope / Trigger

- Trigger: 修改 Qt/VTK/ITK/GDCM/tinyxml2 发现、adapter 链接、Windows
  `CMAKE_PREFIX_PATH`、依赖审计脚本，或重建供 XQ 使用的 Qt。
- Purpose: package configure 成功、产品链接图和最终 PE 闭包必须同时可复现；
  linker 最后裁掉了污染 target 不算 build isolation 通过。

### 2. Signatures

```cmake
function(xq_validate_explicit_package_dir variable config_file)

find_package(Qt6 6.7.0 EXACT CONFIG REQUIRED COMPONENTS Core Widgets)
find_package(VTK 9.3.0 EXACT CONFIG REQUIRED COMPONENTS <explicit-cxx-components>)
find_package(tinyxml2 8.0.0 EXACT CONFIG REQUIRED)
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

```text
XQ/probes/vascular_foundation/build_isolated_qtbase.ps1
XQ/probes/vascular_foundation/check_isolated_qtbase.ps1
XQ/probes/vascular_foundation/check_dependency_baseline.ps1
XQ/probes/vascular_foundation/run_dependency_negative_probes.ps1
XQ/probes/vascular_foundation/run_dependency_remediation.bat
```

TetGen research gate：

```cmake
option(XQ_ENABLE_TETGEN ... OFF)
option(XQ_ACKNOWLEDGE_TETGEN_RESEARCH_ONLY ... OFF)
```

### 3. Contracts

- XQ 的壳子 Qt 基线只需要 QtBase。使用全新的
  `windows-x64-vascular/QtBaseClean` build tree 和
  `windows-x64-vascular/qt-6.7.0` prefix；cache 必须满足
  `QT_BUILD_SUBMODULES=qtbase`、`FEATURE_zstd=OFF`、
  `QT_FEATURE_zstd=OFF`、`zstd_DIR-NOTFOUND`。禁止复制 host `zstd.dll`
  作为修复。
- 显式传入的 `Qt6_DIR`、`VTK_DIR`、`ITK_DIR`、`tinyxml2_DIR` 是失败
  边界：空值、目录不存在或缺少对应 Config 文件时必须在 package discovery
  前失败，不得继续搜注册表或 host prefix。
- 生产基线的 `CMAKE_PREFIX_PATH` 只列具体锁定 roots；不要加入过宽的
  `install/windows-x64` 总根。
- canonical configure 前必须清空调用者环境中的 `CMAKE_PREFIX_PATH`，并设置
  `CMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE`、
  `CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE`；cache 中的窄 prefix 不能证明
  环境搜索组或 registry 已自动失效。
- 安装版 ITK 的 `ITKGDCM.cmake` / `ITKHDF5.cmake` 含构建时 package dirs。
  checker 必须把它们与锁定 GDCM/HDF5 roots 比较，并读取版本元数据证明
  GDCM 3.0.10 / HDF5 1.14.3。现有安装不能只复制到新盘符后声称可迁移；需在
  目标路径重建完整安装或让 metadata 与新 roots 一致。
- 安装版 ITK 5.4 的 `ITKVtkGlue.cmake` 会再次无 components 地调用
  `find_package(VTK)`，并覆盖 `VTK_LIBRARIES`。第一次显式 VTK discovery 后
  必须立即保存产品 C++ set，ITK discovery 后恢复；后续
  `vtk_module_autoinit` 和旧 target 只消费恢复后的集合。
- `ITKLevelSets`、`ITKImageGradient` 等 header-heavy modules 不保证存在同名
  imported target。`xq_adapter_itk` 只用 module-specific
  `${ITKLevelSets_LIBRARIES}`、`${ITKImageGradient_LIBRARIES}`；
  `xq_adapter_dicom` 只链接 `ITKIOGDCM`、`gdcmMSFF`。禁止恢复
  `ITK_USE_FILE` 或 package-wide `${ITK_LIBRARIES}`。
- 当前 Windows GDCM 3.0.10 是 shared 安装，但其 exported CMake target 没有
  传播 `GDCM_BUILD_SHARED_LIBS`。所有直接 include GDCM header 的本地 target
  必须显式定义该宏；否则 `GDCM_EXPORT` 退化为空，简单 reader 可能侥幸链接，
  一旦 writer/Attribute 拉入 DLL import member 就会出现 `LNK2005` 重复定义。
  当前直接消费者是 `xq_adapter_dicom`、`test_dicom_fixture_policy`、
  `xq_generate_dicom_fixture`、`xq_normalize_dicom_frame`。
- `ITKVtkGlue` 的安装配置会间接触发 VTK 的 Python3 查找。当前构建脚本将
  **配置期** Python 锁到 Externals Python 3.11.0 的 executable/include/lib；
  它不得进入产品 link command、VTK Python target set 或递归 PE closure，
  也不得解析到 `C:\software\anaconda`。
- shared env 可以定义 configure/runtime 两侧共用的 roots，但 GUI run preflight
  不得检查 Python、样例、旧 Qt cache 或构建工具；launch 前还要清除
  Python/Conda/configure-only 环境变量。否则“文件必须存在”本身就把 Python
  变成运行入口依赖。
- canonical tree 使用 `cmake --fresh` 重新生成配置，并用 `--clean-first` 重编
  二进制；只有 `--fresh` 不会删除旧 object，不能单独作为 fresh-binary 证据。
- Windows Release 产品和本地静态 adapters 使用 MSVC 动态 CRT `/MD`；
  不得把 `/MT` 对象混入同一产品闭包。
- `XQ_ENABLE_TETGEN=ON` 必须同时显式传入
  `XQ_ACKNOWLEDGE_TETGEN_RESEARCH_ONLY=ON`。acknowledgement 不是许可证，
  只允许标记清楚的 research build。

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| Qt cache 启用 zstd、解析 host zstd 或 QtCore 导入 zstd | baseline fails；不得复制 DLL 绕过 |
| 显式 package dir 为空/错误 | configure 非零退出并指出变量与所需 Config 文件 |
| package version 不等于锁定版本 | `EXACT` configure 失败 |
| caller 环境含宽 prefix 或 package registry | configure 清空/禁用；cache/checker 只接受六个精确 roots |
| ITK 内嵌 GDCM/HDF5 dir 与声明 root 不同 | checker 失败，即使旧绝对路径仍存在 |
| runtime 机器移除 configure-only Python | GUI 仍可启动；不得在 runtime preflight 检查 Python |
| ITK component 缺失 | configure 失败，不得退回 componentless ITK |
| ITKVtkGlue 覆盖显式 VTK set | 恢复冻结集合；build graph 中 VTK Python/Python lib 必须为 0 |
| shared GDCM consumer 未定义 `GDCM_BUILD_SHARED_LIBS` | writer/Attribute 链接可报 `LNK2005`；基线失败，不得移除 writer 绕过 |
| Python 只在 configure cache 中解析到锁定 3.11 | 允许；link graph 和 PE closure 仍必须为 0 |
| TetGen ON、ack OFF | configure 失败并说明 AGPL/commercial research-only 边界 |
| 默认 Release tree | build、focused CTest、full CTest、build graph、PE graph 全绿 |
| TetGen/MMG ON/ON tree | 技术测试全绿，但结论仍写 `accepted-research-only` |

### 5. Good/Base/Bad Cases

- Good: 隔离 QtBase 无 zstd；exact package dirs 指向锁定 roots；
  ITKVtkGlue 前后冻结/恢复 VTK C++ set；adapter 只链接实际 module closure；
  direct GDCM targets 以 `GDCM_BUILD_SHARED_LIBS` 消费同一 DLL 安装；build
  graph 与 PE graph 双检查均为零污染。
- Base: TetGen/MMG OFF 的普通产品构建；配置期 Python 3.11 只服务于已安装
  VTK/ITK package metadata，不进入生成物。
- Bad: 把 `install/windows-x64` 总根放入 prefix 后依赖 CMake 自己挑版本，
  使用 `${ITK_LIBRARIES}`，只检查最终 PE 没有 Python，或把 Anaconda
  `zstd.dll` 复制进发布目录后宣称 Qt 已隔离；或者只因 reader 能链接就忽略
  GDCM shared 宏，直到 writer target 才暴露重复符号。

### 6. Tests Required

- 运行 Externals `tests/test_qt_windows_no_host_zstd.ps1`。
- 对隔离 Qt 运行 `check_isolated_qtbase.ps1`；断言 cache、旧/新 DLL identity、
  Core/Gui/Widgets/OpenGL/platform plugins 的递归闭包。
- 运行 `run_dependency_remediation.bat`；断言默认 Release build、focused
  dependency tests、full CTest 和 package/version/component/TetGen 负向矩阵。
- 若修改 TetGen/MMG 路径，再顺序运行 `run_mesh_on_audit.bat`；不能与默认树
  full CTest 并发。
- `check_dependency_baseline.ps1` 必须同时扫描 generated build graph 和递归
  PE imports；断言 Python/MITK/Slicer/CTK/BlueBerry/vmtk shared runtime/zstd、
  host Anaconda 和旧 Qt 命中为 0，且所有非系统 DLL 可从锁定 roots 解析。
- 同一 checker 还要断言六项 `CMAKE_PREFIX_PATH` identity、registry OFF、ITK
  内嵌 GDCM/HDF5 dirs 与 3.0.10/1.14.3 版本元数据。
- 运行 `XQ/verify_canonical_shell_a.bat` 生成顺序 ON/OFF 日志；自动 green 仅为
  PASS-EVIDENCE，真实 `run_xq.bat <project>` 与人工 GUI 检查未记录前不得关闭
  A1/A15。
- `test_dependency_baseline` 与 `test_arch_boundaries` 必须保留在 full CTest 中。
- 构建并链接 `test_dicom_fixture_policy`、`xq_generate_dicom_fixture` 和
  `xq_normalize_dicom_frame`，证明 shared GDCM reader/writer consumers 使用
  一致的 dllimport ABI；不能只编译 `xq_adapter_dicom` 静态库。

### 7. Wrong vs Correct

#### Wrong

```cmake
list(APPEND CMAKE_PREFIX_PATH "C:/.../install/windows-x64")
set(ENV{CMAKE_PREFIX_PATH} "C:/host/anaconda;C:/.../install/windows-x64")
find_package(ITK 5.4 REQUIRED)
include(${ITK_USE_FILE})
target_link_libraries(xq_adapter_itk PRIVATE ${ITK_LIBRARIES})
# ITKVtkGlue may also replace VTK_LIBRARIES with all installed VTK/Python targets.
```

#### Correct

```cmake
find_package(VTK 9.3.0 EXACT CONFIG REQUIRED COMPONENTS <explicit-cxx-components>)
set(ENV{CMAKE_PREFIX_PATH} "")
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY FALSE)
set(_xq_product_vtk_libraries ${VTK_LIBRARIES})

find_package(ITK 5.4.0 EXACT CONFIG REQUIRED COMPONENTS
    ITKLevelSets ITKImageGradient ITKAnisotropicSmoothing ITKImageFeature
    ITKBinaryMathematicalMorphology ITKConnectedComponents ITKDistanceMap
    ITKVtkGlue ITKIOGDCM)

set(VTK_LIBRARIES ${_xq_product_vtk_libraries})
target_link_libraries(xq_adapter_itk PRIVATE
    ${ITKLevelSets_LIBRARIES}
    ${ITKImageGradient_LIBRARIES})
target_link_libraries(xq_adapter_dicom PRIVATE ITKIOGDCM gdcmMSFF)
```

## Scenario: ITK 三维血管预处理边界

### 1. Scope / Trigger

- Trigger: 新增或修改 CT/CTA 去噪、Hessian vesselness、对应 profile、
  `IVoxelSource` 导入或 XQ-owned vesselness 输出。
- Purpose: 复用 ITK 5.4 的真实三维 kernel，保持 LPS/mm 完整几何，并禁止
  逐切片二维、自写 Hessian/eigen 或 ITK 对象泄漏到公共 API。

### 2. Signatures

```cpp
VascularPreprocessResult IVascularPreprocessor::run(
    const XQImageVolume& image,
    const IVoxelSource& source,
    const VascularPreprocessProfileV1& profile,
    const VascularPreprocessCancellation* cancellation = nullptr) const;
```

```text
xq_vascular_preprocess_probe <dicom-directory> [series-uid]
```

生产实现：

```text
ItkVascularPreprocessor
  -> itk::Image<float,3>
  -> CurvatureAnisotropicDiffusionImageFilter
  -> MultiScaleHessianBasedMeasureImageFilter
  -> HessianToObjectnessMeasureImageFilter
  -> XQImageVolume + XQMemoryImageBufferHandle(Float32)
```

### 3. Contracts

- 公共 profile/result/diagnostic/cancellation 类型位于 `core/image`，只包含 XQ
  和标准库类型。所有 `itk::` 类型只存在于 adapter `.cpp`。
- 输入必须是 LPS、单分量、有限且非空的三维几何。`IVoxelSource::meta()` 必须
  与 `XQImageVolume` 的 dimensions/type/components 一致；每次 run 只调用一次
  `acquire_whole()`。
- XQ -> ITK 导入设置完整 dimensions、spacing、origin、3x3 direction，标量按
  canonical rescaled buffer 转成 float；不重设 identity，不逐 slice 执行。
- diffusion 必须 `SetUseImageSpacing(true)`。三维 curvature diffusion 的 profile
  timestep 同时满足 `<= 0.0625` 和 `<= minSpacing / 16`；不满足时在进入 ITK
  前返回 `DiffusionTimeStepUnstable`。
- multi-scale Hessian 的 sigma minimum/maximum/steps 以 mm 传给 ITK；管状目标
  固定 `ObjectDimension=1`，首个 portal-venous profile 使用 bright object、
  scale objectness、logarithmic `0.6..4.0 mm` 六尺度。
- diffusion 和 vesselness 各自显式 `Update()`、`DisconnectPipeline()`，阶段间检查
  cancellation，并尽早释放前一阶段 ITK buffer。最终结果必须复制到 XQ 自有
  Float32 buffer 后再返回。
- 成功输出记录完整 profile、输入/profile/output SHA-256 fingerprint、algorithm
  id/version、ITK version、输入/输出 range、正响应 voxel 数和耗时。
- 任意 invalid geometry/source/profile、非有限标量、ITK/内存异常、取消或全零
  vesselness 均返回空 `output`；不得把 ITK exception 文本或 DICOM free text
  放进诊断。

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| 非 LPS、空/溢出 dimensions、非有限 geometry、非正 spacing、奇异 direction | `InvalidGeometry`; no source acquire/output |
| source meta/view 与 image 不同或 byte count 不符 | `InvalidSource`; no ITK execution/output |
| 多分量或 Unknown scalar | `UnsupportedInput`; no output |
| float/double 输入含 NaN/Inf 或超出 float range | typed input diagnostic; no output |
| timestep 超过 `min(0.0625, minSpacing/16)` | `InvalidProfile` + `DiffusionTimeStepUnstable` |
| cancellation before/between/final materialization stages | `Cancelled`; discard all partial state |
| ITK `MemoryAllocationError` / `std::bad_alloc` | `AllocationFailed`; no output |
| ITK exception、output geometry drift、非有限 output | `ProcessingFailed`; no output |
| vesselness 没有任何正响应 | `EmptyOutput` + `EmptyVesselness` |
| complete finite non-empty output | `Ok`, stage `Complete`, valid XQ-owned Float32 volume |

### 5. Good/Base/Bad Cases

- Good: derived IRCAD case `512x512x139` 在 ITK 5.4 上运行两次，得到相同 output
  fingerprint、`0..317.78466796875` range、`16,455,684` 个正响应 voxel，且
  output geometry 与输入逐值相等。
- Base: 倾斜 direction、各向异性 spacing 的合成高斯管走相同 3D adapter；两种
  sampling 的等物理管径保持相同尺度响应趋势。
- Bad: 对每张 axial slice 跑 2D filter 后拼回 3D；用自写 finite-difference/
  Hessian/eigen fallback；把 gold mask、Path 或 seed 传给预处理器；返回仍借用
  ITK pixel container 的 XQ 结果。

### 6. Tests Required

- `test_itk_vascular_preprocessor`: 真正执行 ITK 3D filters；断言一次 whole acquire、
  oblique LPS geometry 守恒、物理 spacing 趋势、逐字节确定性、取消、非法 profile
  和非有限输入零半结果。
- `test_arch_boundaries`: core/adapter public headers 无 ITK/VTK/GDCM/Qt 类型。
- `xq_vascular_preprocess_probe`: 在 accepted enhanced CT case 上断言非空有限输出、
  exact geometry、ITK version、fingerprints 和重复运行稳定性。
- 修改本链后重建正式 Release 产品图并运行 full CTest；全绿仅是回归证据，自动
  分割和完整壳仍由后续任务验收。

### 7. Wrong vs Correct

#### Wrong

```cpp
for (int z = 0; z < depth; ++z) {
    customDiffusion2D(slice(z));
    customHessianEigen2D(slice(z));
}
```

#### Correct

```cpp
using Image = itk::Image<float, 3>;
auto diffusion = itk::CurvatureAnisotropicDiffusionImageFilter<Image, Image>::New();
diffusion->SetUseImageSpacing(true);

auto objectness =
    itk::HessianToObjectnessMeasureImageFilter<HessianImage, Image>::New();
objectness->SetObjectDimension(1);

auto multiscale =
    itk::MultiScaleHessianBasedMeasureImageFilter<Image, HessianImage, Image>::New();
multiscale->SetHessianToMeasureFilter(objectness);
// Materialize to XQ-owned bytes before returning.
```
