# M4 网格 — 技术设计(design)

> 基于 M0~M3 实际接口核查(XQMesh / XQSurfaceModel / XQTriangleSurfaceGeometryHandle / 命令 / payload),非 plan 文字。
> 网格语义参考 SimVascular/MITK(及底层 VTK/TetGen/MMG);**首版不引入 TetGen/MMG**(plan/09 明确),用 XQ 自有四面体化。core 零依赖、命令/undo、LPS/mm。

## M0~M3 现状核查(已读源码)

- `XQMesh`(core 已有,接口齐全):`VolumeMeshHandle`/`SurfaceMeshHandle`(**句柄,只 pointCount/cellCount,无真实单元**)、
  `MeshRegion`、`MeshBoundaryFace{faceId, name, FaceKind, capId, cellIds}`、`MeshQualitySummary{min/max/mean/elementCount}`、
  `PreservedMeshArrays`、addBoundaryFace/boundaryFaceById、setSourceModelNode、setQuality。
- `XQSurfaceModel`(M3 扩展):`triangleGeometry()` 真三角面、`faces()`(ModelFace: faceId/kind/capId)、`sourceContourGroupNode`。
- `XQTriangleSurfaceGeometryHandle`(M3,真三角网格):points + 三索引 triangles、pointCount/triangleCount/point/triangle、is_valid。
- 命令:复用 `AddNodeWithSourceRelationCommand`/`AddNodeCommand`(M0),不新建命令类型。
- 范式:service 只读 const + 只产命令、不碰 scene mutator;xq_services 只 link xq_core;payload : public XQPayload + domainType + clone 深拷贝。
- `GeometryTypes`:add/sub/scale/dot/cross/norm/normalized/distance(够算四面体有符号体积 = dot(cross(b-a,c-a),d-a)/6 与质量指标)。
- 验收对照:`0007_H_AO_H/Meshes/0090_0001.{msh,vtu,vtp}`(.msh 证实原 kernel = TetGen + MMG);`MSHMeshReader`(M0 已建)可读入对照。

## 关键事实:正式 kernel 是 TetGen/MMG,首版不引入

- 0090_0001.msh 头部:`<mitk_mesh type="TetGen">`、`option UseMMG 1` → SimVascular/MITK 体网格 = TetGen,重网格 = MMG。
- plan/09:**MMG 首版用 VTK/自有三角化,不引入;需高质量重网格时再引入 `adapters/mmg`**。05-meshing 同。
- → M4 首版:**XQ 自有四面体化**跑通主线 + 保 face id 全链路 + 质量摘要;TetGen/MMG 留作后续 `adapters` kernel(记技术债)。

## 核心设计抉择

### 1. core 新增真实单元 handle(参照 M3 三角面 handle 模式)

- `src/core/XQTetVolumeMeshHandle.h/.cpp`:真四面体体网格,零依赖。
  - `vector<Point3> points` + `vector<array<int,4>> tets`(四索引);addPoint/addTet、pointCount/tetCount、point/tet。
  - `is_valid()`:非空 + 每四面体四索引互异且在范围内。
  - 不在 handle 内做几何有效性(体积符号)判断,留给 service 的质量摘要。
- 表面网格:**直接复用 M3 的 `XQTriangleSurfaceGeometryHandle`**(已是真三角网格),不另造。

### face→三角归属(正道:M3 生成时直接打 faceId 标签)

plan 硬要求「face id 全链路可追踪」。最干净的做法 = 来源处(M3)就给每个三角标 faceId,不在 M4 脆弱重建。
本次将**回改已归档 M3**(合理接口完善,M3 已提交、不违反「别动未提交改动」):

- `XQTriangleSurfaceGeometryHandle` 增加 per-triangle faceId 标签:
  - 内部 `vector<int> triangleFaceIds_`,与 triangles_ 同步增长。
  - `addTriangle(a,b,c)` 保持原签名(faceId 默认 0=未标记,向后兼容,M3 现有测试不破);
    新增重载 `addTriangle(a,b,c,faceId)`;`triangleFaceId(triIdx)` / `setTriangleFaceId(triIdx,faceId)`。
- `ModelingService::loftSurface`:放样带三角用 `addTriangle(.., wallFaceId())`(=1)。
- `ModelingService::capModel`:cap 扇三角用 `addTriangle(.., cap.faceId)`(inlet=2 / outlet=3);
  拷贝原 wall 几何时连同 triangleFaceIds 一起拷(handle 拷贝构造已含该 vector)。
- M3 测试补充:loft 后每三角 faceId==wall;cap 后 wall 三角 faceId==1、inlet 扇==2、outlet 扇==3。
  → 这是对 M3 的加强,不是破坏;M3 的 32 测试 + 新断言全绿才算。
- M4 直接读 `triangleFaceId(t)` 做边界面映射,**归属逻辑消失**。

- `XQMesh` 扩展(保留 M0 句柄路径,不破 test_mesh):
  - 增持可选 `std::shared_ptr<XQTriangleSurfaceGeometryHandle> surfaceTriangles_`(真表面网格几何) + getter/has。
  - 增持可选 `std::shared_ptr<XQTetVolumeMeshHandle> volumeTets_`(真体网格几何) + getter/has。
  - 原 VolumeMeshHandle/SurfaceMeshHandle 句柄字段保留(读入路径/句柄式),M0 测试不动。

### 2. XQMeshPayload(core)

- `: public XQPayload`,domainType()==Mesh,clone() 深拷贝(value copy XQMesh + 深拷 surfaceTriangles_/volumeTets_ handle)。承载 XQMesh 入 scene。
- (核查 XQDomainType 是否已有 Mesh 枚举;若无则补,并同步所有 switch/mock。)

### 3. SurfaceMeshService(services/meshing)

