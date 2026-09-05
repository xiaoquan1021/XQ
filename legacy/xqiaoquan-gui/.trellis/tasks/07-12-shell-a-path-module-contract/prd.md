# 壳档 A：Path 与模块契约

> Status: in_progress
>
> Parent: `07-12-shell-a-v1-alignment`

## Goal

补齐父任务 A5–A8：提供版本化、LPS/mm、正半径、来源诚实的不可变 Path module input；用纯 geometry smoke 和只消费 Path 的静态 Noop/PathValidate 模块取代壳 GUI 中旧 Flow smoke 入口，同时保留 `VesselProfileV1` 为唯一持久化物理几何权威。

## Background

- 现有 `XQPath` 负责导航、重采样和 section frame，不携带半径。
- 现有 `VesselProfileV1` 已持久化 LPS/mm/mm2、来源证据和输入 stamp，并且是当前唯一 solver-facing 物理几何权威。
- Flow OFF 构建会移除 solver/controller 实现，但保留 Path、Profile、项目 IO 与非 Flow services；Path 模块必须位于该边界之外。
- 本 child 不新增 Scene domain、payload 或项目 schema；Path snapshot 在选择/执行 Profile 时重建。

## Requirements

- 新增 XQ-owned `VesselPathV1`（最终命名可在 design 冻结）及 station/source/validation DTO，core/public API 无第三方类型。
- Path 至少两个站点；position/radius/arclength finite；radius > 0；arclength 非降；frame、LPS/mm、version 和 source 有效。
- Profile-to-Path adapter 必须先验证 `VesselProfileV1`，以 `sqrt(areaMm2/pi)` 派生半径，并把 evidence 映射为 AutomaticCenterlineB/SemiAutomatic/GoldFile。
- Snapshot 不作为第二份 Scene 可写权威；优先从现有 Profile 在加载/执行时重建，避免 project schema 变化。
- Path dump 稳定、可复现、无 PHI/free-text DICOM tags，可用于 A15 证据。
- `ShellGeometrySmokeService` 只消费 Path，不运行 Flow solver、不访问 Contour/Scene/native ITK/VTK，输出确定性几何摘要。
- 静态 registry 至少真注册并运行 Noop 和 PathValidate；module interface 只接受 Path 快照。
- `WorkflowCapabilities` 继续只控制 Flow；Flow OFF 仍可运行模块、查看 Path 和保存重开。
- 六页壳的第 5 页必须由 Path/Modules 取代 Flow：不得继续暴露 solver smoke、协议、Flow 源或 Flow capability 状态；Flow ON/OFF 的壳页面形态相同。
- 现有 Profile 装配只作为 Path snapshot 的内部兼容准备步骤，并入同一 Path/Modules 工作流；不得再显示为独立 Flow 功能区。
- 出错返回 typed diagnostics，零半状态、零隐式修复、零 GUI 算法旁路。

## Technical Notes

- 类型名冻结为 `VesselPathV1`，由 `VesselPathSnapshotService` 从一个已验证的 `VesselProfileV1` 和其 `DerivationInputStamp` 生成。
- 半径定义冻结为等效圆半径 `sqrt(areaMm2 / pi)`；dump 必须明确写出该弱半径定义。
- 来源映射采用最小声称：纯 `SegmentationDerived` 为 `AutomaticCenterlineB`，纯 `ImportedGold` 为 `GoldFile`，纯 `MeasuredContour` 或任何混合证据为 `SemiAutomatic`。
- GUI 将原第 5 页整体替换为 Path/Modules 页面：Path 输入准备与模块选择在同一工作流中，Path module 是唯一壳执行入口，并显示来源、geometry-only 摘要和可复制稳定 dump。
- 静态 registry 是 Path 专用服务，不改造 `WorkflowCapabilities`，也不引入通用 operation/plugin framework。

## Acceptance Criteria

- [ ] AC1：validator 对合法 Path 通过，并逐项拒绝版本/frame/source/点数/NaN/Inf/非正半径/弧长错误。
- [ ] AC2：Profile-to-Path 的半径、来源、输入 stamp 和重复运行结果精确可验证。
- [ ] AC3：geometry-only smoke 对同一输入稳定，对坏输入零输出；无 Flow solver/link 依赖。
- [ ] AC4：Noop/PathValidate 通过 registry 注册、枚举和运行；重复 ID、未知 ID 和 module failure 有稳定诊断。
- [ ] AC5：public-header/architecture guard 证明模块 API 不泄漏 Qt/ITK/VTK/Contour/Flow 类型。
- [ ] AC6：Flow ON/OFF 下第 5 页均为 Modules、模块能力存在且旧 Flow smoke 控件不存在；focused tests 与 full Release CTest 均通过。
- [ ] AC7：Path 来源在 dump/GUI/日志中可见，半自动/金文件没有全自动宣称。

## Out of Scope

- 动态 DLL、热加载、插件市场、进程隔离。
- 改写 Flow solver、BC/material schema 或可信 1D。
- 实现 thinning kernel、自动分割或树拓扑；centerline child 只消费本 contract。
