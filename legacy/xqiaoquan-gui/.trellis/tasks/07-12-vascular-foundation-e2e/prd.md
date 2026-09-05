# 血管地基：真实数据端到端

> Parent: `07-11-vascular-imaging-foundation`
>
> Depends on: dependency remediation, data gate, preprocess, automatic segmentation, bridge, centerline tree and real meshing.

## Goal

把全部已复用成熟能力接成一条 headless/GUI 共用的生产服务，并在产品中用它取代旧 threshold/seed/artificial Path 主流程：`CTA -> vesselness -> mask -> tree -> surface -> mesh -> Scene -> save/reopen`。

## Requirements

- 单一 `VascularWorkflowService` 编排所有 adapters/services；GUI 只收集 image/profile 意图、显示进度/诊断和提交结果。
- headless 与 GUI 调用同一 production entry，不存在测试专用算法分支。
- 所有阶段结果离 Scene 准备、验证；完整 bundle 成功后用一个原子 command 提交 mask/tree/surface/mesh 和 lineage。
- 主 Segmentation 页改为自动血管 workflow：选择 profile、运行/取消、进度、诊断和结果预览；移除 threshold/region-grow/seed picking 的默认产品入口。
- 旧人工 Path/Profile 能继续读取旧项目，但最终新建 workflow 不读取它们作为算法输入。
- 持久化形状在编码前版本化规划；保存、释放 resident data、重开后所有结果/坐标/provenance/lineage 一致。
- 失败/取消不留下 mask-only 或 mesh-only 半项目；重试不产生重复/孤儿节点。
- 日志不输出 PHI/free-text DICOM tags，只输出公开 case ID/fingerprint/profile/backend/stage/status。

## Acceptance Criteria

- [ ] AC1：一条真实 CTA 通过同一 production entry 自动生成 mask/tree/surface/mesh，gold 不在调用参数或读取路径中。
- [ ] AC2：GUI 正常用户流程不再显示或调用 threshold、人工 seed region-grow 和人工 Path 作为成功主链。
- [ ] AC3：GUI 可观察到阶段进度、取消、失败诊断以及 mask/tree/surface/mesh 的真实结果。
- [ ] AC4：完整 bundle 一次原子提交；任一阶段失败/取消时 Scene/Project 与运行前一致。
- [ ] AC5：保存、释放资源、重开后对象数量、IDs、LPS geometry、provenance、lineage 和可视结果一致。
- [ ] AC6：headless 与 GUI 输出 fingerprints/metrics 一致，不存在 mock/noop/test-only route。
- [ ] AC7：坏 DICOM、错 frame、空 vesselness、分割失败、centerline failure、surface/mesh failure 均有稳定诊断和零半状态。
- [ ] AC8：现有 Flow OFF、Path module 和旧项目读取能力不因 cutover 被破坏，但它们不再定义血管生产链。

## Out of Scope

可信 1D/Flow 结果、Darcy、CTC、ONNX、临床声明和最终用户验收判定（由 full-delivery acceptance child 完成）。
