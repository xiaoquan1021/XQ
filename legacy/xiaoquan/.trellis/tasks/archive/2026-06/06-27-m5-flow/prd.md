# M5 流体:FlowService 内置 1D/降阶血流求解 + 边界条件

## Goal

应用内**直接求解血流**,不依赖外部 3D CFD solver。采用 **1D / 降阶(0D-RCR)血流模型**:
输入中心线、沿程截面积、边界条件,输出沿血管的流量 / 压力波形。结果作为 AI 分析(M6)输入。

> 旧 plan 只做到"导出 solver 输入文件"就停。本里程碑把求解搬进应用内,作为可单测的数值服务。

## 依赖

- 前置:**M4**(网格边界面表)、M1(中心线)、M3/M4(截面积来源)。
- 后续:M6 AI 分析(消费 `XQFlowResult` 算 FFR/WSS 等指标)。

## Requirements

### 所有权

```
src/core/XQSimulationCase.h / .cpp       仿真/边界/ROM 设置 payload(XQrebuild 已有,扩展)
src/core/XQFlowResult.h / .cpp           流体结果 payload(沿程/随时间的流量、压力、面积)
src/services/flow/BoundaryConditionService.h / .cpp
src/services/flow/FlowSolver1D.h / .cpp  1D/降阶血流求解器(纯数值)
tests/services/flow/BoundaryConditionServiceTest.cpp
tests/services/flow/FlowSolver1DTest.cpp
```

### 边界条件(BoundaryConditionService)

- 以网格边界面表(M4 的 face/cap id)为**权威面列表**,编辑 `XQSimulationCase` 中类型化
  `BoundaryCondition`(inlet 流量波形 / outlet RCR 阻抗等)。
- **不把** `.sjb` 原始 XML、solver deck 文本或 UI 私有状态当真值来源。

### 求解器(FlowSolver1D)

- 模型:一维血流方程(质量+动量守恒,面积-压力本构),出口用 0D RCR 边界。
- 离散:沿中心线分段;显式/隐式时间推进(首版可显式 + CFL 限制),输出周期解。
- 纯 C++ 数值,**无外部 solver、无 Python**。

### ROM / 多物理设置(随 case 保存)

- `RomSettings`:centerline 源、分支定义、inlet/outlet 映射、RCR 参数、时间设置、输出控制。
- `MultiphysicsSettings`:流体材料、壁面模型、耦合类型、边界耦合 id、solver-prep 选项。
- 这些是 `XQSimulationCase` 的 core payload 字段;领域服务校验/编辑,core 不依赖领域库;可经原生存档与 `.sjb` 扩展元素 round-trip。

### 校验

- 边界条件必须绑定到存在的网格 face id;inlet/outlet 角色齐全。
- RCR 参数为正;时间步满足稳定性条件(发诊断而非默默发散)。
- 截面积来源(模型/网格)有效;中心线分段非空。

## 约束

- 不依赖 svSolver / svFSI / Python / 外部 CFD;公开 API 不暴露求解内部对象;不直接改 scene;设置不得存为无类型脚本字典。
- 参考来源:血流建模/边界条件语义参考 **SimVascular**(svZeroDSolver / 1D solver 行为);**XQ1 不参考**。

## Acceptance Criteria(plan「M5 验收」+ 06「验收」)

- [ ] 合成算例(刚性管、单段 RCR)数值结果与解析/参考解吻合,单测通过。
- [ ] 由 `0007` 的中心线 + 截面 + `inflow_1d.flow` 跑出沿程 Q/P 波形,结果入 scene 并可被 M6 消费。
- [ ] 边界条件绑定网格 face、undo、存档 round-trip 测试通过。

## Notes

- 复杂 task(含数值方法),进 Phase 2 前补 design.md / implement.md,实现前做数值方案研究(research/)。
