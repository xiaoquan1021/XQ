# M5 流体 — 实现清单(implement)

> 严格按 design.md。零外部依赖(core/services 不 include VTK/ITK/求解器/Python)。命令复用 M0。测试用 CHECK 宏(Release 真绿)。
> 全程 CGS 单位。不破 M0~M4 的 35 测试(尤其 test_simulation_case 依赖现有枚举值/顺序)。

## 步骤 1:XQDomainType 加 FlowResult 枚举

文件:`src/core/XQDomainType.h`
- enum **末尾追加** `FlowResult`(SimulationCase 之后),现有值不动。
- domainTypeToString switch 加 `case FlowResult: return "flow_result";`。
文件:`src/core/XQScene.cpp`
- 该文件的 domain switch 加 FlowResult 分支(与其他分支同样处理,参照 SimulationCase 那条)。
- 检查 tests/core/test_payload_and_groups.cpp、src/io/project/SvProjectReader.cpp 是否需补(编译报漏分支才动)。

## 步骤 2:XQSimulationCase 扩展(末尾追加,不破 test_simulation_case)

文件:`src/core/XQSimulationCase.h/.cpp`
- `BoundaryConditionType` 末尾追加 `RCR`、`InletFlowWaveform`(NoSlip/PrescribedVelocity/PrescribedPressure/Resistance 不动)。
- `BoundaryCondition` 加字段(保留 faceId/type/value):
  - `std::vector<double> rcr;`(type==RCR 时 = [Rp, C, Rd])。
  - `std::vector<std::pair<double,double>> flowWaveform;`(type==InletFlowWaveform 时 = (t,Q))。
  - `double waveformPeriod = 0.0;`
- 新增 `struct FluidProperties { double density = 1.06; double viscosity = 0.04; };`(CGS,同 .sjb)。
- 新增 `struct RomSettings { NodeId centerlineNode; std::vector<int> inletFaceIds; std::vector<int> outletFaceIds; double period=0.0; int numTimeSteps=0; double dt=0.0; int numCycles=1; };`
- XQSimulationCase 增持 FluidProperties + RomSettings + getter/setter。原有方法/字段不动。

## 步骤 3:XQSimulationCasePayload(core,新建)

文件:`src/core/XQSimulationCasePayload.h`(仿 XQSurfaceModelPayload)
- : public XQPayload,explicit ctor(XQSimulationCase),domainType()==SimulationCase,clone()(值类型深拷即可,无裸指针),case() const/非 const。

## 步骤 4:XQFlowResult + payload(core,新建)

文件:`src/core/XQFlowResult.h/.cpp`
- `struct FlowSegment { int segmentId; double arcLengthStart; double arcLengthEnd; int faceId; };`(faceId 关联边界,可选)。
- 字段:`std::vector<double> times;`(末周期采样时刻);`std::vector<FlowSegment> segments;`
  每段时间序列:`std::vector<std::vector<double>> flowQ, pressureP, areaA;`(外层 segment,内层 time;或外层 time 内层 segment——定一致并注释)。
- `NodeId sourceCaseNode;` has/set;诊断:`bool converged; double maxCfl;`。
- getter/setter;基本一致性(times.size 与每段序列长度一致)由 service 保证。
文件:`src/core/XQFlowResultPayload.h`
- : public XQPayload,domainType()==FlowResult,clone() 深拷(值类型即可),result() const/非 const。

## 步骤 5:BoundaryConditionService(services/flow)

文件:`src/services/flow/BoundaryConditionService.h/.cpp`(static,纯域,只 link xq_core)
- enum Status { Ok, MissingFace, MissingInlet, MissingOutlet, InvalidRcr, EmptyWaveform, NullScene }。
- `static std::vector<std::pair<double,double>> parseFlowFile(const std::string& text)`:解析两列 `t Q` 文本(逐行,跳空行/非法行),纯字符串。
- `Result validateAndBind(const XQSimulationCase& base, const XQMesh& mesh, ...)`:
  - 每 BC.faceId ∈ mesh.boundaryFaces() 否则 MissingFace。
  - 至少 1 inlet(InletFlowWaveform/PrescribedVelocity)+ 1 outlet(RCR/Resistance/PrescribedPressure)否则 MissingInlet/MissingOutlet。
  - RCR 时 rcr.size==3 且 Rp>0,C>0,Rd>0 否则 InvalidRcr。inlet 波形非空、period>0 否则 EmptyWaveform。
  - 返回校验过/补全的 XQSimulationCase(或诊断状态)。
