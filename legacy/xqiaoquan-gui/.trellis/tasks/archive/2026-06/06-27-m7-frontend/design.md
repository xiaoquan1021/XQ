# M7 前端 — 技术设计(design)

> 基于 M0~M6 实际接口核查(现有 Qt 壳骨架 + 各 service 公开 API + 存档模型)+ plan/08。
> 壳薄可替换:UI 只调 service、读 scene、把意图经命令栈提交;**不实现任何领域算法**;不绕过 service 改 scene。

## M0~M6 现状核查(已读源码)

- **现有 Qt 壳骨架**(M0 建,薄):
  - `src/ui/XQSceneModel.{h,cpp}`:QAbstractItemModel,XQScene → 项目树(test_scene_model 覆盖)。
  - `src/app/XQMainWindow.{h,cpp}`:QMainWindow,持 `const XQScene*` + XQSceneModel(test_main_window offscreen 覆盖)。
  - `src/visualization/XQImageViewer.{h,cpp}`:VTK 离屏渲染影像→RGBA(test_image_viewer 覆盖)。
  - 缺:工具面板、工作流串联、命令栈接入、各 service 调用。
- **各 service 公开 API(M1~M6,均"读 const + 产命令")**:
  - PathService / CenterlineFrameService(M1)、SegmentationService(M2)、ModelingService(M3,loftSurfaceCommand)、
    SurfaceMeshService/VolumeMeshService(M4,buildSurface/VolumeMeshCommand)、BoundaryConditionService/FlowSolver1D(M5,buildFlowResultCommand)、
    FlowMetricsService/AiService(M6,analyzeFlowCommand/segment/identify/predictFlow)。
  - 均返回 CommandResult(status + unique_ptr<XQCommand>)或结果对象;入 scene 经命令。
- **命令栈**:`XQCommandStack`(M0):push(执行并入 undo 栈)、undo/redo、can_undo/can_redo。
- **存档模型(关键,已核查)**:`XQProjectWriter` 只序列化节点**结构元数据**(id / domain_type / display_name / 派生关系 / stale),
  **不序列化 payload 实体数据**。新 domain(FlowResult/AiAnalysis,M5/M6 已加 domainTypeToString)→ **结构级 round-trip 自动支持**。
  → M7 验收"关闭/重开 scene 一致"= 节点结构/关系/stale 一致(现有模型已支持);payload 实体持久化是独立技术债,不在 M7。

## 范围界定(收尾里程碑,切清楚)

- **纳入 M7**:6 个工具面板(意图→service→命令→XQCommandStack→scene)+ 主窗口集成(树 + 视图 + 右侧阶段面板 + Edit undo/redo)+ 端到端工作流 + 无头链路测试。
- **不纳入 M7(记技术债)**:
  - **W1 scene 可变性收紧**:高风险(改 mutator 边界 + 波及大量 fixture),plan 非强制。M7 守"UI 不绕过 service 改 scene"纪律即可;W1 留独立清理。
  - **payload 实体数据持久化**(几何/掩膜/网格/流场 round-trip):现有存档只存结构,实体靠 reader 重建;完整持久化是大议题,独立处理。

## 核心设计抉择

### 1. 面板逻辑与 widget 解耦(可无头测试)—— Controller 模式

> plan:工具面板"以无头方式验证 点击→service→命令→scene 变更"。

- 每个阶段一个 **Controller**(纯 C++ / 轻 Qt,不依赖 widget 渲染),持 `XQScene*`(非 const,用于读 + 经命令栈改)+ `XQCommandStack*`:
  - 收集"意图"(参数结构,如路径控制点、阈值、选中的 contour group 节点 id、网格参数、边界条件、AI 请求)。
  - 调对应 service 拿 CommandResult → 经 commandStack->push 提交 → 返回状态供 UI 反馈。
  - **Controller 不含 Qt widget 逻辑** → 可在无头测试里直接 new + 调方法 + 断言 scene 变更。
- 放 `src/ui/controllers/`(或 src/app/controllers):
  - `PathController`、`SegmentationController`、`ModelingController`、`MeshingController`、`FlowController`、`AiController`。
- 对应 Qt widget 面板(`src/ui/panels/` 或 app)只是壳:收集 widget 输入 → 调 Controller。首版 widget 可极薄(甚至仅 main window 集成时挂)。

### 2. 主窗口集成(XQMainWindow 扩展)

