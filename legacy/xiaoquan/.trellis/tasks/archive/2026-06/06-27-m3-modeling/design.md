# M3 建模 — 技术设计(design)

> 基于 M0~M2 实际接口核查(XQSurfaceModel / XQContourGroup / 命令 / payload),非 plan 文字。
> 放样/封口语义参考 SimVascular/MITK(及底层 VTK),不看 XQ1。core 零依赖、命令/undo、LPS/mm。

## M0~M2 现状核查(已读源码)

- `XQSurfaceModel`(core 已有,较完整):`SurfaceGeometryHandle`(**句柄,只 pointCount/cellCount,无真实三角面**)、
  `ModelFace{faceId, name, FaceKind(Wall/Cap/Inlet/Outlet), capId, boundaryLoopIds}`、addFace/faceById、
  source/sourceContourGroupNode、PreservedVtpArrays。
- `XQContourGroup`(core 已有,放样输入齐全):`XQContour{contourId, pathArcLength, ContourFrame{origin,normal,xAxis,yAxis},
  type, points, closed}`、`orderedByPathPosition()`、`projectToFrame/unprojectFromFrame`。
- 命令(M0):AddNodeWithSourceRelationCommand / AddNodeCommand / ReplacePayloadCommand,复用,不新建命令类型。
- payload 范式(M1/M2):`XQXxxPayload : public XQPayload`,domainType()+clone()。
- 范式(M1/M2):service 只读 const 节点 + 只产命令、不碰 scene mutator;xq_services 只 link xq_core。
- **缺口**:SurfaceGeometryHandle 无真实三角面;无 XQSurfaceModelPayload;ModelingService/放样未实现。

## 核心设计抉择(参考 SimVascular 放样思路)

### 1. XQTriangleSurfaceGeometryHandle(core 新增,真三角面)
- 现有 SurfaceGeometryHandle 是句柄;放样要产真实三角网格 → 新建 core 自有真三角面 handle。
- `src/core/XQTriangleSurfaceGeometryHandle.h/.cpp`:vector<Point3> points + vector<三个 int 索引> triangles;
  pointCount/triangleCount、按索引取三角形、基本校验(索引在范围内)。零依赖。
- 与现有 SurfaceGeometryHandle 共存(后者句柄式给读入模型用,前者给生成模型用);XQSurfaceModel 的 geometry 可承载二者之一,或扩展存真三角面。
  **决策**:XQSurfaceModel 增持一个可选的 `XQTriangleSurfaceGeometryHandle`(生成路径用),不破坏句柄式字段(读入路径用),避免改动 M0 测试。

### 2. XQSurfaceModelPayload(core)
- `: public XQPayload`,domainType()==SurfaceModel,clone() 深拷贝(含三角面 + faces)。承载 XQSurfaceModel 入 scene。

### 3. ContourLoftInputBuilder + XQContourLoftInput(services/modeling)
- `buildLoftInput(contourGroup, options)`:从 contour group 构 `XQContourLoftInput`——
  按 orderedByPathPosition 排序的 contour、每 contour 重采样到统一点数(放样要求各截面点数一致、对应)、
  起始点对齐(最小化扭转,参考 SimVascular loft 的 align/rotation)。
- 校验:≥2 个有效 contour;contour 须有 frame(对应 path 标架)。

### 4. ModelingService(services/modeling,XQ 自有三角化,零依赖)
- `loftSurfaceCommand(name, loftInput, srcGroupId)`:相邻 contour 间放样成三角带(每相邻两环 N 点 → 2N 三角形),
  产 XQSurfaceModel(真三角面 + Wall face)+ AddNodeWithSourceRelationCommand(source=contour group)。
- `capModel(model, capOptions)`:对开口端(首/末环)做端面封口三角化(扇形/质心三角化),
  生成 inlet/outlet cap face(FaceKind::Inlet/Outlet + capId);封口后表面闭合。返回新 shared_ptr<XQSurfaceModel>。
- ModelFace 稳定 id:wall=固定 id,inlet/outlet cap 各自稳定 id + capId;face id 供 M4/M5 追踪。
- service 不持/不改 scene。

## 校验(prd)
- 放样≥2 contour;contour 可对应 path 标架(有 frame)。
- cap 仅作用于开口端;封口后表面闭合(可作体网格输入)——测试验证闭合性(每条边被偶数个三角形共享 / Euler 特征)。
- 非法返回带状态的失败,不抛异常。

## 验收对照(0007)
- 0007 的 contour group(M2 已读成节点,含 XQContour)放样出 XQSurfaceModel。
- `0007/Models/0090_0001.{mdl,vtp}` 可经现有 MDLModelReader 读入对照(点数/面数量级、ModelFace 概念对应)。
  **注**:首版 XQ 自有三角化与 SimVascular 原模型不会逐点一致;对照是"能放样出合理闭合表面 + ModelFace 稳定",非逐点等价。

## 风险
- 放样需各 contour 点数一致 + 起点对齐,否则三角带扭转/自交。重采样与对齐是关键(参考 SimVascular loft)。
- 封口闭合性必须测试(非闭合会让 M4 体网格失败)。
- 不破坏 M0~M2 的 30 测试。
- 真三角面 handle 与句柄式并存,XQSurfaceModel 改造要保 M0 的 surface_model 测试绿。
