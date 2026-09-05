# First Principles Analysis: 完整复用壳 v2

## Axioms

1. **A1: 产品能力由真实输入触发的可观察行为定义。** 测试、文档或对象数量不能替代用户实际执行同一条生产链；违反它必然产生假完成。
2. **A2: 已有成熟且适用的标准 kernel 应被复用。** `D:\XQ` 已明确 ITK/GDCM/VTK 等解决的标准问题，重新实现只增加缺陷面，除非适配或许可被证实不可行。
3. **A3: 外部实现必须被稳定边界隔离。** 第三方类型或平台身份一旦泄漏到 XQ 公共层，后端不可替换且宿主被绑架，直接破坏复用目标。
4. **A4: 评价真值不能成为生产输入。** gold mask 若参与 seed/ROI/阈值/Path，量化结果失去独立性，最终结论必然无效。
5. **A5: 用户定义的交付单位是完整壳。** 单个 child 或全绿测试不能改变这个验收单位；实机确认前交付就是违反需求。

## Problem Essence

**Core problem:** 当前 XQ 已有宿主和局部壳契约，但默认血管影像/几何链仍依赖初级自研或人工输入，未把 `D:\XQ` 指定的成熟前人能力组成一个真实可用的完整产品流程。

**Success criteria:** 一例未作为 gold 输入的真实增强 CT/CTA 能在纯 C++ XQ 内自动运行到 mask、中心线树、表面/体网格、GUI 与保存重开；默认路径不调用人工 seed/Path/Contour 或自写标准 kernel；用户实机确认功能。

## Assumptions Challenged

| Assumption | Why question it | Axiom(s) | Verdict |
| --- | --- | --- | --- |
| Path/Modules 页完成就等于壳完成 | 它没有从影像产生真实几何 | A1, A5 | Discard |
| ITK 应替换整个 XQ | ITK 不提供 Project/Scene/GUI/command 宿主 | A2, A3 | Modify: only imaging kernels |
| threshold/seed 可与自动链长期并列为主模式 | 用户会继续走旧成功路径，替换没有发生 | A1, A2 | Discard; diagnostic only |
| 自动分割可继续算“加分” | v2 的真实 CTA 自动竖井离不开它 | A1, A5 | Discard for v2 |
| 安装了 ITK component 就等于功能可用 | 当前代码没有 3D diffusion/vesselness production adapter | A1, A2 | Discard |
| vendored TetGen 就等于生产网格后端完成 | 许可和真实血管质量都尚需门控 | A1, A3 | Discard |
| v2 centerline-tree 应重新实现 centerline B | 会复制已经由 Shell A A13 交付的 thinning/distance kernel | A2, A3 | Discard; reuse v1 fallback and extend only tree/quality ownership |
| 全量 CTest 绿即可交付 | 回归不证明真实数据功能和实机交互 | A1, A5 | Discard |
| 搬入 Slicer/MITK 整机能最快复用 | 会引入第二宿主并破坏边界 | A3 | Discard |

## Ground Truths

1. **GT1:** XQ 已有可复用的 DICOM Source、Project/Scene、command、GUI、lineage 和 persistence；不需要重建宿主。
2. **GT2:** 当前主 Segmentation UI 明确暴露 threshold、人工 seed region grow，自写 service 负责 6-connected grow 和 largest component。
3. **GT3:** 隔离依赖基线已证明 VTK 9.3.0、ITK 5.4.0、GDCM 3.0.10、ITKVtkGlue 与所需 ITK components 可进入有界产品构建。
4. **GT4:** 生产 3D diffusion、vesselness、自动分割、ITK-VTK bridge、自动 tree 代码尚未存在。
5. **GT5:** 当前 LIDC 样本没有血管 reference mask，只能验 DICOM IO；最终算法需要新的真实 CTA/增强 CT 数据门。
6. **GT6:** vtkvmtk 和 ITKThickness3D 当前只是锁定 source candidates；TetGen 技术可用但仍是 research-only 许可边界。

## Reasoning Chain

- GT1 + A3 -> 保留 XQ 宿主，只增加 adapters/services，不引第二平台。
- GT2 + A2 -> ITK segmentation/morphology/components 必须接管默认影像主链，旧 kernel 退出生产入口。
- GT3 + GT4 -> 依赖可用不等于功能完成；按 preprocess -> segmentation -> bridge 逐段落地真实 adapter。
- GT5 + A4 -> 先完成真实数据/gold 隔离门，再冻结指标和做最终优化。
- GT6 + A2/A3 -> Shell A 先交付唯一 B fallback kernel；v2 tree 复用它并统一拥有自动端点、树质量和生产 A/B 选择；mesher 受许可与质量双门。
- A1 + A5 -> 只有完整 E2E、保存重开和实机验收能关闭总任务。

## Conclusion

**Recommended approach:** 复用现有 XQ 宿主，按 `D:\XQ` P0 顺序接入 GDCM/ITK/ITKVtkGlue/VTK/centerline backend/TetGen-MMG，并在最终 E2E 中撤下旧默认分割路径。

**Key insight:** 缺口不是再造一个“壳页面”，而是把成熟标准件真正接成唯一产品主链。

**Trade-offs:** 接受前置数据/许可/ABI门带来的顺序约束，换取可复现、可替换和不自欺的完整交付。

## Validation and Pre-Mortem

- [x] Every conclusion traces to at least one ground truth.
- [x] Every ground truth is covered by the design.
- [x] No phase was skipped.
- [x] Stress-tested with pre-mortem.

假设一年后失败，最可能原因：gold 泄漏让指标虚高；旧 threshold 页面仍是用户主路径；vtkvmtk/TetGen 只在玩具数据上跑；保存重开丢 provenance；child 全绿后提前交付。对应预防分别是生产/评价接口隔离、GUI cutover 检查、真实病例质量门、round-trip 门和总任务实机门。