- 编辑入 scene:`bindCommand(scene, caseNodeId, newCase)` → ReplacePayloadCommand(case 已在 scene)；签名核对 XQSceneCommands.h。

## 步骤 6:FlowSolver1D(services/flow,纯数值,零依赖)

文件:`src/services/flow/FlowSolver1D.h/.cpp`
- enum Status { Ok, EmptyCenterline, InvalidArea, CflViolation, NoInlet, NoOutlet }。
- `struct SolverInput { std::vector<double> arcLength; std::vector<double> area0; /*A0(x)*/ FluidProperties fluid; std::vector<std::pair<double,double>> inletWaveform; double period; std::vector<double> rcr; /*出口[Rp,C,Rd]*/ int numTimeSteps; double dt; int numCycles; };`
- `Result solve(const SolverInput&)` → XQFlowResult。
  - 1D 守恒(A,Q):质量 ∂A/∂t+∂Q/∂x=0;动量含泊肃叶摩擦 K_R=8πμ/ρ;首版壁 rigid。
  - 显式时间推进(Lax–Friedrichs 或两步 LW)+ CFL 检查(dt≤CFL·dx/(|u|+c)),超限 → CflViolation 诊断(不发散)。
  - inlet 施加 Q(t)(波形线性插值,周期);outlet 0D RCR 半隐式。
  - 跑 numCycles,取末周期写 times/Q/P/A。
- **必须实现两条可解析校验的路径**(验收门槛):
  1. 稳态泊肃叶:恒定 Q0、常截面、rigid → 稳态沿程压降与泊肃叶解析吻合。
  2. 单段 RCR 阶跃:恒定 Q0 入 RCR 出口 → P(t)=Q0(Rp+Rd)+(P0−Q0(Rp+Rd))e^{−t/(Rd C)} 吻合。
  - 这两条用专门入口或参数即可触发(如常波形=恒定值、单段)。
- `buildFlowResultCommand(scene, newResultId, name, result, caseNodeId)` → AddNodeWithSourceRelationCommand(source=caseNodeId)。

## 步骤 7:CMake 注册

文件:`XQ/CMakeLists.txt`
- xq_core 加 XQSimulationCase.cpp(已在?确认)+ XQFlowResult.cpp。
- xq_services 加 BoundaryConditionService.cpp、FlowSolver1D.cpp。
- 新增 test_boundary_condition_service / test_flow_solver_1d / test_flow_integration(integration 设 flow-files 路径 + offscreen,仿 modeling/meshing integration)。
- add_test 三条。

## 步骤 8:测试(全 CHECK 宏)

- `tests/services/flow/BoundaryConditionServiceTest.cpp`:见 design。含 parseFlowFile 正确性、各诊断分支、命令 undo。
- `tests/services/flow/FlowSolver1DTest.cpp`:**核心两个解析对照 + 相对误差阈值**(泊肃叶、RCR 阶跃);CFL 超限诊断;XQFlowResult 维度一致;payload 深拷;命令 undo。
- `tests/services/flow/FlowIntegrationTest.cpp`:读真实 0007 inflow_1d.flow + .sjb 的 outflow RCR 值 + contour 截面 → 跑周期解,结构正确、压力量级合理、收敛诊断。

## 完成门槛(worker 自检)

- 全新 build 目录构建零错误;全量 ctest 真绿(M0~M4 的 35 + M5 新增,预期 ≥38)。
- 假绿抽查:篡改 RCR 时间常数(Rd·C)或泊肃叶系数 → 解析对照测试相对误差超阈 Release FAIL → 恢复 → PASS。写进汇报。
- grep 确认 core/services/flow 无 VTK/ITK/求解器/Python include。
- 单位全程 CGS。枚举/字段末尾追加未破 test_simulation_case。
- 小抉择(离散格式细节、误差阈值、插值方式)参考 SimVascular(svOneDSolver/svZeroDSolver 思路)/教科书 1D 血流自定合理默认,别停下问。
- 汇报写进 `.trellis/tasks/06-27-m5-flow/implement-report.md`(含文件清单、ctest 数字、假绿证据、解析对照误差数值、技术债)。
</content>