- `buildSurfaceMesh(model, params)`:从 `XQSurfaceModel.triangleGeometry()` 产表面 `XQMesh`。
  - 首版策略:表面网格 = 模型三角面的拷贝(可选按 params 做简单边长上限细分,首版可不细分,直接等同模型三角面 —— 验收只要"能生成表面网格 + 保 face id + 质量摘要")。
  - 每个 ModelFace(wall/inlet/outlet)→ `MeshBoundaryFace`(同 faceId/kind/capId + 该 face 覆盖的三角 cellIds)。
    - face→三角归属:**直接读 `XQTriangleSurfaceGeometryHandle::triangleFaceId(t)`**(M3 已在生成时标好)。
      按 faceId 把三角索引归入对应 MeshBoundaryFace 的 cellIds;kind/capId 从模型 faces() 取。归属逻辑无重建、无几何推断。
  - 质量摘要:三角形质量(如 2*inradius/circumradius 的简化,或最小角代理)min/mean/max + elementCount。
- `buildSurfaceMeshCommand(scene, newMeshId, name, model, params)` → 复用 AddNodeWithSourceRelationCommand(source = model 节点)。

### 4. VolumeMeshService(services/meshing,XQ 自有四面体化,零依赖)

- **算法:质心星形剖分(star tetrahedralization)** —— capModel 质心扇封口的体积版推广。
  - 输入:闭合 2-流形三角表面(M3 capModel 输出 / 或表面网格的三角 handle)。
  - 闭合性前置校验:每条无向边恰被 2 个三角共享(复用 M3 测试逻辑);**非闭合 → 返回诊断状态 NotClosed,不生成**(plan 硬要求)。
  - 算质心 C = 所有表面顶点均值;C 加入点列。
  - 对每个表面三角形 (a,b,c) 生成四面体 (a,b,c,C)。→ tetCount == 表面 triangleCount。
  - **边界面映射**:每个表面三角形继承其 `triangleFaceId(t)`(M3 标好的 faceId)→ 对应四面体的边界 cellId 计入该 `MeshBoundaryFace`(faceId/kind/capId 原样传递)。满足"face id/cap id 端到端"。
  - 质量摘要:每四面体有符号体积 V=dot(cross(b-a,c-a),d-a)/6;质量代理 = 归一化形状比(如 V 与边长立方比,或简单用 |V| 与最差 |V|)。统计 min/mean/max + count。退化(|V|<eps)计入最差值,不丢弃(暴露而非隐藏)。
- `buildVolumeMeshCommand(scene, newMeshId, name, surfaceOrModel, params)` → AddNodeWithSourceRelationCommand(source = 表面模型/表面网格节点)。
- **算法局限(记技术债)**:质心星形剖分仅对「相对质心星形可见」的域正确;弯曲主动脉若质心落在体外/壁附近会产翻转或退化四面体 —— 这正是 TetGen 后续解决的。首版用质量摘要暴露最差单元,验收对象 0007 单段管道在可接受范围。

### 5. transferBoundaryFaces(meshA → meshB)

- 把 meshA 的 `MeshBoundaryFace`(faceId/kind/capId)语义传到 meshB(表面网格→体网格的 face 表传递),cellIds 按 meshB 自身单元重映射。
- 静态工具,纯数据搬运。

## 校验(prd / plan)

- 体网格输入表面必须闭合 → 非闭合发诊断(状态)而非静默生成。
- 输出附质量摘要(单元数 + 最差质量)。
- face id / cap id 端到端保留(表面模型 ModelFace → 表面网格 MeshBoundaryFace → 体网格 MeshBoundaryFace)。
- 非法输入返回带状态失败,不抛异常。

## 测试(目标:全用 CHECK 宏,Release 真绿;不破 M0~M3 的 32 测试)

- `tests/services/meshing/SurfaceMeshServiceTest.cpp`:
  - 表面网格点数/三角数 == 模型;每 ModelFace → MeshBoundaryFace 且 faceId/kind/capId 一致;质量摘要 elementCount 正确、min≤mean≤max;命令入 scene + source relation + undo/redo;空/无三角几何 → 失败状态。
- `tests/services/meshing/VolumeMeshServiceTest.cpp`:
  - 用 M3 ContourLoftInputBuilder+loft+cap 造闭合管道 → 体网格:tetCount==表面 triangleCount、pointCount==表面点数+1(质心)、所有四面体有符号体积同号(无翻转,星形域)、边界面 faceId/cap 保留、质量摘要;**开口(未封口)表面 → NotClosed 诊断**;命令入 scene + undo/redo;payload 深拷贝。
- `tests/services/meshing/MeshingIntegrationTest.cpp`(端到端,读真实 0007):
  - 读 0007 主动脉 .ctgr → loft → cap → buildSurfaceMesh → buildVolumeMesh → 校验闭合、face id 全链路、质量摘要非空。

## 验收对照(0007)

- 0007 表面模型(M3 产)→ 表面网格 + 体网格;`0007/Meshes/0090_0001.{msh,vtu}` 经 MSHMeshReader 读入对照。
  **注**:XQ 自有星形剖分与原 TetGen 网格不逐点/逐单元一致;对照是"能生成合理闭合体网格 + face id 全链路 + 质量摘要",非逐单元等价(同 M3)。

## 风险

- 星形剖分对非星形域产翻转四面体 → 质量摘要须能暴露(min 体积符号/最差质量)。技术债:后续 TetGen 进 adapters。
- 回改 M3 加 triangleFaceId 标签:addTriangle 原签名保留(默认 faceId=0),M3 现有 32 测试不破;新增标签断言。须重跑 M3 全量验证。
- 不破 M0~M3 的 32 测试(尤其 test_mesh 的句柄路径)。
</content>
