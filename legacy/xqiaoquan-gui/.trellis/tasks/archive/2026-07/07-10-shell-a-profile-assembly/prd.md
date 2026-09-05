# 壳 A：血管剖面装配

## Goal

复用现有 `XQPath` 的重采样/截面标架与 `XQContourGroup` 的真实截面证据，通过纯 C++ service 生成经过统一验证的 `VesselProfileV1`，并提供金 profile 导入旁路。此后所有求解消费者只读取 Profile，不再直接解释 contour。

## Dependencies

- Parent: `07-10-google-earth-shell-a`。
- Hard dependency: `07-10-shell-a-domain-contracts` 完成并冻结 `VesselProfileV1`、validator、ScaleSlot 与持久化。
- 逻辑契约只依赖 domain child；但共享 worktree 的执行顺序固定在 DICOM child 之后，避免共同修改 CMake/controller/app 接缝时冲突。可使用现有 VTI/合成 geometry 开发，最终 e2e 接到 DICOM image lineage。

## Requirements

### R1. 三层几何权威不可混淆

- `XQPath` 只提供中心导航、弧长、位置、切向和截面 frame。
- `XQContourGroup` 只提供沿 Path 的截面测量证据。
- `VesselProfileV1` 是唯一求解几何；不得向 Path 添加并列权威半径或让 Flow 继续直接读取 contour。

### R2. Contour-to-Profile assembler

- 输入为同一 scene lineage 中的 Path 与 ContourGroup；必须验证 group 的 `sourcePathNode` 与实际 Path node 一致。
- contour 必须 closed、至少三个有限点、frame 有效、pathArcLength 在 Path 范围内且按位置唯一。
- 面积在 contour 自身 frame 的二维投影上计算，单位 mm²；退化、自交、重复点导致不可信面积时明确拒绝。
- Profile 的 position/tangent 由 Path 在 contour 弧长处的 frame 给出，面积来自 contour；不得把 contour centroid 当中心线真源。
- 输出样本弧长严格递增；sample id 从稳定 contour id 确定并在重复装配中保持一致。

### R3. 结构化质量与 provenance

- 输出记录 assembler algorithm id/version、参数摘要、输入 node/version、frame/units 与每样本 source/quality。
- 少于三个有效截面、来源不匹配或任何必需测量失败时，整个装配失败；默认不静默丢弃坏 contour。
- 可选宽松策略必须显式命名并记录被排除 contour，不作为壳 A 默认路径。

### R4. Imported-gold Profile 路径

- 允许测试/科研导入器把显式 version/frame/units/source/samples 组装为 typed `VesselProfileV1`，用于黄金数值和 legacy cm 转换验证。
- 壳 A 不新增第二套用户文件格式；imported-gold 数据进入同一 validator、payload、command 和项目持久化链。
- legacy cm 必须由调用者显式声明，position/arc ×10、area ×100；禁止按文件名猜单位。
- imported-gold 仍必须引用一个现存 Path node，并携带版本化 external evidence id/fingerprint；Scene 至少建立 Path→Profile，若 gold 已登记为 Asset 则同时建立 evidence asset→Profile asset lineage。它不要求伪造 Contour node。

### R5. 原子提交与 stale

- controller/service 先完成计算和验证，再返回 command；不得直接改 scene。
- Profile node 同时链接 Path 与 ContourGroup 为 derived sources；任一上游变化都使 Profile 及其下游传递式 stale。
- command 失败或 undo 后不得留下半条 relation、asset 或 undo 污染。
- Profile intent 可显式指定 ScaleSlot；未指定时，只有 Path 与全部 evidence source 都显式具有同一 ScaleSlot 才传播该值。任一 source absent/不一致则 Profile 保持 absent，不能自动升级为 Organ；zoom 永不参与推断。

## Acceptance Criteria

- [ ] 三个及以上合法 contour 确定性地产生弧长递增、正面积、LPS/mm 的合法 Profile。
- [ ] 重复运行相同输入得到相同 sample id、geometry 和 provenance 摘要。
- [ ] 来源 path 不匹配、open/退化/自交 contour、重复弧长、越界弧长和少于三个有效 contours/Profile samples 全部失败，Scene/Asset/relations/undo 均零副作用。
- [ ] imported-gold mm/cm 正向与错误矩阵通过；missing Path 或 missing external evidence id/fingerprint 被拒绝，并与 contour 路径共用同一 validator/持久化契约。
- [ ] Profile 同时依赖 Path/Contour；修改任一上游会传递 stale，undo/redo 行为正确。
- [ ] MainWindow 不再拥有新增的面积/单位转换逻辑；focused 与 Release full ctest 通过。

## Out of Scope

- 自动中心线、vmtk、完整血管树或分叉拓扑。
- 从稀疏 contour 自动生成患者微血管或复杂统计插值。
- FlowSolver1D、BC、材料或 CGS 转换。
- 自动分割质量提升；已有手工/阈值/level-set 轮廓都只作为证据输入。
