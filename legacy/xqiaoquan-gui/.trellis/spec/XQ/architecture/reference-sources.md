# 参考来源规则(reference sources)

> 这是一条**硬性规则**,适用于 XQ 所有函数库能力的实现与设计。优先级高于 plan 旧文档里
> 任何"XQ1 仅作行为参考"之类的措辞——以本规则为准。

---

## 库能力一律参考 SimVascular + MITK

凡涉及函数库能力(path / segmentation / modeling / meshing / flow 的**算法语义、数据语义、
坐标处理、文件格式解释**),一律参考 **SimVascular** 和 **MITK** 的处理方式:

- 它们若底层调用了外部依赖(VTK / ITK / GDCM 或其它),就**顺着去看那些库怎么处理**,
  以底层库的官方语义为准。
- 参考的是**行为与语义**,不是直接搬它们的架构——XQ 的分层与"外部库只当 kernel"规则不变
  (见 `index.md`、`external-libs.md`)。

## XQ1 全面作废,任何场景都不参考

- `XQ1/` **不作为任何参考来源**——库能力、UI、工作流,**一律不看**。
- 原因:XQ1 不止是胶水层产物,本身是**失败的产物,有大量小问题**;照着它学会踩坑。
- plan 文档里出现的"XQ1 仅作行为参考""M7 复用 XQ1"之类提法,**在 XQ 实现里全部作废**。

## 代码起点 vs 参考来源(别混淆)

- **代码起点**:`XQrebuild/`(只读),按需搬入已实现的干净 core / io / adapter。
- **行为/算法参考**:SimVascular + MITK(及其底层 VTK / ITK / GDCM)。
- **排除**:`XQ1/`(失败产物,任何场景都不参考)。

> 一句话:搬代码看 XQrebuild,学算法看 SimVascular/MITK,**永远别看 XQ1**。

## 自动 Path/中心线前人实现的复用边界

- `D:\XQ\可复用工作打包说明.md` 与 `D:\XQ\research` 是本路线已选前人工作的只读权威
  清单；开始重写 Path/中心线 kernel 前必须先检索这些位置。
- 最终 `SegmentationMask -> Path` 默认复用 ITKThickness3D 三维 thinning 和 ITK 物理距离图；
  XQ 只拥有 geometry adapter、图/剪枝策略、Path/Profile 契约和错误/lineage 处理。
- locked ITKMinimalPathExtraction 提供最小代价路径 optimizer，可用于显式端点之间的重连；
  它不提供 XQ 的自动端点/器官拓扑策略，也不等同于完整 DICOM->最终 Path 产品。
- `XQ/third_party/itk_minimal_path` 存在不代表已经产品化。只有 production adapter/service/GUI
  target 真正链接并执行后才能声称复用；当前仅 development analyzer 消费它。
- TubeTK 可复用管状结构处理能力和 MinimalPath 示例，但其旧 VTK 中心线功能已停维护，不能
  作为完整最终中心线承诺。
- vtkvmtk 只允许最小 C++ 抽取的可选升级；禁止 vmtk SuperBuild、第二套 ITK/VTK 或 Python
  runtime。
- 用户要求“取代旧手工 Path”时，手工 control-point/contour 路线不能继续作为默认验收主线。
  必须先实现并验证自动产品链，再按任务契约移除或降级旧入口。
