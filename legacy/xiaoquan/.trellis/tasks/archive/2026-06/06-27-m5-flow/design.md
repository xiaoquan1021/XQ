# M5 流体 — 技术设计(design)

> 基于 M0~M4 实际接口核查(XQSimulationCase / XQPath / XQMesh boundaryFaces / 命令 / payload)+ SimVascular 仿真设置参考(0090_0001.sjb)。非 plan 文字。
> 应用内 1D/降阶血流求解,**无外部 CFD/Python**。core 零依赖、命令/undo、CGS 单位(与 .sjb 一致)。

## M0~M4 现状核查(已读源码 + .sjb 参考)

- `XQSimulationCase`(core 已有,需扩展):SolverParameters{timeSteps,timeStepSize}、
  `BoundaryConditionType{NoSlip,PrescribedVelocity,PrescribedPressure,Resistance}`、
  `BoundaryCondition{faceId,type,value}`、setSourceMeshNode、addBoundaryCondition、boundaryConditionByFaceId。
  - **关键兼容**:`tests/core/test_simulation_case.cpp` 依赖现有 4 个枚举值及其语义/顺序(NoSlip/PrescribedVelocity/Resistance)。
    扩展枚举**只能在末尾追加**(加 RCR、InletFlowWaveform),不改现有值;BoundaryCondition 加可选字段不删旧字段。否则破 M0 测试。
- `XQMesh`(M4):`boundaryFaces()`(MeshBoundaryFace{faceId,name,kind(Wall/Cap/Inlet/Outlet),capId,cellIds})。
  → M5 BoundaryConditionService 以此为**权威面列表**绑定边界条件。
- `XQPath`(M1):samplePoints(position/tangent/.../arcLength)、resample(spacing)、frameAtArcLength。**无截面积** → 截面积由模型/网格或 contour 提供(见下)。
- 命令复用 M0 AddNodeWithSourceRelation/AddNode;payload : public XQPayload + domainType + clone。
- `XQFlowResult`:**需新建**。
- `XQDomainType::SimulationCase` 已存在;**无 FlowResult 枚举,需新增**(末尾追加 `FlowResult`)。
  同步点(已核查):`XQDomainType.h` 的 domainTypeToString switch、`XQScene.cpp` 的 domain switch;
  检查 `tests/core/test_payload_and_groups.cpp`、`src/io/project/SvProjectReader.cpp` 是否引用枚举(按需补)。
  两处 switch 无 default → 漏分支编译会报,据此确保全覆盖。
- **无 XQSimulationCasePayload**(test_simulation_case 只测值对象,未入 scene)→ M5 需新建。
- 命令齐全:AddNodeCommand / AddNodeWithSourceRelationCommand / ReplacePayloadCommand / RemoveNodeCommand 都在 src/core/command/XQSceneCommands.h。
- **SimVascular 参考(0090_0001.sjb,权威)**:
  - 流体:Fluid Density=1.06 g/cm³,Fluid Viscosity=0.04 poise(血液,CGS)。
  - 出口 BC = **RCR**:每 cap `Values="Rp C Rd"`(近端阻力、顺应、远端阻力),如 outflow=`106.0 0.00068483 1784.0`。
  - 入口 BC = **Prescribed Velocity + Flow waveform**(parabolic,Period=0.984s),波形来自 inflow.flow。
  - 壁面 = rigid(刚性管)。
  - inflow_1d.flow 格式:两列文本 `t Q`(时间 流量),周期波形。

## 单位约定

- 全程 **CGS**(cm, g, s):与 .sjb 一致(density 1.06 g/cm³, viscosity 0.04 poise)。流量 Q [cm³/s],压力 [dyn/cm²=Ba],面积 [cm²]。在 spec 注明。

## 核心设计抉择

### 1. XQSimulationCase 扩展(core,末尾追加,不破 M0 测试)

- `BoundaryConditionType` 末尾追加:`RCR`、`InletFlowWaveform`(现有 4 个值不动)。
- `BoundaryCondition` 加可选字段(不删 faceId/type/value):
  - `std::vector<double> rcr;`(RCR:[Rp, C, Rd];仅 type==RCR 用)。
  - `std::vector<std::pair<double,double>> flowWaveform;`(InletFlowWaveform:(t,Q) 采样点;仅 inlet 用)。
  - `double waveformPeriod = 0.0;`
