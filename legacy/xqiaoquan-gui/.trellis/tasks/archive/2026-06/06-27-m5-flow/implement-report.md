# M5 流体里程碑 — 实现汇报(implement-report)

状态:**全部完成门槛达成**。全新独立 build 目录构建零错误,全量 ctest 38/38 真绿(M0~M4 的 35 + M5 新增 3),两条解析对照均通过双向证伪(假绿抽查),core/services/flow 零外部依赖。

## 1. 新增 / 改动文件清单

### core 改动(末尾追加,未破 M0~M4)
- `src/core/XQDomainType.h`:enum 末尾追加 `FlowResult`;domainTypeToString switch 加 `case FlowResult -> "flow_result"`。
- `src/core/XQScene.cpp`:groupForDomain switch 加 `FlowResult -> Group::Simulations`(流体结果归仿真组)。
- `src/core/XQSimulationCase.h/.cpp`:
  - `BoundaryConditionType` 末尾追加 `RCR`、`InletFlowWaveform`(前 4 值/顺序不动)。
  - `BoundaryCondition` 加可选字段 `std::vector<double> rcr`(=[Rp,C,Rd])、`std::vector<std::pair<double,double>> flowWaveform`、`double waveformPeriod`(聚合初始化下旧调用点不受影响)。
  - 新增 `struct FluidProperties{density=1.06, viscosity=0.04}`(CGS,同 .sjb)。
  - 新增 `struct RomSettings{centerlineNode, inletFaceIds, outletFaceIds, period, numTimeSteps, dt, numCycles}`。
  - XQSimulationCase 增持 FluidProperties + RomSettings + getter/setter。

### core 新建
- `src/core/XQSimulationCasePayload.h`:: public XQPayload,domainType==SimulationCase,clone()=值拷(纯值对象)。
- `src/core/XQFlowResult.h/.cpp`:1D/降阶解结果(times、segments、[segment][time] 的 Q/P/A 序列、sourceCaseNode、converged、maxCfl、isConsistent 维度自检)。
- `src/core/XQFlowResultPayload.h`:: public XQPayload,domainType==FlowResult,clone()=值拷。

### services 新建(services/flow,纯域,只 link xq_core)
- `src/services/flow/BoundaryConditionService.h/.cpp`:parseFlowFile(两列 t Q,跳空行/非法行)、validateAndBind(以 mesh.boundaryFaces() 为权威面校验)、bindCommand(ReplacePayloadCommand)。诊断:Ok/MissingFace/MissingInlet/MissingOutlet/InvalidRcr/EmptyWaveform/NullScene。
- `src/services/flow/FlowSolver1D.h/.cpp`:1D 血流(A,Q)守恒 + 泊肃叶摩擦 + 0D RCR 出口。三条路径:solveSteadyPoiseuille(稳态压降,直接积分)、solveRcr0D(0D 三元 windkessel,后向欧拉)、solve(瞬态 Lax-Friedrichs + CFL 检查 + 末周期记录)。诊断:Ok/EmptyCenterline/InvalidArea/CflViolation/NoInlet/NoOutlet/NullScene。buildFlowResultCommand(AddNodeWithSourceRelationCommand,source=case 节点)。

### 测试新建(全 CHECK 宏,Release 真绿)
- `tests/services/flow/BoundaryConditionServiceTest.cpp`:parseFlowFile 正确性、各诊断分支(MissingFace/MissingInlet/MissingOutlet/InvalidRcr×2/EmptyWaveform/NullScene)、命令 execute/undo。
- `tests/services/flow/FlowSolver1DTest.cpp`:**两个解析对照 + 相对误差阈值**、CFL 超限诊断、维度一致、EmptyCenterline/InvalidArea 诊断、payload 深拷、命令 undo。
- `tests/services/flow/FlowIntegrationTest.cpp`:读真实 0007 inflow_1d.flow + aorta_final.ctgr contour 截面(鞋带)+ .sjb outflow RCR → 跑 2 周期,验结构/收敛/压力量级。

### 构建
- `XQ/CMakeLists.txt`:xq_core 加 XQFlowResult.cpp;xq_services 加两个 flow 服务;新增 3 个 test 目标 + add_test;test_flow_integration 加入 tinyxml2 DLL 的 PATH 行,definitions 设 XQ_CTGR_DIR/XQ_FLOW_DIR。
- `XQ/build_m5.bat`、`XQ/ctest_m5.bat`(独立 build_m5 目录,.gitignore 已忽略 build_*/ build_*.bat)。

## 2. ctest 结果数字

- 构建:全新 `build_m5` 目录,132/132 链接成功,**零错误**。
- 全量 ctest(Release + QT_QPA_PLATFORM=offscreen):**100% tests passed, 0 failed out of 38**(M0~M4 的 35 + M5 新增 test_boundary_condition_service / test_flow_solver_1d / test_flow_integration)。

## 3. 两个解析对照的实际相对误差数值(防假绿核心)

