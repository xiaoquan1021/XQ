# Design：Flow 能力可关

## Runtime model

新增小型 `WorkflowCapabilities` 值对象，默认与当前行为兼容：Path/Segmentation/Modeling/Meshing/AI 按现状，Flow 可显式开关。`XQWorkflowSession::attach` 只在 Flow 可用时构造现有 controller；提供集中 `hasFlowCapability()` 查询，UI 只消费该投影。

不引入统一 `IModule::run(Context&)`。未来能力仍采用现有 `ITetMesher`、`ILevelSetSegmenter` 一类窄 typed port。

## Build model

`XQ_ENABLE_FLOW` 默认 ON。OFF 时：

- 不编译/链接 FlowSolver1D、BoundaryConditionService、FlowController 和 Flow 执行 UI；
- core 中 SimulationCase/FlowResult 及 project reader/writer 保留，确保历史项目可读；
- protocol stamp、station/source-weight mapping、conversion record、source revisions 等持久 DTO 全部留在 core/io；Flow service 仅提供运行时 protocol 常量和执行逻辑；
- 非 Flow controller/service/app 仍构建；
- Flow 专属 tests 条件注册，另注册 no-flow shell tests。

条件编译集中在 target source 列表、WorkflowSession 私有成员和 Flow stage factory，避免遍布业务代码。

新增可复制执行的 `XQ/build_shell_noflow_wt.bat`，复用已验证 `build_gui_wt.bat` 的 vcvars64、Ninja、Release、完整 `CMAKE_PREFIX_PATH` 与 `XQ_TEST_DATA_ROOT`，仅使用独立 `build_shell_noflow` 并追加 `-DXQ_ENABLE_FLOW=OFF`。不得用缺 generator/prefix 的裸 clean configure 作为验收命令。

## UI behavior

Flow action/page保持可解释的 disabled/unavailable 状态并显示“当前构建未包含 Flow 执行能力”。不得隐藏历史结果节点，不得将 unavailable 当作运行成功。

## Tests

- runtime config ON/OFF session tests；
- MainWindow disabled state；
- OFF build project load/render/history test；
- ON build smoke 回归。
