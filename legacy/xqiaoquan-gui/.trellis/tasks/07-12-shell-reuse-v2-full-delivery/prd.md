# 完整复用壳 v2 总交付

> Status: in_progress
>
> Delivery tier: post-Shell-A v2 enhancement. It depends on Shell A v1 and never blocks A1–A15 completion.
>
> Completion policy: the complete v2 shell is delivered once, after real-data and physical-machine acceptance. Child completion is internal progress only.

## Goal

按 `D:\XQ\可复用工作打包说明.md` 的边界，把成熟前人能力组成 XQ 的默认生产壳，而不是继续自写医学影像和几何标准算法：

```text
真实增强 CT/CTA
  -> GDCM/ITK IO
  -> ITK 3D 去噪 + 多尺度 vesselness
  -> ITK 自动传统三维分割 + 后处理
  -> ITKVtkGlue 官方桥
  -> vtkvmtk C++ 或批准的 3D thinning fallback
  -> 中心线 + 正半径 + 树拓扑
  -> VTK 表面 + TetGen/MMG 或批准后端
  -> 现有 XQ Project/Scene/GUI/保存重开
```

## Authoritative Dependencies

本 v2 总任务不重写历史，显式依赖：

- `07-12-shell-a-v1-alignment`：复用现有宿主、Path/Module、canonical entry 和薄尺度身份。
- `07-11-vascular-imaging-foundation`：完成真实 CTA 到自动血管几何/网格的成熟外部依赖生产链。
- `07-12-shell-reuse-v2-final-acceptance`：两条依赖完成后的整壳实机验收。

`07-12-shell-a-centerline-b` 先行拥有壳档 A 的最小 thinning + distance-map 单 Path fallback。`07-12-vascular-foundation-centerline-tree` 依赖并复用该实现，只新增自动端点、完整树拓扑、真实数据质量与 v2 生产 A/B 选择；禁止复制 thinning/distance kernel。

## Requirements

### R1 - 复用优先，不重复造标准算法

- GDCM/ITK IO、ITK diffusion/vesselness/segmentation/morphology/components、ITKVtkGlue、VTK surface、vtkvmtk 或 3D thinning、TetGen/MMG 必须按批准边界复用。
- 只有 XQ 特有的契约、编排、来源/provenance、质量门、GUI 意图和原子提交允许自写。
- 未经书面技术/许可失败结论，不得用新的自研 kernel 代替清单中的成熟能力。

### R2 - 旧初级实现必须退出默认产品主链

- `SegmentationService` 的体素阈值、人工 seed region-grow 和自写最大连通域不得继续作为主 Segmentation 页的默认成功路径。
- 人工 Path/Contour、gold mask、Mock、Noop 和合成输入不得通过 v2 最终门。
- 旧能力若保留，只能作为明确标记的诊断/兼容入口，不能与生产自动链并列，也不能被最终 E2E 调用。

### R3 - ITK 只替代影像 kernel，不替代 XQ 宿主

- 保留并复用 XQ 的 Project、Scene、Source、payload、command、lineage、模块和 GUI 架构。
- ITK/VTK/GDCM/vtkvmtk/TetGen/MMG 类型止于 adapter/private implementation；公共 API 只暴露 XQ 类型。
- 纯 C++ 产品运行时；禁 Python runtime、Slicer/MITK/CTK 整机、vmtk SuperBuild 和 Claude。

### R4 - 真实数据生产链

- 至少一套真实增强 CT/CTA 与 reference vessel mask 完成来源、许可、hash、物理空间和 gold 隔离数据门。
- 同一生产调用在不读取 gold 的情况下自动产出 mask、centerline tree、surface 和 mesh。
- LIDC 仅保留 DICOM IO gate，不得用作血管算法验收。

### R5 - 可观察产品行为

- GUI 主流程允许用户选择影像和参数 profile，运行自动血管处理，查看进度/诊断、mask、中心线树、表面/体网格并保存。
- 保存释放运行时资源后重开，几何、坐标、来源、算法版本和 lineage 一致。
- 报告先描述输入、用户动作、屏幕/项目中可见结果和剩余缺口；测试计数只作回归证据。

### R6 - 原子总交付

- 子任务完成不触发对用户交付；总任务在真实数据 E2E、失败路径、保存重开和用户实机验收前保持未完成。
- 自动测试全绿不是完成定义；用户最终实机判断功能是否合格。
- 未解决的生产许可、数据获取、中心线质量或真实网格问题会阻止总任务完成，不能以 fallback 输出数量替代质量。

## Acceptance Criteria

- [ ] AC1：依赖整改与真实 CTA/reference mask 数据门均完成，实际 bytes/hash/license/空间报告存在。
- [ ] AC2：ITK 3D diffusion + multi-scale vesselness 在生产 target 和真实数据上执行，参数以 mm 表示。
- [ ] AC3：无需人工 seed/Path/Contour/gold 输入的自动传统分割成为 GUI 和 headless 的唯一默认生产路径。
- [ ] AC4：ITKVtkGlue 官方桥实际执行并守恒 dimensions/spacing/origin/direction/LPS/scalar/lifetime。
- [ ] AC5：自动中心线结果含连续点列、finite positive radius、稳定树拓扑和 provenance；不使用人工 Path 过门。
- [ ] AC6：真实血管 surface/volume mesh 通过预先冻结的有效性与质量门；后端许可边界明确。
- [ ] AC7：同一服务链贯通 headless 与 GUI，成功时原子提交，失败时零半状态。
- [ ] AC8：主 Segmentation 页面和最终 E2E 不再调用旧 threshold/seed/custom-component 主链。
- [ ] AC9：真实项目保存、释放、重开后 mask/tree/surface/mesh/坐标/provenance/lineage 一致。
- [ ] AC10：用户在物理机器上按验收剧本完成操作并确认功能；此记录存在前不交付整壳。

## Out of Scope

- 可信 1D、Boileau/openBF、Darcy、CTC、ONNX、Level 4 数字孪生。
- Python 运行时、深度学习生产分割、Slicer/MITK/CTK 整机、Cesium。
- 以测试通过率、Mock、合成数据或人工几何替代真实功能验收。
