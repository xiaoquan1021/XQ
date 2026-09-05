# A1–A15 当前基线与任务归属

日期：2026-07-12

权威来源：`D:\XQ\规划-GoogleEarth全路线与壳子阶段.md` §4.2、§5.3、§5.4。

状态含义：

- `PASS-EVIDENCE`：已有强证据，最终 child 仍需复核。
- `PARTIAL`：部分能力存在，但未严格满足原条文或缺 canonical/人工证据。
- `GAP`：当前没有所需生产能力。
- `NON-BLOCKING`：建议项或明确非门闩，但仍需诚实记录。

| Gate | Baseline | Evidence / gap | Owning child |
| --- | --- | --- | --- |
| A1 | PARTIAL | 独立 dependency remediation 树已净化；`build_gui_wt.bat`、`build_shell_noflow_wt.bat`、`run_xq.bat` 仍使用旧 Qt/过宽 prefix，旧 cache 含 host Anaconda | canonical-entry |
| A2 | PASS-EVIDENCE | architecture/public-header guard、adapter/private external types 已存在 | acceptance-v1 regression |
| A3 | PASS-EVIDENCE | DICOM/XQ Volume 保留 spacing/origin/direction、LPS/mm 与 voxel source | acceptance-v1 regression |
| A4 | PASS-EVIDENCE | LIDC-IDRI-0957 真实 65 文件通过 reader/import/save/reopen；只证明 IO | acceptance-v1 evidence |
| A5 | GAP | `XQPath` 无半径；`VesselProfileV1` 有 arc length/area/version/LPS，但没有 Path-only module snapshot | path-module-contract |
| A6 | PARTIAL | Profile evidence 能区分 contour/segmentation/gold，但尚无壳级 PathSource 与统一 GUI/dump 口径 | path-module-contract |
| A7 | PARTIAL | 现有 Flow geometry smoke 消费 Profile 并运行真实 solver；不是纯 Path `smoke_geometry_only` | path-module-contract |
| A8 | GAP | `WorkflowCapabilities` 是 Flow build/runtime gate，不是 Noop + PathValidate registry | path-module-contract |
| A9 | PASS-EVIDENCE | ScaleSlot 三枚举、持久化与 Flow OFF 显示已有回归 | acceptance-v1 regression |
| A10 | PARTIAL | GUI/headless 主链自动化存在；尚缺新 Path module/thinning 入口和最终实机记录 | acceptance-v1 |
| A11 | NON-BLOCKING | VTK/Scene/GUI 有几何显示基础；是否展示本轮表面由限制页记录 | acceptance-v1 |
| A12 | PASS-EVIDENCE | TetGen adapter + tetra smoke + OFF gate 已有；版本应为 1.5，research-only | acceptance-v1 regression |
| A13 | GAP | vtkvmtk/ITKThickness3D 仅 source-only lock；无生产 3D thinning、距离半径、图提取/剪枝 | centerline-b |
| A14 | PARTIAL | scope correction、handoff、task memory 已存在；缺一份以 A1–A15 为主表的最终非声称/限制页 | acceptance-v1 |
| A15 | GAP | 独立整改命令有证据，但 canonical build/run、Path dump、module log 和实机 GUI 尚未形成同一固定仪式 | canonical-entry + acceptance-v1 |

## Existing Evidence That Must Not Be Overstated

- Flow ON 92/92、OFF 83/83：证明旧 Shell 回归，不证明 A5/A8/A13。
- LIDC real-data 2/2：证明真实 DICOM IO，不证明 CTA 或血管分割。
- dependency remediation 93/93、research ON/ON 95/95：证明独立净化树，不证明 canonical 入口已迁移。
- `VesselProfileV1`：是现有持久化/solver 物理几何权威；不能据此直接说 `XQPath` 已满足 A5。
- `WorkflowCapabilities`：证明 Flow 可关闭；不能据此说模块 registry 已完成。

## Frozen Task Decisions

1. 不修改 A1–A15 的语义，不以血管影像 v2 PRD替换。
2. 不新增第二份可写几何权威；壳级 Path module input 从现有 Profile/自动中心线结果生成不可变快照。
3. 不做动态插件；静态 registry 已足够。
4. A13 使用 vmtk OFF 的真实 3D thinning B；vtkvmtk 不挡档 A。
5. 当前 LIDC 可继续关闭 A4；完整 CTA data gate 不挡本任务。
6. 所有 build tree full CTest 串行执行。

