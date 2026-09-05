# Design：1D 血流几何冒烟

## Components

```text
VesselProfileV1
  -> VesselProfileValidator
  -> FlowInputAssembler(profile, FlowSmokeProtocolV1)
       -> SolverInput (CGS)
       -> UniformStationMap(solver index -> source sample interval/weights)
  -> FlowSolver1D::solve
  -> FlowSmokeBundle { SimulationCase, FlowResult, provenance }
  -> atomic scene command
```

## FlowInputAssembler

公开结果包含：

- `status/issues`；
- 完整 `FlowSolver1D::SolverInput`；
- `stationMap`；
- conversion record：source units、target units、protocol id/version。

装配顺序：

1. 调共享 Profile validator。
2. 要求 V1、LPS patient、mm/mm²。
3. 将弧长相对第一 station 归零，并在 canonical mm 空间确定性线性重采样为均匀 stations；记录每个 solver station 对源 sample interval/weights 的映射。
4. 弧长乘 0.1 得到 cm，插值面积乘 0.01 得到 cm²。
5. 复制固定 protocol 的 fluid、waveform、period、RCR、step/cycle 参数。
6. 运行有限性、严格递增、等间距和 solver precondition 检查。

归零只改变 solver 局部坐标，不改变 Profile patient-space identity。

## Smoke protocol

`FlowSmokeProtocolV1` 是强类型常量提供者，不从 Profile 或患者 metadata 猜测：

- protocol id/version；
- fixed fluid properties；
- one periodic inlet waveform；
- one positive RCR triple；
- `stationCount=11`；
- admissible geometry：length 50–500 mm、resampled area 50–2000 mm²；
- period、`dt=1e-4 s`、`numTimeSteps=200`、`numCycles=2`、record cap。

waveform、RCR、fluid 等具体数值从现有 `FlowSolver1DTest` 的稳定 transient case 复用。V1 始终以 11 个等间距 stations（含首末端）线性插值；固定 dt/N/envelope 的最小/最大边界必须真实调用 solver 并锁定无 CFL violation。超出 envelope 或 solver 仍报 CFL 时直接失败，不 retry、不改参数。变更任一常量必须提升 protocol 版本。

## Case and result model

- `XQSimulationCase::RomSettings` 追加 `vesselProfileNode` 指向 Profile；既有 `centerlineNode` 保持 legacy Path/centerline 语义。
- 持久 DTO（smoke protocol stamp、conversion record、uniform station/source-weight mapping、source revisions）定义在 core 的 SimulationCase/FlowResult typed payload 中，不由 `services/flow` header 拥有；这样 no-flow build 仍可读写历史数据。
- case 保存 typed smoke provenance（protocol id/version、assembler version、source profile id/revision、conversion record），不使用通用字符串 property bag。
- result 沿用现有 CGS `XQFlowResult`，并显式记录 sourceCaseNode、sourceVesselProfileNode/revision 与 solver stamp。
- station mapping 若不适合塞入 result，作为 case 的 typed ROM mapping 保存；不得靠数组顺序猜回 Profile。

## Atomic commit

主线程 prepare 先捕获 Profile payload/revision 且拒绝 stale；worker thread 完成 assembler + solve，返回值对象；主线程 commit 前再次核对 Profile revision/stale。随后复用 project-level batch command 原子插入 Case/Result、optional assets/bindings、Profile→Case、Profile→Result 与 Case→Result relations/asset lineage；任一步失败逆序回滚，undo/redo 保持完整。

## Stale behavior

既有图形成：

```text
Path/Contour -> Profile -> SmokeCase
                  \------> FlowResult <------/
```

修改 Path 或 Contour 一次调用 `mark_source_changed` 即使所有下游 stale；source 本身不 stale。禁止 controller 手动逐层标记。

## UI boundary

GUI 只选择 Profile node、显示固定协议说明、触发后台任务和提交 prepared command。任何 contour 面积、unit conversion、RCR 默认值都不得留在 MainWindow。

## Scientific labeling

所有显示名和状态使用 `L0 geometry smoke`，provenance 记录 `engineering-smoke-v1`。结果只用于架构连通性和数值 sanity，不进入患者预测或研究结论。
