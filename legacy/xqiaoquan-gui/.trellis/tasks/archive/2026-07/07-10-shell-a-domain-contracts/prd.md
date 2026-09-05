# 壳 A：领域契约与持久化

## Goal

建立壳档 A 后续所有数据链共同依赖的、零外部库类型的领域契约，并一次性完成项目格式兼容设计：

- `ScaleSlot { Organ, Micro, Cell }` 是可持久化的生物/物理尺度身份，与渲染 LOD 完全分离。
- image scene node 持有可序列化的 XQ 自有影像元数据，而不是只依赖窗口私有 `activeImage_`。
- `VesselProfileV1` 成为 1D 血流、未来 CTC 和血管网络消费者唯一认可的求解几何契约。
- `contentRevision`、`DerivationStamp`、原子多父关系和 semantic edit/stale undo 成为可复现派生链的共同契约。

## Dependencies

- Parent: `07-10-google-earth-shell-a`。
- 无实现型前置子任务；这是 DICOM、profile assembly 和 flow smoke 的共同前置。
- 实现开始前必须读取父任务 `research/current-state.md`、`decision-log.md` 和 `reuse-map.md`。

## Requirements

### R1. ScaleSlot 是独立尺度身份

- 仅定义 `Organ`、`Micro`、`Cell` 三个稳定值。
- ScaleSlot 必须随 scene node 或其权威 asset/payload 保存并重开恢复。
- ScaleSlot 作为 optional node 字段持久化；旧项目缺失该字段时保持“未指定”并给出至多一次信息诊断，不得静默归为 Organ。
- 壳 A 新建的 patient-scale 影像/Path/Contour/Profile/Smoke 对象可由创建服务显式标为 Organ。
- 至少一个非渲染消费者能够读取并报告 ScaleSlot。

### R2. 影像元数据成为一等 payload

- 定义 metadata-only 的 XQ 自有 image payload，保存 `XQImageVolume` 的几何、scalar/component、modality、DICOM identity、window/rescale 等必要元数据。
- 像素数组不得复制进 payload；像素驻留继续由 Asset/Blob/`IVoxelSource` 边界负责。
- payload 不得暴露 ITK、GDCM、VTK 或 Qt 类型。

### R3. VesselProfileV1 契约冻结

- 保存 contract version、LPS patient frame、显式长度/面积单位、来源 Path/Contour/Segmentation 节点、算法与质量来源。
- 每个采样保存稳定 sample id、严格递增弧长、position、tangent 和正截面积；半径只能是面积的派生表示，不能成为第二真源。
- 契约不得包含边界条件、材料、压力、流量或显示伪彩字段。
- 提供结构化验证结果，覆盖版本、frame、单位、finite、少于 3 个 samples、重复/倒退弧长、非单位切向和非正面积。

### R4. 项目格式兼容

- 新 reader 必须继续读取当前合法 1.2 项目。
- 新 writer 对新增 payload/ScaleSlot 使用一个明确的新格式版本，建议 1.3；不得用隐式字段破坏旧语义。
- 旧项目迁移默认值必须有测试；未知未来版本仍须拒绝。
- 新 payload、节点/asset 绑定、ScaleSlot、来源 id 与 VesselProfile 样本保存重开后保持一致。
- 补齐现有 in-memory `XQContourGroupPayload` round-trip，并修正 ContourGroup 被错误映射为 Image AssetKind 的缺口。

### R5. Scene、Asset 与 stale 规则

- VesselProfile 获得明确 `XQDomainType`、scene group 和 `AssetKind`，不伪装成 Path、SimulationCase 或通用 property bag。
- 所有 scene 写入继续经 command stack；来源关系继续使用现有 derived graph。
- 编辑上游 Path/Contour 后，VesselProfile 及未来下游节点可由现有传递式 stale 机制失效。

### R6. Revision、派生戳与原子多父命令

- `XQDataNode` 持久化 `contentRevision`；只有 semantic edit 递增，reader/lazy materialize/residency 变化不递增。
- `DerivationStamp` 记录算法 id/version、参数摘要、可选 seed，以及输入 node id + content revision；可用时记录 asset id/fingerprint。
- 提供窄型 project-level batch command：一次 prepared commit 可包含 node、optional asset、node↔asset binding、多个 Scene source relations 与对应 Asset lineage；任一 asset/node/relation 失败时全回滚且不进入 undo。
- 为 AssetRegistry 增加最小可逆 unregister/remove-relation API；撤销不得回退或复用已发出的自动 asset id。
- 提供 semantic replace+invalidate command，保存并恢复编辑前 stale snapshot；不得改变现有普通 ReplacePayload 的非语义用途。

## Acceptance Criteria

- [ ] `ScaleSlot` 三值和 absent 状态稳定，旧项目保持未指定，新项目 round-trip 不丢失。
- [ ] image metadata payload clone、序列化和重开一致，且不持有外部库对象或像素副本。
- [ ] 合法 `VesselProfileV1` 通过验证并 round-trip；所有规定的坏输入返回明确失败且无 scene 副作用。
- [ ] Path/Contour/Profile 三层职责在公共接口和测试中没有双重几何真源。
- [ ] 1.2 合法 fixture 继续加载；新格式保存/加载和 versioned-save 回归通过。
- [ ] ContourGroup typed payload/AssetKind、contentRevision、DerivationStamp、多父关系与 stale snapshot 全量 round-trip。
- [ ] semantic edit 递增 revision 并传递 stale；undo/redo 恢复旧 revision/payload/stale；惰性 materialize 不触发这些变化。
- [ ] architecture boundary test、focused tests 与 Release 全量 ctest 通过。

## Out of Scope

- DICOM 文件/series 读取。
- 从 contour 或金数据生成 profile。
- FlowSolver1D 输入装配与求解。
- ROI registry、ScaleNode、语义缩放或动态插件框架。
