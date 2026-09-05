# Evidence: 壳档 A ITK 中心线 B

日期：2026-07-14

## 实际行为

- Modules 页从现有 `SegmentationMask` 选择输入，点击“Build Centerline B”。
- 后台通过真实 ITK 三维 thinning 与物理距离图生成单主路径；owner thread 以一个
  `ProjectNodeBundleCommand` 原子发布 Path、VesselProfile、两个 derived Asset 及
  Scene/Asset lineage。
- 成功后自动选择新 VesselProfile，可继续运行现有 `PathValidate`；一次 undo/redo
  同时撤销/恢复两个节点、两个 Asset 和关系。
- Flow ON/OFF 都存在该入口；没有增加分割、Flow、网格或完整中心线树能力。

## 前人实现与依赖身份

- thinning：ITKThickness3D `v5.3.0`，commit
  `36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb`，以 header-only filter 复用，
  没有引入第二套 ITK。
- `itkBinaryThinningImageFilter3D.h` SHA-256：
  `73E9537C5CB6AFC3EC9ECB809D0DE73A93D7C923D00B938F28A3E83447177EDA`。
- `itkBinaryThinningImageFilter3D.hxx` SHA-256：
  `C7101F1AC5EF309BC60416DCBFE7C5B188CBA323D60BB8017D66C5D065C4BCD6`。
- radius：现有 ITK `5.4.0` 的
  `itk::SignedMaurerDistanceMapImageFilter`，`UseImageSpacing=true`、非平方距离、
  内部为正；输入/输出保持完整 LPS/mm geometry。
- runtime 没有新增 vmtk、MITK、Python 或动态插件依赖。

## 聚焦验证

Qt 6.7 `lrelease`：

```text
Generated 456 translation(s) (456 finished and 0 unfinished)
```

ON Release 聚焦 CTest：16/16 通过，包括：

```text
test_itk_centerline_skeletonizer_3d
test_centerline_b_graph
test_centerline_b_service
test_centerline_b_controller
test_vessel_path_snapshot
test_shell_geometry_smoke
test_path_module_controller
test_project_node_batch_command
test_asset_registry
test_workflow_session
test_workflow_capabilities
test_main_window
test_shell_a_gui
test_centerline_b_gui
test_i18n_resources
test_arch_boundaries
```

`test_centerline_b_gui` 使用 disclosed synthetic non-PHI 三维管状 binary mask，
真实执行 ITKThickness3D + SignedMaurer，并验证点击、后台任务、Path/Profile 发布、
Profile 自动选择、PathValidate、stale、undo/redo。`git diff --check` 通过。

## Canonical Release 证据

命令：

```bat
XQ\verify_canonical_shell_a.bat
```

run id：`20260714-151527`

日志：`.trellis/workspace/ocean/shell-a-canonical-logs/20260714-151527/`

- source contract：PASS。
- Flow ON：fresh configure + clean-first build + build graph/PE closure PASS；
  focused 8/8；full CTest 105/105。
- Flow OFF：fresh configure + clean-first build + build graph/PE closure PASS；
  focused 8/8；full CTest 97/97。
- 两套 full CTest 严格串行，`CTEST_PARALLEL_LEVEL=1`。
- ON/OFF 均实际编译并运行 Centerline B adapter、graph、service、controller 和 GUI test。
- explicit-package negative probes 7/7 PASS。
- 锁定 Qt 6.7.0 闭包通过，Qt6Core SHA-256：
  `1E28DFCE1891482756B983648C31E0BF251F00E81CF413B8DBC124B52E3145D9`。

## 真实机器基础壳验收

2026-07-14，用户在真实 Windows 桌面确认：

- 必须通过 `XQ\run_xq.bat` 启动。直接双击
  `build_shell_a_on\xq_app.exe` 会因未设置依赖 PATH 而报告缺少
  `vtkRenderingLOD-9.3.dll` 等 DLL；该直接启动方式无效。
- 从 `D:\XQ\data\dicom\tcia_lidc_idri_0957_ct\series` 打开包含
  `1-01.dcm` 到 `1-65.dcm` 的真实 Series 成功。
- MPR/3D 显示和浏览成功。
- 保存 `.xqproj`、关闭、重新通过 `run_xq.bat` 启动并打开该项目后，影像恢复成功。

这关闭了真实启动、DICOM IO、基本显示和持久化检查，不关闭本 child 的自动 Centerline B
实机检查。

## Pending 与非声称

- Centerline B 的人工 GUI 验收仍 pending：尚未由正常 DICOM 产品流程生成兼容
  `SegmentationMask`，并在真实桌面点击 `Build Centerline B` 后确认 Path/Profile、
  `PathValidate`、保存和重开。
- 首次产品输入是 DICOM；`.xqproj` 只是保存/恢复格式。`D:\XQ` 中没有预制
  `.xqproj` 不是产品依赖，也不得再作为“无法开始验收”的理由。
- 当前 production Centerline B 复用了 ITKThickness3D；locked
  ITKMinimalPathExtraction 仍只接入 development analyzer，尚未进入正常 GUI/生产 target。
- 用户明确要求用前人自动 Path 工作取代可替代的旧手工 Path。手工
  `Pick Control Points`/Contour 旁路不能作为目标自动流程的验收替代品。
- 当前证据只证明 vmtk-OFF 的壳档 A 单路径 fallback 可装配。不得据此声称完整自动
  DICOM->Mask->Path、完整中心线树、临床半径准确性、Dice/clDice 或真实 CTA 生产质量。