- 新增 `RomSettings`(plan 数据契约,随 case 存):centerline 源节点、inlet/outlet faceId 映射、时间设置(period/numTimeSteps/dt/numCycles)、输出控制。
- 新增 `FluidProperties{ density=1.06, viscosity=0.04 }`(CGS 默认同 .sjb)。
- XQSimulationCase 增持 RomSettings + FluidProperties + getter/setter。
- **XQSimulationCasePayload**:core 已有节点机制;若无 payload 则新建(: public XQPayload, domainType==SimulationCase, clone 深拷)。核查 test_simulation_case 当前如何入 scene。

### 2. XQFlowResult + XQFlowResultPayload(core,新建)

- `XQFlowResult`:降阶/1D 解。字段:
  - `segments`:沿中心线分段(每段:arcLength 起讫、关联 faceId/segment id)。
  - 时间序列:`times`(一个周期内采样时刻);每段/每节点 `Q(t)`、`P(t)`、`A(t)`(vector<vector<double>>)。
  - `sourceCaseNode`(来源 case 节点)。诊断摘要(是否收敛/稳定、最大 CFL)。
- `XQFlowResultPayload`:: public XQPayload,domainType==FlowResult(若新增枚举)或复用既有,clone 深拷。零依赖。

### 3. BoundaryConditionService(services/flow)

- `bindBoundaryConditions(case, mesh, spec)`:以 `mesh.boundaryFaces()` 为权威面列表,产/校验 case 的 BoundaryCondition。
  - 校验:每个 BC 的 faceId 必须存在于 mesh 边界面;inlet/outlet 角色齐全(至少 1 inlet + 1 outlet);wall 为 NoSlip。
  - RCR 参数为正(Rp>0,C>0,Rd>0);inlet 波形非空、period>0。
  - 失败返回带状态诊断(MissingFace / MissingInlet / MissingOutlet / InvalidRcr / EmptyWaveform),不抛异常。
- 编辑 case 入 scene 走命令(复用 ReplacePayloadCommand 或 AddNode;核查 M0 命令)。
- 解析 .flow 文本:`parseFlowFile(text)` → vector<(t,Q)>(纯字符串解析,io 或 service util;不引外部库)。

### 4. FlowSolver1D(services/flow,纯数值,零外部依赖)

> plan:一维血流(质量+动量守恒,面积-压力本构),出口 0D RCR;首版显式 + CFL;周期解。
> **务实分层**:保证每层都能用解析解证伪(防假绿),逐层加深。首版至少达成可解析校验的两个验收对照。

- **状态/方程**(经典 1D 血流,CGS):
  - 状态 (A, Q) 沿中心线 x。
  - 质量:∂A/∂t + ∂Q/∂x = 0。
  - 动量:∂Q/∂t + ∂(α Q²/A)/∂x + (A/ρ) ∂P/∂x = -K_R Q/A(K_R 摩擦,泊肃叶 8πμ/ρ;α 动量修正,首版 α=1)。
  - 本构(刚性壁简化/弹性壁):弹性 P(A)=P_ext + β(√A−√A₀)/A₀,β 由壁弹性;**首版壁=rigid**(.sjb 一致)→ 可退化为不可压管中的 1D 线性波/稳态。
- **离散**:沿中心线等分 N 段;显式时间推进(Lax–Friedrichs 或两步 Lax–Wendroff)+ **CFL 限制 dt ≤ CFL·dx/(|u|+c)**;超限发诊断不默默发散。
- **边界**:inlet 施加 Q(t) 波形;outlet 0D RCR 常微分耦合(Q-P:P 由 RCR 阻抗-顺应,半隐式更新)。
- **输出**:运行 numCycles 周期,取末周期作 XQFlowResult 的 Q/P/A(t) 序列。

