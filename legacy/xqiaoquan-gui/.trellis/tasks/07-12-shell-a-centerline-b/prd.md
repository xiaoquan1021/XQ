# 壳档 A：ITK 中心线 B

> Status: in_progress
>
> Parent: `07-12-shell-a-v1-alignment`
>
> Ownership: this task owns the minimal Shell A v1 A13 implementation. `07-12-vascular-foundation-centerline-tree` depends on and extends it for v2 tree topology and real-data production quality; v2 does not supersede or block this child.
>
> Continuation correction (2026-07-14): read `handoff.md` before any further
> implementation or acceptance claim. The user requires predecessor-backed
> automatic Path processing to replace replaceable manual Path workflow in the
> complete DICOM shell. This child owns only `SegmentationMask -> Path/Profile`;
> it must not be mistaken for, or silently expanded into, the complete
> `DICOM -> automatic processing -> Path` product route.

## Goal

在 vmtk 完全关闭/不存在时，从 XQ-owned 3D binary mask 经真实 ITK 3D thinning、物理距离图、图提取和剪枝，稳定产生满足壳级 Path validator 的单主路径，并通过现有 command/lineage 机制原子发布。

## Dependencies

- canonical build child 已提供稳定 ITK package/runtime 基线。
- Path/module contract child 已冻结壳级 Path invariants 与 source mapping。
- 使用依赖审计锁定的 ITKThickness3D v5.3.0/commit/hash，或在 child design 中明确批准另一真实 3D ITK thinning 来源。
- 后续 v2 tree child 必须复用本任务的 thinning/distance adapter 与已验证基础图结果，不得另造同功能 kernel；本任务不承担自动端点、完整树质量或生产 A/B 选择。

## Requirements

- 输入为 XQ-owned binary voxel/mask source，保存 dimensions/spacing/origin/direction/LPS/mm；外部 ITK image 只在 adapter 私有实现存在。
- 真实执行 3D thinning，禁止逐切片伪三维和测试专用替代算法。
- 距离图必须使用物理 spacing；skeleton voxel 半径 finite/positive。
- 26 邻域图提取 endpoint/junction；以 mm 阈值剪短毛刺；主路径选择确定性且记录 algorithm/version/parameters。
- index-to-physical 使用完整 direction，输出 LPS-mm position 和非降 arclength。
- 发布 XQPath + segmentation-derived VesselProfile/Path snapshot 时使用原子 command、lineage、stale、undo/redo；失败无半节点/半 asset。
- vmtk OFF 构建和运行证据必须独立存在。
- 合成直管/弯管/分叉/毛刺覆盖数学与病态；至少一个 disclosed 非 PHI mask fixture 运行真实 adapter。
- 结论只证明档 A 单路径可装配，不声称自动分割、完美树、Dice 或半径金标准准确性。

## Acceptance Criteria

- [ ] AC1：vmtk OFF 配置中真实 thinning adapter 编译并执行。
- [ ] AC2：各向异性 spacing、倾斜 direction 的 index/physical round-trip 与输出位置正确。
- [ ] AC3：合法 fixture 输出至少两点、正半径、非降弧长并通过 Path validator/geometry smoke。
- [ ] AC4：空 mask、无 skeleton、断裂/歧义图、非正半径和坏 geometry 返回稳定诊断且零提交。
- [ ] AC5：毛刺剪枝、主路径选择和重复运行具有确定性测试。
- [ ] AC6：Scene/Asset lineage、stale、undo/redo、保存重开与 source identity 通过集成测试。
- [ ] AC7：canonical ON/OFF full Release regressions 通过，public boundary 无 ITK type 泄漏。

## Out of Scope

- Frangi/Sato、自动分割、gold mask 指标、树拓扑持久化。
- vtkvmtk 生产化、vmtk 对比报告或 Voronoi 最大内切球路线。
- CFD/Flow 求解或真实 CTA-derived 网格质量。
