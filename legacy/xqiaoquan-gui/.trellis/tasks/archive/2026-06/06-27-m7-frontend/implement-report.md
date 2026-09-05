# M7 前端里程碑 — 实现报告

状态:**完成,全部门槛达成**。全新独立 build 目录 `build_m7` 构建零错误;全量 ctest(Release + offscreen)44/44 真绿;两处假绿抽查均按预期 FAIL→恢复 PASS;0007 端到端工作流真跑通(6 阶段节点入树 + 全程 undo 回初始 + redo 恢复);Controller/core/services 无 Qt/VTK;Controller 只经命令栈改 scene。

---

## 1. 新增 / 改动文件清单

### 新增 —— 6 个 Controller(纯 C++,只 include core + services,无 Qt/VTK)
- `src/ui/controllers/PathController.{h,cpp}` — `addPath(intent)`:控制点+源影像 → PathService::createPathCommand
- `src/ui/controllers/SegmentationController.{h,cpp}` — `threshold/regionGrow/aiSegment(intent[,backend])`:SegmentationService + AiService.segment(mock backend) → createMaskNodeCommand
- `src/ui/controllers/ModelingController.{h,cpp}` — `loft(intent)`:ContourLoftInputBuilder → loftSurface → capModel → createModelNodeCommand
- `src/ui/controllers/MeshingController.{h,cpp}` — `buildSurfaceMesh/buildVolumeMesh(intent)`:从 model 节点 payload 读 XQSurfaceModel → SurfaceMeshService/VolumeMeshService build*MeshCommand
- `src/ui/controllers/FlowController.{h,cpp}` — `solve(intent)`:BoundaryConditionService::validateAndBind → FlowSolver1D::solve → buildFlowResultCommand
- `src/ui/controllers/AiController.{h,cpp}` — `analyzeFlow(intent)`:从 flow 节点 payload 读 XQFlowResult → FlowMetricsService::analyzeFlowCommand

### 新增 —— 测试
- `tests/ui/controllers/WorkflowControllerTest.cpp` — 6 Controller 无头链路测试(纯 C++,合成输入;每 Controller:意图→scene 新增正确 domain 节点+派生关系+undo 回退+redo 恢复+非法意图失败)
- `tests/app/WorkflowIntegrationTest.cpp` — 0007 端到端工作流(真实 .vti/.ctgr/.flow;AI 用 mock backend)
- `tests/io/test_new_domain_roundtrip.cpp` — 新 domain(flow_result/ai_analysis)结构 round-trip

### 改动
- `src/app/XQMainWindow.{h,cpp}` — **保留** 现有 `XQMainWindow()` 默认构造 + `setScene(const XQScene*)`(test_main_window 不破);新增 `attachWorkflow(XQScene*, XQCommandStack*)`(持可变 scene + 命令栈 + 6 Controller + 右侧 QStackedWidget 阶段面板 `xqStagePanel` + Edit 菜单 undo/redo,undo/redo 后刷新树);新增 6 个 Controller 访问器 + `undo()/redo()`。析构改 out-of-line(unique_ptr 持不完整类型)。
- `tests/app/test_main_window.cpp` — 保留原有断言;追加一段 attachWorkflow 覆盖(阶段面板存在 + Controller 已挂 + 经 PathController 加路径 + 窗口 undo/redo 生效)。
- `CMakeLists.txt` — 新增 `xq_controllers` 静态库(纯 link `xq_core` + `xq_services`,**不 link Qt**);链进 `xq_app_shell`;新增 `test_workflow_controllers`(link `xq_controllers`)、`test_workflow_integration`(link `xq_controllers`+`xq_io`+`xq_adapter_vtk`,带 XQ_CTGR_DIR/XQ_FLOW_DIR/XQ_TEST_VTI_PATH + VTK autoinit + VTK/tinyxml2 PATH env)、`test_new_domain_roundtrip`(link `xq_io`);3 条 add_test。
- 构建/测试脚本:`build_m7.bat`、`ctest_m7.bat`(照用既有配方:vcvars64 + CMAKE_PREFIX_PATH 多根 + Ninja Release;ctest 设 `QT_QPA_PLATFORM=offscreen`)。

> 未改:`../PROGRESS.md` 在 git status 里显示 modified,但**非本里程碑改动**(我未触碰),按纪律未动。

---

## 2. ctest 结果数字

全新 `build_m7` 目录,构建 119/119 零错误。ctest(Release + offscreen):

```
100% tests passed, 0 tests failed out of 44
Total Test time (real) = 4.38 sec
```

- M0~M6 原有 41 测试:全绿。
- M7 新增 3 测试:`test_workflow_controllers` / `test_workflow_integration` / `test_new_domain_roundtrip` 全绿(41 + 3 = 44,符合预期 ≥43)。
- `test_onnx_backend` 仍在 `XQ_ENABLE_ONNX`(默认 OFF)块内不注册——onnxruntime 未装,AI 走 mock backend。

---

## 3. 假绿抽查证据(两处,均 Release)

**抽查 A — 不 push 命令**:篡改 `ModelingController::loft`,注释掉 `stack_->push(...)`(命令构造成功但不提交)。
- 结果:`test_workflow_controllers` **FAIL** — `FAIL: domainOf(scene, modelId) == xq::XQDomainType::SurfaceModel (line 426)`,exit=1。
- 恢复 push → **PASS**(`OK: all six workflow controllers ...`,exit=0)。