测试通过时(test_flow_solver_1d stdout):
- **稳态泊肃叶(常截面)**:analytic dP=201.061930,numeric dP=201.061930,**relErr = 0.000e+00**(常截面下梯形积分精确),阈值 1e-9。
- **稳态泊肃叶(线性变截面)**:analytic dP=279.252680,numeric dP=279.254717,**relErr = 7.292e-06**,阈值 1e-2。
- **0D RCR 阶跃**:pInf=Q0(Rp+Rd)=9450.0,numeric_final=9449.8401,**ssErr = 1.692e-05**(阈值 1e-3),逐点 **maxRelErr = 9.977e-04**(阈值 1e-2)。

## 4. 假绿抽查证据(双向证伪,均 Release FAIL → 恢复 PASS)

| 篡改点 | 改动 | 结果 |
|---|---|---|
| **RCR 时间常数 Rd·C** | FlowSolver1D.cpp `solveRcr0D` 后向欧拉分母 `1 + dt/(Rd*C)` → `1 + dt/(2*Rd*C)`(τ 翻倍) | ssErr 1.69e-05 → **9.359e-01**,maxRelErr **9.360e-01**,test_flow_solver_1d **FAIL(EXIT=1)** |
| **泊肃叶系数** | FlowSolver1D.cpp `solveSteadyPoiseuille` `8.0*kPi*viscosity` → `16.0*kPi*viscosity`(系数翻倍) | 常截面 numeric dP 201.06 → 402.12,relErr 0 → **1.000e+00**,test_flow_solver_1d **FAIL(line 76,EXIT=1)** |

两次篡改均从备份精准恢复;恢复后 `diff` 与备份 IDENTICAL,全量 ctest 重跑 38/38 绿。

注:首次跑 RCR 稳态断言曾因测试只跑到 ~3τ(`e^-3≈5%` 未到稳态)误 FAIL,已修正为跑 ~11τ(5500 步)。这是测试断言修正,非求解器问题——逐点 maxRelErr 当时已通过。

## 5. 零外部依赖核查

grep `src/services/flow/` + M5 core 文件:include 仅 `core/*` 与标准库(cmath/vector/memory/sstream/string/utility/cstddef)。无 VTK/ITK/GDCM/Qt/svSolver/svZero/svOne/Python/pybind/tinyxml 的真实 include(命中项仅为"零依赖"说明注释)。xq_services 仍只 link xq_core。

## 6. 关键数值决策

- **单位**:全程 CGS(cm,g,s);density 1.06、viscosity 0.04(同 0090_0001.sjb)。
- **泊肃叶阻力**:圆截面 `dP/dx = -(8πμ/A²)·Q`,常截面退化为 `ΔP = 8πμLQ/A² = 128μLQ/(πD⁴)`;稳态路径用梯形积分自出口向上游直接求(无需迭代,可精确对照)。
- **0D RCR**:三元 windkessel,电容压 `C·dPc/dt = Q − Pc/Rd`,出口压 `P = Q·Rp + Pc`,稳态 `Q(Rp+Rd)`,τ=Rd·C;后向欧拉(无条件稳定)逐点对照解析指数解。
- **瞬态 1D(solve)**:Lax–Friedrichs 显式格式 + 上界 CFL 检查 `dt ≤ 0.9·dx/(|u|+c)`(用初始态+峰值入流估 maxSpeed),超限返回 CflViolation(不静默发散)。出口 RCR 半隐式(后向欧拉电容压)。inlet 波形周期线性插值。
- **本构(瞬态稳定性)**:rigid 壁波速 c→∞ 无法显式推进,故 solve() 用有限弹性壁 `P(A)=β(√A−√A0)/A0`(β=1e6 dyn/cm²,c~O(100 cm/s)生理量级)使显式格式稳定;验收两条解析门槛走专用 solveSteadyPoiseuille/solveRcr0D 路径(稳态/0D),与瞬态本构解耦,保证可证伪。
- **截面积 A0(x)**:integration 从 aorta_final.ctgr 各 contour 多边形面积(投影到 contour frame 后鞋带公式)按 pathArcLength 排序取得;合成单测用解析常/线性变截面。
- **维度布局**:XQFlowResult 序列 `[segment][time]`,segment = 中心线相邻站间区段(N 站 → N−1 段);isConsistent() 自检每段行=times.size、序列数=segments.size。

## 7. 留下的技术债

1. **瞬态 1D 波传播未做解析波速对照**:solve() 的弹性壁 β 为稳定性而设(注释已说明),其线性波速尚未加第三条解析校验(design 列为"可选加深")。两条硬验收门槛已达成。
2. **存档 round-trip**:M5 未触及序列化(XQSimulationCase 新字段 / XQFlowResult 未进 XQProjectWriter/Reader);按 design 记技术债,留 M7 存档统一处理。
3. **rigid 壁的真瞬态**:首版瞬态用有限弹性化近似 rigid;真正 rigid(不可压)需隐式/特征线格式,留作加深。
4. **截面积来源单一**:A0(x) 仅从 contour 多边形取;表面模型切片 / 网格来源(design 备选②③)未实现。
5. **TetGen/真实网格 face 关联**:FlowSegment.faceId 当前 integration 路径置 0(中心线分段与边界 cap 的映射未接入);RomSettings.inlet/outletFaceIds 已具数据契约但 solve 暂未消费,留 M6 消费侧接入。
