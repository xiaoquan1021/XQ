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
