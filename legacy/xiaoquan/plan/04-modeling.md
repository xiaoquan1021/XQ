# 04 建模(M3)

## 目标

由 contour 放样生成表面模型,并为血管模型做封口(cap),保留面元(face)元数据。首版用
XQ 自有三角化,不把 VTK / OCCT 的对象图带进业务代码。

## 所有权

```
src/core/XQSurfaceModel.h / .cpp                  表面模型 payload(XQrebuild 已有,扩展)
src/services/modeling/ModelingService.h / .cpp
src/services/modeling/ContourLoftInputBuilder.h / .cpp
tests/services/modeling/ModelingServiceTest.cpp
```

## 输入 / 输出

- 输入:contour 放样输入(`XQContourLoftInput`,由 builder 从 contour group 构建)、来源 contour
  group 节点 id、本服务自有的建模选项。
- 输出:链接到来源 contour group 的 `XQSurfaceModel`;XQ 自有三角面几何 handle
  (`XQTriangleSurfaceGeometryHandle`);壁面与可选进出口 cap 的稳定 `ModelFace` 记录;可选入场景命令。

## 公开 API

```cpp
buildLoftInput(contourGroup, options)            -> XQContourLoftInput
loftSurfaceCommand(name, loftInput, srcGroupId)  -> AddNodeWithSourceRelationCommand
capModel(model, capOptions)                      -> shared_ptr<XQSurfaceModel>   // 生成 inlet/outlet cap face
```

## 算法 kernel 边界

- 首版:XQ 自有三角化(contour 间放样 + 端面封口)。
- VTK / OCCT 可作为后续更强建模的 kernel,置于 `adapters`,**不进公开 API**。
- 留意:后续评估"AI 直接从掩膜/几何生成表面"能否替代部分放样(见 09)。

## 面元元数据(ModelFace)

- 每个面有稳定 id;区分 wall / inlet cap / outlet cap。
- face id 必须在后续网格(05)与仿真边界条件(06)中保持可追踪。

## 校验

- 放样至少需要 2 个有效 contour;contour 必须可对应到 path 标架。
- cap 仅作用于开口端;封口后表面应闭合(供体网格)。

## 禁止

- 公开 API 不依赖 VTK / OCCT / Qt / 插件;不直接改 scene。

## 验收

- 由 `0007` 的 contour group 放样出表面模型;`0007/Models/*.mdl,*.vtp` 可读入对照。
- ModelFace 记录稳定;封口后表面闭合,可作为体网格输入。
- 单测覆盖放样、封口、面元 id 稳定性、undo。
