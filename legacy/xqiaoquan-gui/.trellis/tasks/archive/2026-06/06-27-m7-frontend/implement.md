# M7 前端 — 实现清单(implement)

> 严格按 design.md。壳薄:UI 只调 service、读 scene、经命令栈提交;**不实现领域算法**;不绕过 service/命令直接调 scene mutator。
> Controller 纯 C++(只 include core + services,不 include Qt)→ 无头可测。Qt/VTK 只在 widget/viewer。不破 M0~M6 的 41 测试。

## 步骤 1:6 个 Controller(src/ui/controllers/,纯 C++,只 link xq_core + xq_services)

每个 Controller 持 `XQScene* scene_` + `XQCommandStack* stack_`(构造注入),方法:收意图 → 调 service 拿 CommandResult → stack_->push → 返回状态。**绝不直接调 scene mutator**(只经命令栈)。

- `PathController`:`addPath(intent) -> Status`。intent = 控制点 vector + 名称 + sourceImage 节点 id。调 PathService 产路径命令(核查 PathService 公开 API 的命令产出方法名)。
- `SegmentationController`:`threshold(intent)/regionGrow(intent)/aiSegment(intent, backend) -> Status`。调 SegmentationService(threshold/regionGrow 命令)+ AiService.segment(mock backend)。
- `ModelingController`:`loft(intent) -> Status`。intent = contour group 节点 id + 名称 + 参数。调 ModelingService::loftSurfaceCommand(+ 可选 cap via capModel→createModelNodeCommand)。
- `MeshingController`:`buildSurfaceMesh(intent)/buildVolumeMesh(intent) -> Status`。调 SurfaceMeshService/VolumeMeshService 的 build*MeshCommand。
- `FlowController`:`solve(intent) -> Status`。intent = case(边界/RCR/inflow)+ 截面 + 中心线。先 BoundaryConditionService 校验绑定,再 FlowSolver1D::solve → buildFlowResultCommand。
- `AiController`:`analyzeFlow(intent) -> Status`。intent = flowResult 节点 id。调 FlowMetricsService::analyzeFlowCommand。

> 各 service 命令产出方法名/签名:动手前 rg 各 service .h 确认(PathService/SegmentationService 的命令方法可能与 M3+ 的 *Command 命名不同;按实际签名调用,别臆造)。

## 步骤 2:XQMainWindow 集成(src/app/)

- XQMainWindow 增持可变 scene 接入 + `XQCommandStack` 成员 + 6 个 Controller(挂在右侧阶段面板)。
  - **保留现有 `XQMainWindow()` 默认构造 + `setScene(const XQScene*)`**(test_main_window 不破);新增可变 scene + 命令栈的接入方法(如 `attachWorkflow(XQScene*, XQCommandStack*)` 或新构造)。
  - 右侧 dock:QStackedWidget 阶段面板(路径/分割/建模/网格/流体/AI),每个 widget 薄壳收输入 → 调对应 Controller。
  - Edit 菜单 undo/redo → commandStack;树选中驱动面板上下文。
- 首版 widget 可极薄(按钮 + 最小输入);核心逻辑在 Controller。

## 步骤 3:CMake 注册

- xq_ui(或新 xq_controllers 静态库,纯 C++ 只 link xq_core + xq_services)加 6 个 Controller.cpp。
  - **关键**:Controller 库**不 link Qt**(纯 C++),这样 Controller 测试不需 Qt。widget/MainWindow 在 xq_ui/xq_app_shell(link Qt)。
- xq_app_shell 加 widget 面板(若新增)。
- 新增 test_workflow_controllers(纯 C++,link controller 库 + xq_services + xq_io)+ WorkflowIntegrationTest;add_test。
- 现有 ui 测试目标不动。

## 步骤 4:测试(全 CHECK 宏 / 现有 UI 风格)

- `tests/ui/controllers/WorkflowControllerTest.cpp`(纯 C++,核心):
  对每个 Controller:构造 XQScene + XQCommandStack → 准备前置节点(如 contour group / model / flowResult)→ 给意图调 Controller → 断言:scene 新增正确 domain 节点 + 派生关系 + Controller 返回 Ok + stack.undo() 回退节点/关系 + redo() 恢复。非法意图 → 对应失败状态。
- `tests/app/WorkflowIntegrationTest.cpp`(端到端,读真实 0007):
  构造 scene + 命令栈 → Path→Seg→Model→Mesh→Flow→Ai 依次调 Controller(AI 用 mock backend)→ 断言树最终含各阶段 domain 节点 + 全程 undo 回初始 + redo 恢复。
- 现有 test_main_window / test_scene_model / test_image_viewer 保持绿(offscreen)。

## 步骤 5:存档结构 round-trip(轻)

- 验证新 domain(FlowResult/AiAnalysis 等)节点经 XQProjectWriter 写 + XQProjectReader 读回结构一致(复用现有 test_project_roundtrip 风格,补一条覆盖新 domain 的结构 round-trip;**只验结构:id/domain/display_name/关系/stale**,payload 实体持久化是技术债不测)。

## 完成门槛(worker 自检)

- 全新 build 目录构建零错误;全量 ctest 真绿(M0~M6 的 41 + M7 新增,预期 ≥43)。
- 假绿抽查:篡改某 Controller(如让它不 push 命令 / push 错 domain) → WorkflowControllerTest Release FAIL → 恢复 → PASS。写进汇报。
- grep 确认 Controller 源与 core/services 无 Qt/VTK include(Controller 纯 C++);Qt/VTK 只在 ui widget/app/visualization。
- UI 不绕过 service/命令改 scene(Controller 只经 stack->push,不直接调 scene.insert/remove)。
- 小抉择(widget 布局、intent 字段、面板切换)参考 plan/SimVascular UI 自定合理默认,别停下问。
- 汇报写 `.trellis/tasks/06-27-m7-frontend/implement-report.md`:文件清单、ctest 数字、假绿证据、端到端工作流验证(各阶段节点 + undo/redo)、技术债。

## 注意
- onnxruntime 未装 → AI 阶段用 mock backend(同 M6)。
- W1 scene 收紧不在本里程碑(记技术债);但 Controller 必须守"只经命令栈"纪律。
- payload 实体持久化不在本里程碑(现有存档只存结构)。
</content>