**抽查 B — push 错 domain**:篡改 `AiController::analyzeFlow`,改 push 一个 `XQDomainType::Mesh` 的 AddNodeCommand(而非 analyzeFlowCommand 产出的 AiAnalysis)。
- 结果:`test_workflow_controllers` **FAIL** — `FAIL: domainOf(scene, analysisId) == xq::XQDomainType::AiAnalysis (line 575)`(期望 AiAnalysis 实得 Mesh),exit=1。
- 恢复 → 全量 ctest **44/44 PASS**。

→ 测试确实校验真实 scene 变更与 domain,非 assert 假绿(全程 CHECK 宏,无副作用进 assert)。

---

## 4. 端到端工作流验证(0007 主线,真跑通)

`test_workflow_integration` 输出:
```
0007 workflow: 9 stage commands, 10 nodes
  (image+path+mask+group+model+surfMesh+volMesh+case+flow+analysis)
OK: 0007 end-to-end workflow (path->seg->model->mesh->flow->ai),
    full undo to initial + redo restored
```

链路(单一 scene + 单一 XQCommandStack,真实数据):
1. **种子**:image 源节点直接入 scene(作为"初始状态"根)。
2. **Path**:PathController 控制点(无头程序化给点)→ path 节点(derived from image)。
3. **Segmentation**:VtkImageAdapter 解码真实 `OSMSC0090-cm.vti` → SegmentationController.aiSegment(mock backend)→ mask 节点。
4. **Modeling**:contour-group 节点经命令栈入树(M0 AddNode 命令);ModelingController 用真实 `aorta_final.ctgr` loft+cap → model 节点(derived from contour group)。
5. **Meshing**:MeshingController 从 model 节点读 payload → surface mesh 节点 → volume mesh 节点(derived from surface mesh)。
6. **Flow**:从真实 ctgr 截面积建 A0(x) + 解析真实 `inflow_1d.flow` 波形;case 节点经命令栈入树;FlowController validateAndBind + FlowSolver1D::solve(CFL 触发则缩 dt 重试一次,同 FlowIntegrationTest)→ flow 节点(derived from case)。
7. **AI**:AiController 从 flow 节点读 payload,按其最小段时均压抬到生理基线(60 mmHg,同 AiIntegrationTest)→ analyzeFlowCommand → analysis 节点(derived from flow)。

**各阶段 domain 节点全部断言入树**:Image / Path / SegmentationMask / ContourGroup / SurfaceModel / Mesh(surf)/ Mesh(vol)/ SimulationCase / FlowResult / AiAnalysis。

**undo/redo 全程**:9 条命令 → `undo()` × 9 → scene 回到初始(仅 image 节点,`!can_undo()`,path/model/flow/analysis 全消失)→ `redo()` × 9 → 全部阶段节点恢复(`!can_redo()`,节点数回到 10)。

---

## 5. 纪律复核

- **Controller 纯 C++**:`rg "#include <Q|#include <vtk"` 在 `src/ui/controllers/` + `src/core/` + `src/services/` 命中 0。Qt/VTK 只在 ui widget(XQSceneModel)/ app(XQMainWindow)/ visualization(XQImageViewer)。
- **Controller 只经命令栈改 scene**:`rg "scene_->insert|remove|link_derived|mark_source_changed|clear"` 在 `src/ui/controllers/` 命中 0;只有 `find`(读)+ `stack_->push`(7 处)。
- **CHECK 宏**:三个新测试全用 `if(!(cond)) return fail(#cond,__LINE__)`,无 assert,Release 不被 /DNDEBUG 删。
- **测试文件齐全**:CMake 注册的 3 个新测试源均存在;6 个 Controller .h/.cpp 齐全。

---

## 6. 技术债(本里程碑明确不做,记录待后续)

- **W1 scene 可变性收紧**:高风险、plan 非强制。本里程碑守"UI 不绕过 service/命令改 scene"纪律(Controller 持 `XQScene*` 但只 `find` 读 + `stack->push`,从不调 scene mutator);W1(收紧 scene mutator 边界、波及大量 fixture)留独立清理。
- **payload 实体数据持久化**:现有存档(XQProjectWriter)只序列化节点结构(id/domain_type/display_name/派生关系/stale),不存 payload 实体(几何/掩膜/网格/流场)。`test_new_domain_roundtrip` 只验**结构级** round-trip(新 domain 的 domainTypeToString 令结构自动支持);实体持久化是独立大议题,不在 M7。
- **MainWindow 阶段面板首版极薄**:右侧 QStackedWidget 为 6 个占位页(标签 + objectName),证明集成结构存在;真正的"widget 输入→Controller"细化与真实交互取点(MPR 鼠标拾取)首版以无头程序化覆盖,GUI 手动操作非自动验收项。AiController 首版只覆盖纯数值 flow-metrics 路径;identify/surrogate AI 面板(经 AiService + backend)留后续。

---

## 7. 抉择记录(从 plan/参考自定,未停下问)

- **contour group 无 payload 类型**:核查发现 core 无 `XQContourGroupPayload`(contour group 由 reader/分割阶段产出,非 payload 节点)。故 ModelingController 的 intent **以值携带 `XQContourGroup`** + contour-group 节点 id(用于 source 绑定),而非从 scene payload 读——保持壳薄、source 绑定权威。其余阶段(model/mesh/flow)上游对象都有 payload 类型,Controller 从 scene 节点直接读回。
- **FlowController intent** 携带 `XQSimulationCase`(BC 校验)+ `XQMesh`(校验靶)+ `FlowSolver1D::SolverInput`(中心线/波形/RCR/时间方案):校验+求解编排在壳里,数值全在 service。
- **端到端的 contour-group / case 节点** 经命令栈用 M0 `AddNodeWithSourceRelationCommand` 入树(scene 命令本就供上层用),使整条工作流可一路 undo 回初始;image 种子节点直接 insert 作为"初始状态"根。
