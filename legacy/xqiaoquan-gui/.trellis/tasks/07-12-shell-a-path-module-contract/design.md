# Design: 壳档 A Path 与模块契约

## 1. Boundary

本 child 建立一条只读数据流，不改变现有持久化模型：

```text
Scene VesselProfile node
  -> PathModuleController (同步捕获 node/profile stamp)
  -> VesselPathSnapshotService (Profile -> immutable VesselPathV1)
  -> VesselPathValidator
  -> ShellGeometrySmokeService
  -> PathModuleRegistry {Noop, PathValidate}
  -> GUI source/summary/dump
```

`VesselProfileV1` 继续是唯一可持久化物理面积权威；`VesselPathV1` 不进入 `XQDomainType`、`XQPayload`、reader/writer 或 Scene。

## 2. Core Path Contract

新增 `src/core/XQVesselPath.h/.cpp`：

- `VesselPathV1::ContractVersion == 1`；
- coordinate system 仅接受 LPS，length unit 仅接受 mm；
- radius definition 固定为 `EquivalentCircularArea`；
- 非空 `frameOfReferenceId`；
- 来源枚举仅为 `AutomaticCenterlineB`、`SemiAutomatic`、`GoldFile`；
- `DerivationStamp` 精确记录一个源 Profile stamp；
- station 保存稳定 sample id、LPS position、arc length mm、radius mm；
- validator 接受至少 2 个 station、finite position/arc/radius、`radius > 0`、arc 非降，并返回逐项 typed issues。

验证失败只返回 issues，不修改输入、不删除 station、不夹紧数值。

## 3. Profile-to-Path Snapshot

新增 `services/profile/VesselPathSnapshotService`。输入为源 Profile 的 `DerivationInputStamp` 与 `VesselProfileV1` 值：

1. 先调用 `VesselProfileValidator`；失败时保留原 validator issues，零 Path 输出。
2. 要求输入 stamp node 等于所选 Profile node；输出 derivation 只引用该 Profile stamp。
3. station 一对一继承 sample id、position、arc length；半径为 `sqrt(areaMm2 / pi)`。
4. 来源采用最小声称映射：
   - 全部 `SegmentationDerived` -> `AutomaticCenterlineB`；
   - 全部 `ImportedGold` -> `GoldFile`；
   - 全部 `MeasuredContour` 或任何混合 -> `SemiAutomatic`。
5. 最后调用 `VesselPathValidator`；失败时零 Path 输出。

重复输入必须得到逐字段相同的 snapshot 与 dump。

## 4. Stable Dump and Geometry-only Smoke

新增 `services/path/VesselPathDump` 与 `ShellGeometrySmokeService`：

- dump 使用 classic locale、稳定字段顺序和足够的 double 精度；
- 只输出固定枚举、版本、frame UID、Profile node/revision/asset fingerprint 和 station 数值；不输出病人姓名、DICOM free-text tag 或 Profile parameter free text；
- dump 明确记录 `equivalent_circular_radius_from_area`；
- smoke 只接受 `VesselPathV1`，先验证，再计算 station/segment 数、arc span、polyline length、min/max/mean radius 和 canonical dump；
- smoke 不 include/link Flow、Contour、Scene、Qt、ITK 或 VTK；坏输入返回 typed status 且没有 summary/dump。

## 5. Static Path Module Registry

新增 `services/modules/PathModuleRegistry`：

- `IPathModule::run(const VesselPathV1&)` 是唯一执行入口；
- descriptor 只有稳定 `id/name/version`；
- built-in registry 固定注册 `noop` 与 `path-validate`；
- duplicate/empty id、unknown id、invalid path 和 module-declared failure 均返回稳定 status/diagnostics；
- 枚举顺序稳定，不扫描 DLL、不热加载、不读取环境变量；
- Noop 与 PathValidate 都只消费 Path。Noop 证明生命周期，PathValidate 返回共享 validator 的 issues；不得复制一份验证规则。

## 6. Controller and GUI

新增同步 `PathModuleController`：

- 只从打开的 `XQProject` 读取一个非 stale `VesselProfile` node；
- 验证可选 Asset 的 kind/fingerprint，构造精确 Profile input stamp；
- 调用 snapshot、smoke、registry，向 UI 返回 typed status、source label、summary 和 dump；
- UI 不直接计算半径、来源或 validator 结果。

`XQWorkflowSession` 始终拥有该 controller，和 Flow capability 无关。六页壳的第 5 页由 `Modules` 取代 `Flow`；页面把现有 Profile 装配兼容桥并入 Path 输入准备，并只暴露 Path module 执行、来源、geometry-only 摘要与 dump。Flow ON/OFF 的页面结构一致，均不暴露 solver smoke。

## 7. Build and Compatibility

- core、services、controller 文件全部无条件进入 ON/OFF target；
- 不修改 `WorkflowCapabilities` 的 Flow-only 语义；
- 不修改 project schema、旧 `.xqproj`、`XQPath` 或 `VesselProfileV1` 持久化格式；
- native writer 为原本未绑定 Asset 的 payload 自动派生 Asset 时，必须对 canonical payload block + blob refs 写稳定内容指纹；重开后 Controller 继续严格验证，不为 writer 空指纹开旁路；
- 新 UI 字符串同步 TS/QM；页面总数与顺序保持六页，但第 5 页的可见名称、对象名和工具栏语义从 Flow/Simulation 改为 Modules。

## 8. Failure and Rollback

- 任何 validation/snapshot/module 失败只更新 UI 状态，不写 Scene、Registry、文件或 undo 栈。
- 新文件和 Modules 页接线可独立回退；底层 Flow geometry smoke/service/controller 保留作后续芯与独立回归，但不得恢复为壳 GUI 入口。
- 不删除旧 build tree，不清理混合工作区，不提交其它 task 文件。
