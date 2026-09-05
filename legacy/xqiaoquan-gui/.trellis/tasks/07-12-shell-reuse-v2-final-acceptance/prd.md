# 完整复用壳 v2：最终实机验收

> Parent: `07-12-shell-reuse-v2-full-delivery`
>
> Starts only after `07-12-shell-a-v1-alignment` and `07-11-vascular-imaging-foundation` are functionally complete.

## Goal

以用户可实际操作的完整功能而非测试计数验收整壳。此 child 是唯一允许关闭 `07-12-shell-reuse-v2-full-delivery` 的门。

## Acceptance Script

1. 从 canonical `run_xq.bat` 启动产品。
2. 导入数据门固定的真实增强 CT/CTA case，确认影像空间/切片可见。
3. 在 Segmentation 页选择冻结 profile 并运行自动流程，不提供 seed/Path/Contour/gold。
4. 观察 vesselness/分割阶段状态，查看 mask 与原 CT 叠加。
5. 查看自动中心线树、分叉、半径和表面/体网格；检查诊断和来源显示。
6. 保存项目、退出、释放运行时资源、重开，重新查看全部结果。
7. 使用独立 evaluator 读取 reference mask 并生成冻结指标报告；确认 production fingerprint 未包含 gold。
8. 人为触发至少一个失败/取消场景，确认项目无半状态。
9. 用户在物理机器上决定功能是否合格并记录结论。

## Acceptance Criteria

- [ ] AC1：上述真实用户动作从头到尾可完成，屏幕与项目中出现预期真实结果。
- [ ] AC2：正常主流程没有 threshold/人工 seed/人工 Path/gold 输入捷径。
- [ ] AC3：自动 mask/tree/surface/mesh 的冻结指标和 hard validity gates 全部满足；失败样本完整报告。
- [ ] AC4：保存重开后 geometry/provenance/lineage 与运行前结果一致。
- [ ] AC5：失败/取消零半状态，重新运行不产生重复孤儿对象。
- [ ] AC6：用户物理机器 GUI 验收明确通过；offscreen、CTest 或 AI 判断不能替代。
- [ ] AC7：最终报告先说明实际功能、操作和可见结果，再列构建/测试回归；不声称 Flow/Darcy/CTC/ONNX 完成。

## Completion Rule

任何一项未满足，本 child 和总任务保持未完成。不得以“其余后续补”“测试全绿”或“已有输出”交付。