- **数值校验门槛(关键,防假绿,plan 硬要求)** —— 测试用合成算例对解析解:
  1. **稳态泊肃叶刚性管**:恒定流入 Q₀、直管、刚性壁 → 沿程压降 ΔP = 8πμL Q₀ /(A²/π)... 用泊肃叶 ΔP=128μLQ₀/(πD⁴) 形式,数值稳态压降与解析吻合(相对误差 < 阈值,如 1–2%)。
  2. **单段 0D RCR 阶跃响应**:恒定 Q₀ 流入一个 RCR 出口(单集总腔)→ 出口压力 P(t) = Q₀(Rp+Rd) + (P₀−Q₀(Rp+Rd))·e^{−t/(Rd·C)} 的解析指数趋近;数值解与解析解吻合。
  3. (可选加深)线性波在刚性/弹性管中以理论波速传播。
- 这两个对照是**真数值断言**(CHECK + 相对误差阈值),不是"能跑就过"。worker 必须证伪(篡改 RCR 时间常数/泊肃叶系数 → 误差超阈 FAIL)。

### 5. 命令 / 入 scene

- BoundaryConditionService 编辑 case → ReplacePayloadCommand(若 case 已在 scene)或 AddNode;核查 M0 命令签名。
- FlowSolver1D 产 XQFlowResult → buildFlowResultCommand → AddNodeWithSourceRelationCommand(source = case 节点)。

## 截面积来源

- 1D 需要沿程 A₀(x)。来源优先级:① contour group 各截面面积(M2/M3 的 XQContour 多边形面积,最直接);② 表面模型沿中心线切片;③ 网格。
  **首版决策**:从 XQContourGroup 的 contour 多边形按弧长给 A₀(x)(鞋带公式算多边形面积),映射到中心线分段。
  合成校验算例用解析的常截面/线性变截面,不依赖真实数据。

## 校验(prd / plan)

- BC 必须绑定存在的网格 face id;inlet/outlet 齐全。
- RCR 参数为正;dt 满足 CFL(发诊断而非默默发散)。
- 截面积来源有效;中心线分段非空。
- 非法返回带状态失败,不抛异常。

## 测试(全 CHECK 宏,Release 真绿;不破 M0~M4 的 35 测试)

- `BoundaryConditionServiceTest`:以 mesh 边界面绑定 BC;缺 face/缺 inlet/缺 outlet/RCR 非正/空波形 → 对应诊断;.flow 解析正确;命令入 scene + undo。
- `FlowSolver1DTest`:**核心是两个解析对照**(稳态泊肃叶刚性管压降、单段 RCR 阶跃指数解),相对误差 < 阈值;CFL 超限 → 诊断;XQFlowResult 结构(times/Q/P/A 维度一致)、payload 深拷、命令 + undo。
- `FlowIntegrationTest`(端到端,读真实 0007):读 inflow_1d.flow + contour 截面 + RCR(用 .sjb 的 outflow 值)→ 跑出周期 Q/P 波形,结果结构正确、压力量级合理(诊断收敛)。**不与 SimVascular 3D 结果逐点对照**(不同模型),只验"能跑出合理周期解 + 入 scene"。

## 验收对照(plan「M5 验收」)

- 合成算例(刚性管泊肃叶、单段 RCR)数值结果与解析解吻合(单测,误差阈值)。
- 0007 中心线 + 截面 + inflow_1d.flow → 沿程 Q/P 波形入 scene,可被 M6 消费。
- BC 绑定网格 face、undo、(存档 round-trip 若 M5 触及序列化则测,否则记技术债到 M7 存档统一处理)。

## 风险

- 1D 双曲 PDE 显式格式稳定性是难点 → CFL 限制 + 诊断;首版若全 1D PDE 风险高,**至少先达成两个解析对照**(稳态泊肃叶可用稳态/隐式简化,RCR 阶跃是 0D 常微分),再加深瞬态 1D 波。分层保证验收可证伪。
- 枚举/字段扩展必须末尾追加,不破 test_simulation_case。
- 单位必须全程 CGS 统一(与 .sjb),否则量级错。
- 存档 round-trip(plan 提)若本里程碑不做序列化,记技术债,M7 统一。
</content>
