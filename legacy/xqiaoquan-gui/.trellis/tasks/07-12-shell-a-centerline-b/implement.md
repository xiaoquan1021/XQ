# Implementation Plan: 壳档 A ITK 中心线 B

## Step 1 - XQ-owned skeletonizer contract and real ITK adapter

- 添加 `core/path/ICenterlineSkeletonizer3D` 的 typed status/output 契约。
- 添加 `ItkCenterlineSkeletonizer3D`，复用 vendored ITKThickness3D 与 ITK 5.4
  SignedMaurer；严格校验 geometry/source/binary values，只 acquire whole 一次。
- 注册到现有 `xq_adapter_itk`，不新增依赖版本、DLL 或第二 ITK。
- 先以真实 adapter focused test 证明 3D thinning、物理半径、oblique LPS 和失败语义。

## Step 2 - Pure graph and domain assembly

- 添加 `CenterlineBGraph`：确定性 26 邻域、单连通检查、mm 短毛刺剪枝、endpoint
  geodesic diameter 与稳定 tie-break。
- 添加 `CenterlineBService`：组装现有 `XQPath` 与 segmentation-derived
  `VesselProfileV1`，生成 canonical SHA-256 fingerprints。
- 复用现有 Profile/Path validators、snapshot 与 geometry smoke；任一失败零输出。
- 用 fake skeletonizer 覆盖直管、弯管、分叉/毛刺、断裂/环、非正 radius 和重复运行。

## Step 3 - Atomic controller and project integration

- 为 `AssetRegistry` 增加只读 next-id 查询，供 prepared atomic command 选择未占用
  AssetId，不预注册、不复用已发 id。
- 添加 `CenterlineBController` 的 capture/compute/commit，捕获 mask/image source guards，
  准备 Path/Profile 两个 batch，并用 `ProjectNodeBundleCommand` 单次提交。
- 覆盖 target/source race、失败零副作用、完整 Scene/Asset lineage、stale、undo/redo、
  保存重开与 strict source identity。

## Step 4 - Existing Modules workflow wiring

- `XQWorkflowSession` 无条件持有 ITK skeletonizer 与 CenterlineB controller。
- 在 Modules 页 Path 输入区增加 mask 选择与 Centerline B 操作；成功后选中新 Profile，
  继续走现有 PathValidate/dump，不创建平行页面或 Flow 控件。
- 更新 `XQStageWidgets`、MainWindow provider、对象名、状态文本和 TS/QM。
- 增加 controller、workflow session、main-window/shell GUI 的可操作性回归。

## Step 5 - Build, architecture and acceptance evidence

- 更新 CMake sources、tests、ITK runtime test environment 和 public-header guard。
- 顺序运行 focused Release tests：

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
test_main_window
test_shell_a_gui
test_arch_boundaries
```

- 最后运行 canonical Flow ON full Release CTest，再运行 Flow OFF full Release CTest；
  不并发、不以重试隐藏失败。
- 将命令、结果、backend identity、non-claims 和人工 GUI pending 写入 task evidence；
  不提交、不归档，除非用户明确批准。

## Rollback Points

- adapter 契约与 graph/service 可独立保留，即使 GUI 接线回滚也不影响旧 Path/Modules。
- controller bundle 失败必须自身回滚，禁止用清理 Scene/Registry 的补偿脚本。
- 不删除旧 build tree、不 reset/revert 用户改动、不修改 D:\XQ。

## Forbidden Actions

- 不实现 Frangi/Sato、自动分割、病例特定 seed/ROI、完整中心线树或 vtkvmtk 生产路线。
- 不添加 Python runtime、动态插件、Flow/mesh/1D 能力。
- 不持久化 `CenterlineGraph` 或新增第二份 Path/radius 权威。
- 不让 ITK/VTK/Qt 类型进入 core/service/controller public API。
- 不使用 Claude/Claude provider/channel/subagent；本任务 inline 执行。
