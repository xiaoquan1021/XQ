# 血管地基：自动中心线树

> Parent: `07-11-vascular-imaging-foundation`
>
> Depends on: `07-12-shell-a-centerline-b`, `07-12-vascular-foundation-auto-segmentation`, `07-12-vascular-foundation-itk-vtk-bridge`, data and dependency gates.

## Goal

在壳档 A 的最小 A13 fallback 之上，从自动 mask/surface 生成 XQ-owned 中心线、物理半径和树拓扑。本 child 拥有自动端点、完整树拓扑、真实数据质量门和 v2 生产 A/B 选择；`07-12-shell-a-centerline-b` 继续拥有并先行交付最小单 Path fallback。

## Backend Policy

- **A:** time-box 接入锁定 SimVascular/vtkvmtk 最小 C++ closure，应用并审计 VTK 9.3 兼容修复；禁止 SuperBuild/Python runtime。
- **B:** 只有 A 的 ABI/许可/稳定性门失败时，复用并扩展 `07-12-shell-a-centerline-b` 已验证的真实 3D thinning + ITK physical distance map；增加自动端点、完整树提取、剪枝和稳定 ID，不复制 thinning/distance kernel。
- A/B 消费相同输入，产出相同 `XQCenterlineTreeV1`，通过相同真实数据指标；fallback 不能降低完成门。

## Requirements

- 自动生成 endpoint/source-target candidates；最终成功路径不要求用户提供 Path 或点击种子。
- B 路线复用 Shell A fallback 的 adapter、物理空间与基础图契约；若契约需扩展，必须保持 v1 Path 行为兼容且只有一个 thinning/distance 实现。
- 每个 node/branch/sample 有稳定 ID、LPS-mm point、非降弧长、finite positive radius、父子关系、endpoint/bifurcation 类型、backend/version/profile/fingerprint。
- 半径来自最大内切球（A）或物理距离图（B），不得使用 GUI 粗细或常数半径冒充。
- 验证无悬空引用、重复边、非法环（除非契约显式支持）、断裂主干、NaN/Inf/非正半径。
- topology/branch pruning 参数版本化；不静默删掉真实小分支来换取单连通输出。
- 输出可派生现有 `VesselPathV1`/下游只读视图，但 tree 是自动几何权威，不由人工 Path 反向构造过门。

## Acceptance Criteria

- [ ] AC1：A 路线有时间盒 probe 和明确 pass/fail 报告；失败才激活复用 Shell A fallback 的 B 扩展，不能复制 kernel 或同时维护两套 v2 默认实现。
- [ ] AC2：真实数据输出全部点/半径 finite，半径 > 0，主树连通且 graph references 有效。
- [ ] AC3：中心线覆盖/连续性、断裂数、半径误差和适用的 branch/endpoint 指标达到数据门冻结阈值。
- [ ] AC4：最终 E2E 不读取人工 `XQPath`、Contour 或 gold centerline 作为输入。
- [ ] AC5：同一输入/profile 的 stable IDs、topology 和 provenance 可重复并可持久化。
- [ ] AC6：backend failure、空 skeleton、断裂、异常环和半径失败返回 typed diagnostics，零半状态。

## Out of Scope

1D 求解、材料/BC、人工中心线编辑器、vmtk SuperBuild 和“有一条线就算成功”的 smoke 标准。
