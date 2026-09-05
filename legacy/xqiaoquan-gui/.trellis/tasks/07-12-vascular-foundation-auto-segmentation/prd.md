# 血管地基：自动三维分割

> Parent: `07-11-vascular-imaging-foundation`
>
> Depends on: `07-11-vascular-foundation-data-gate`, `07-12-vascular-foundation-itk-preprocess`.

## Goal

用 ITK 的 thresholding、morphology、connected components、relabel 和可选 level-set 细化组成无需人工 seed/Path/Contour/gold 的传统自动三维血管分割，取代当前生产主流程中的自写 threshold、6-connected region grow 和 largest-component kernel。

## Requirements

- 输入只能是 XQ image + vesselness + versioned site/modality profile；生产接口没有 gold、seed、Path 或 Contour 参数。
- 高置信 vessel core 与低阈值候选均由 profile/自动统计产生；使用 ITK filters 生成 mask 和组件，不自写体素 flood-fill/connected-component kernel。
- XQ 仅自写组件评分/保留策略：core 接触、体积、长度/细长度、边界接触和多分支保留；不能只留最大组件。
- 形态学闭合/开操作和可选 level-set 均复用 ITK，参数以 mm 或有明确物理含义的单位记录。
- 输出为 `XQSegmentationMask` + provenance/diagnostics；成功前全部离 Scene 准备和验证。
- 冻结验证集上使用同一 profile；禁止逐病例看 gold 后手调并只报最好结果。
- 当前 `SegmentationService` 初级实现不得被该自动 service 调用；最终 GUI cutover 由 E2E child 完成。

## Acceptance Criteria

- [ ] AC1：生产成功路径不要求 seed/Path/Contour/gold，且日志证明 ITK morphology/components 实际执行。
- [ ] AC2：旧 custom threshold/region-grow/largest-component service 不在自动 workflow 调用图中。
- [ ] AC3：空 vesselness、无高置信 core、全前景、碎裂、错误 frame 和取消均稳定失败且零半状态。
- [ ] AC4：冻结 validation baseline 上报告 Dice、HD95/ASSD、centerline-aware/连通与分支保留指标，失败 case 不删除。
- [ ] AC5：达到数据门预先冻结阈值；gold fingerprint 与生产输入 fingerprint 分离。
- [ ] AC6：输出 geometry/provenance/lineage 可由后续 bridge 与持久化消费，公共 API 无 ITK 类型。

## Out of Scope

中心线、网格、GUI 最终页面、深度学习、TotalSegmentator runtime 和逐病例人工修补。