- XQMainWindow 改持 `XQScene*`(可变,经命令栈)+ `XQCommandStack`(成员)。保留 const scene 兼容旧 test_main_window(看是否需调整测试——若改签名,同步更新 test_main_window)。
  **最小改**:加一个可变 scene + 命令栈的构造/接入路径,保留原 const 构造或更新测试。
- 右侧 dock:阶段工具面板(QStackedWidget 或 dock 切换),各挂对应 Controller。
- Edit 菜单:undo/redo → commandStack;树选中 → 驱动面板上下文。
- viewer 通过 adapter 消费 scene 节点 payload(不持平行业务图)。

### 3. 工作流串联(每阶段 Controller → service)

| Controller | service(M) | 意图 → 命令 |
|---|---|---|
| PathController | PathService(M1) | 控制点 → 路径节点命令(MPR 取世界坐标首版可程序化给点) |
| SegmentationController | SegmentationService(M2) | 阈值/区域生长种子 → 掩膜命令;AI 分割经 AiService.segment(mock backend) |
| ModelingController | ModelingService(M3) | 选 contour group 节点 → loftSurfaceCommand(+ cap) |
| MeshingController | SurfaceMeshService/VolumeMeshService(M4) | 选模型 → buildSurface/VolumeMeshCommand |
| FlowController | BoundaryConditionService + FlowSolver1D(M5) | 设边界 + 解 → buildFlowResultCommand |
| AiController | FlowMetricsService / AiService(M6) | 选 flowResult → analyzeFlowCommand |

- 所有命令经 XQCommandStack;每步结果入树、可 undo。

### 4. VTK / Qt 边界

- VTK 仅在 visualization(XQImageViewer)与 app 渲染路径;viewer 经 adapter 消费 payload。
- **services/core 不依赖 Qt/VTK**(M7 只在 ui/app/visualization 用 Qt/VTK)。Controller 尽量纯 C++(可含轻 Qt 但不依赖 widget 渲染);若 Controller 完全纯 C++ 更好测,优先纯 C++(只用 core + services 类型)。
  **决策**:Controller 纯 C++(只 include core + services),不 include Qt → 无头测试直接编译进非 Qt 测试可执行;widget 面板才 include Qt。这样 Controller 测试不需 offscreen Qt。

## 测试(全 CHECK 宏 / 现有 UI 测试风格;不破 M0~M6 的 41 测试)

- **Controller 无头链路测试**(核心,纯 C++,每 Controller 一个或合一):
  `tests/ui/controllers/WorkflowControllerTest.cpp`:对每个 Controller——构造 scene + 命令栈 → 给意图 → 调 Controller → 断言 scene 新增正确 domain 节点 + 派生关系 + undo 回退 + redo 恢复。用合成/真实输入。
- **端到端工作流测试**(集成,读真实 0007):
  `tests/app/WorkflowIntegrationTest.cpp`:打开项目(或构造)→ 依次跑 Path→Seg→Model→Mesh→Flow→Ai 各 Controller → 断言树最终含各阶段节点 + 全程可 undo 回到初始 + redo。**用 mock AI backend(onnxruntime 未装)**。
- **现有 UI 测试保持绿**:test_scene_model / test_main_window(offscreen)/ test_image_viewer。若 XQMainWindow 签名变更则同步更新 test_main_window。
- 存档结构 round-trip:复用现有 test_project_roundtrip;若新 domain 节点需验证可读回,补一条(结构级)。

## 验收对照(plan「M7 验收」)

- [可达] 0007 端到端:打开→树+影像→路径→分割→建模→网格→流体→AI 指标,各步入树可 undo ✅(端到端 Controller 测试,AI 用 mock)。
- [可达] 关闭/重开 scene 结构一致 ✅(现有结构级存档,新 domain 已支持)。
- [注] 真实交互渲染/鼠标取点首版可程序化(无头);真实 GUI 手动操作非自动验收项。

## 风险

- 范围大(6 Controller + 集成)→ Controller 纯 C++ 解耦,逐个可测,降低风险。
- XQMainWindow 签名若改 → 同步 test_main_window。
- W1 未做 → UI 必须经 service/命令改 scene(纪律),不直接调 mutator;Controller 持 XQScene* 但只经命令栈 push,不调 scene mutator(与 service 同纪律)。
- onnxruntime 未装 → AI 阶段用 mock backend(同 M6)。
- 不破 M0~M6 的 41 测试。
</content>
