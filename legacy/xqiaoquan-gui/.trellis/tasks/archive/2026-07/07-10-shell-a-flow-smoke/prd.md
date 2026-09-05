# 壳 A：1D 血流几何冒烟

## Goal

用一个真实、现有的科学消费者证明壳能装入“芯”：经过验证的 `VesselProfileV1` 必须通过纯 C++ `FlowInputAssembler` 显式转换为现有 `FlowSolver1D::SolverInput`，用固定非患者化协议运行，并把 SimulationCase/FlowResult 及完整 lineage 保存重开。

## Dependencies

- Parent: `07-10-google-earth-shell-a`。
- Hard dependencies:
  - `07-10-shell-a-domain-contracts`；
  - `07-10-shell-a-profile-assembly`。
- 不要求 DICOM child 完成；端到端 child 再把其上游接到真实 DICOM。

## Requirements

### R1. FlowInputAssembler 是唯一单位边界

- assembler 位于 pure C++ service，不依赖 MainWindow、Qt、VTK、ITK 或文件 IO。
- 输入只接受合法 `VesselProfileV1` 和显式 `FlowSmokeProtocolV1`。
- 集中执行 mm→cm、mm²→cm²；不得修改 Profile 或在 GUI 重复转换。
- workflow prepare/commit 在 Scene 层拒绝 stale 或 revision 改变的 Profile；pure assembler 拒绝 snapshot 中错误 contract version、非 LPS patient frame、未知单位、非 finite/非正面积、非递增弧长和少于三个 station。
- L0 smoke 只接受显式 `ScaleSlot::Organ` 的 Profile；absent/Micro/Cell 返回 `UnsupportedScaleForSmoke` 或等价诊断，不能猜测。Case/Result 从该 Profile 显式复制 Organ。
- 当前 solver 使用统一 dx；assembler 必须把非均匀 Profile 线性重采样到均匀 solver stations，并保存可追溯的插值/来源映射。
- `FlowSmokeProtocolV1` 冻结 `stationCount=11`，均匀覆盖 Profile 首末弧长并包含两端；重复运行不得根据 GUI、硬件或输入间距改变 N。

### R2. 固定非患者化 smoke protocol

- 使用版本化、硬编码于 service/test contract 的固定 waveform、RCR、fluid 和 time plan；数值来自现有已验证 FlowSolver1D 测试范围并满足 CFL。
- V1 smoke 只接受 organ-scale 大血管几何 envelope：总长 50–500 mm，全部 source 与 resampled stations 面积均为 50–2000 mm²；固定 `dt=1e-4 s`、`numTimeSteps=200`、`numCycles=2`、`stationCount=11`，waveform/RCR/fluid 复用现有稳定 transient test。边界值必须由真实 solver 测试锁定。
- 不做隐式 retry 或偷偷修改 dt/N；超出 envelope 或 solver 返回 CFL violation 时稳定失败。一般 Profile→Solver 装配能力与这个 L0 smoke envelope 分开，不把 envelope 冒充临床适用范围。
- 协议 id 可为 `engineering-smoke-v1`，所有 UI/结果标签必须明确显示 `L0 geometry smoke`，不得显示或记录为患者特异边界条件。
- protocol 参数、版本和 solver status 必须进入可持久化 provenance。

### R3. 调用真实 FlowSolver1D

- 冒烟路径必须调用现有 transient `FlowSolver1D::solve`，不能用 Noop、仅 metrics 或伪造结果替代。
- solver 失败/CFL violation 时无 scene 副作用并保留结构化诊断。
- 成功结果必须 `isConsistent()`，所有 Q/P/A/times finite，且 station/segment 数与输入映射一致。

### R4. Scene lineage 与持久化

- 生成一个标明 smoke protocol 的 `XQSimulationCase`，在 RomSettings 追加显式 `vesselProfileNode`；保留 legacy `centerlineNode` 语义，不用它冒充 Profile。再生成 `XQFlowResult`，显式记录 Profile 与 Case 来源。
- Scene/Asset 关系形成 `VesselProfile -> SimulationCase` 和 `VesselProfile + SimulationCase -> FlowResult`，并与上游 Path/Contour 的 stale 链连通。
- case 与 result 在一个原子 command/bundle 中提交和 undo；部分插入不允许。
- 保存重开后 protocol、source ids、units、result series、relations 和 stale 状态保持一致。

### R5. 切断旧 GUI 几何装配

- MainWindow 不再从 contour 直接填 `solverInput.arcLength/area0`。
- GUI 若触发 smoke，只收集 node 选择并调用 typed controller/service；科学逻辑与单位转换都留在 service。
- 现有正式 Flow workflow 可保留，但必须与 smoke 明确命名区分。

## Acceptance Criteria

- [ ] 合法 Organ Profile 经唯一 assembler 产生正确 cm/cm² 数组和稳定 sample mapping。
- [ ] 非均匀输入始终生成 11 个含端点的均匀 stations；envelope 边界通过，越界/CFL 失败且无自动改参。
- [ ] 错误 frame/unit/version/area/arc 输入全部失败且不调用 solver、不改 scene。
- [ ] 固定协议真实调用 `FlowSolver1D::solve` 并产生一致、finite、可持久化结果。
- [ ] Profile→Case→Result relations、undo/redo、保存重开及上游 transitive stale 全部通过。
- [ ] 代码和 UI 明确标记 smoke，不宣称患者化 BC 或 M5 可信血流。
- [ ] MainWindow 中旧 contour→solver geometry assembly 被删除或封闭，focused 与 Release full ctest 通过。

## Out of Scope

- 患者边界条件反演、参数校准和不确定性传播。
- 分叉血管网络、全循环、可信 1D benchmark 或 M5 验收。
- Darcy、CTC、肿瘤模型和治疗响应。
- 通用 operation registry 或插件 ABI。
