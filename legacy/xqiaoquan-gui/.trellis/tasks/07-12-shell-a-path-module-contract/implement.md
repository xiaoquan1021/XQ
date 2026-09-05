# Implementation Plan: 壳档 A Path 与模块契约

## Step 1 — Core contract

- 添加 `XQVesselPath.h/.cpp`、station/source/radius enums、validator issues。
- 覆盖版本、frame、source、输入 stamp、点数、NaN/Inf、非正半径和 arc 非降矩阵。

## Step 2 — Snapshot, dump and geometry smoke

- 添加 `VesselPathSnapshotService`，复用 `VesselProfileValidator` 并实现 `sqrt(area/pi)` 与最小声称来源映射。
- 添加 stable `VesselPathDump`，禁止 PHI/free-text 输出。
- 添加 `ShellGeometrySmokeService`；只消费 Path，失败时零输出。

## Step 3 — Static module registry

- 添加 Path-only module interface/descriptor/result。
- 固定注册 Noop 与 PathValidate。
- 覆盖枚举/运行、重复 ID、未知 ID、invalid Path 与显式 module failure。

## Step 4 — Controller and replacement Modules GUI wiring

- 添加 `PathModuleController`，从 Profile node 构造 stamp 并调用共享 services。
- `XQWorkflowSession` 在 ON/OFF 下都构造并暴露 controller。
- 用 Modules 页整体取代第 5 页旧 Flow 壳功能；把 Profile 装配并入 Path 输入准备，并以 Path module 作为唯一壳执行入口。
- 删除旧 Flow source/protocol/run 控件、Flow capability 状态与 MainWindow 输出 ID 接线；Flow ON/OFF 页面形态一致。
- 同步 `xq_zh_CN.ts/.qm` 与 GUI 回归。

## Step 5 — Build registration and focused verification

- 更新 `CMakeLists.txt` 的 core/services/controllers/tests 源与测试注册。
- 先构建受影响 targets，再运行新增 core/service/controller/GUI tests、`test_arch_boundaries`、`test_workflow_capabilities`。
- 只有实现稳定后才各运行一次 canonical Flow ON/OFF full Release CTest，严格串行；不做无目的重复测试。

## Expected Focused Tests

```text
test_vessel_path
test_vessel_path_snapshot
test_shell_geometry_smoke
test_path_module_registry
test_path_module_controller
test_workflow_session
test_workflow_capabilities
test_main_window
test_arch_boundaries
```

## Forbidden Actions

- 不给 `XQPath` 或 Scene 增加第二份可写半径权威。
- 不新增 Path snapshot payload/project schema。
- 不让 module interface 接受 Scene、Contour、Flow、Qt、ITK 或 VTK 类型。
- 不把 Path modules 放进 `XQ_ENABLE_FLOW` 条件块。
- 不引入动态插件、通用 Context/OperationRegistry 或 Noop Flow solver。
- 不使用 Claude/Claude provider。
- 不并发运行 ON/OFF full CTest，不 `git add .`，不清理旧树或用户改动。
