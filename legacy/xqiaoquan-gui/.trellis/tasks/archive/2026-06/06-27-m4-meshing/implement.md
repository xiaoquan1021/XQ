# M4 网格 — 实现清单(implement)

> 严格按 design.md。零外部依赖(core/services 不 include VTK/ITK)。命令复用 M0。测试用 CHECK 宏(Release 真绿)。
> 验收前不破 M0~M3 的 32 测试。

## 步骤 0:回改 M3 — XQTriangleSurfaceGeometryHandle 加 per-triangle faceId 标签

文件:`src/core/XQTriangleSurfaceGeometryHandle.h/.cpp`
- 内部加 `std::vector<int> triangleFaceIds_`,与 triangles_ 同步。
- `addTriangle(int a,int b,int c)` 保留(faceId 默认 0);新增 `addTriangle(int a,int b,int c,int faceId)`。
  两者都 push triangleFaceIds_(默认版 push 0)。
- 新增 `int triangleFaceId(std::size_t i) const`、`void setTriangleFaceId(std::size_t i,int faceId)`。
- 拷贝构造(默认即可,vector 随之拷)——确认 capModel 里 `make_shared<...>(src)` 会连标签一起拷。

文件:`src/services/modeling/ModelingService.cpp`
- loftSurface 放样带:`geometry->addTriangle(a0,a1,b1, wallFaceId())` / `addTriangle(a0,b1,b0, wallFaceId())`。
- capModel cap 扇:在已知 cap.faceId 后,`geometry->addTriangle(centroidIndex,a,b, cap.faceId)`。
  注意 cap.faceId 计算在加三角之后 → 调整顺序:先定 cap.faceId 再加扇三角(或加完扇三角后回填标签)。取「先算 cap.faceId,再加扇三角带 faceId」。

文件:`tests/services/modeling/ModelingServiceTest.cpp`
- loft 后:每三角 triangleFaceId == wallFaceId()。
- cap 后:wall 段三角 faceId==1;inlet 扇三角 faceId==2;outlet 扇三角 faceId==3(按含质心点判断或按已知数量区间)。

→ 跑 M3 两测试 + 全量,确认仍真绿(标签是新增,旧断言不动)。

## 步骤 1:core 新增 XQTetVolumeMeshHandle

文件:`src/core/XQTetVolumeMeshHandle.h/.cpp`(零依赖,仿 XQTriangleSurfaceGeometryHandle)
- `using Tet = std::array<int,4>;`
- `int addPoint(const Point3&)`、`void addTet(int,int,int,int)`、pointCount/tetCount、`const Point3& point(i)`、`const Tet& tet(i)`、points()/tets()。
- `bool is_valid()`:非空 + 每 tet 四索引互异且在 [0,pointCount) 内。

## 步骤 2:XQMesh 扩展

文件:`src/core/XQMesh.h/.cpp`
- include XQTriangleSurfaceGeometryHandle.h、XQTetVolumeMeshHandle.h。
- 增 `setSurfaceTriangles/surfaceTriangles/hasSurfaceTriangles`(shared_ptr<XQTriangleSurfaceGeometryHandle>)。
- 增 `setVolumeTets/volumeTets/hasVolumeTets`(shared_ptr<XQTetVolumeMeshHandle>)。
- 原句柄字段/方法不动。test_mesh 不破。

## 步骤 3:XQMeshPayload

文件:`src/core/XQMeshPayload.h`(仿 XQSurfaceModelPayload)
- `: public XQPayload`,explicit ctor(XQMesh)。
- domainType()==XQDomainType::Mesh。
- clone():value copy XQMesh,若 hasSurfaceTriangles/hasVolumeTets 则深拷对应 handle(make_shared 拷贝构造)写回 copy。
- model()/mesh() const + 非 const 取引用。

## 步骤 4:SurfaceMeshService

文件:`src/services/meshing/SurfaceMeshService.h/.cpp`(static,纯域,只 link xq_core)
- `struct Params {};`(首版空;预留细分边长上限,首版不细分)。
- enum Status { Ok, InvalidModel, NullScene }。
- `Result buildSurfaceMesh(const XQSurfaceModel& model, const Params&)`:
  - model 无 triangleGeometry 或 !is_valid → InvalidModel。
  - 表面网格三角 handle = 模型三角 handle 的拷贝(含 faceId 标签)。
  - 建 XQMesh:setSurfaceTriangles;setSurfaceMesh 句柄 setCounts(pointCount, triangleCount)(兼容句柄路径);
    若 model.hasSourceContourGroupNode 设 sourceModelNode 留给命令(实际 source = model 节点,见命令)。
  - 边界面:遍历 model.faces(),每 ModelFace → MeshBoundaryFace(faceId/name/kind/capId),cellIds = 所有 triangleFaceId==faceId 的三角索引。addBoundaryFace。
  - 质量摘要:三角形质量 q = 简化(如 inradius/circumradius * 常数 或 最小内角/60° 归一);min/mean/max + elementCount=triangleCount。setQuality。
- `CommandResult buildSurfaceMeshCommand(XQScene*, newMeshId, name, model, params)`:
  - 失败状态透传;成功建 XQMeshPayload + XQDataNode(Mesh) + AddNodeWithSourceRelationCommand(source = model 的节点 id,由调用方传 modelNodeId)。
    → 接口需带 modelNodeId 参数(源关系指向 scene 里的 model 节点)。

## 步骤 5:VolumeMeshService

文件:`src/services/meshing/VolumeMeshService.h/.cpp`(static,纯域,零依赖)
- enum Status { Ok, NotClosed, InvalidSurface, NullScene }。
- `Result buildVolumeMesh(const XQTriangleSurfaceGeometryHandle& surface, const std::vector<ModelFace>& faces, const Params&)`
  (或直接收 XQSurfaceModel;取收 surface + faces 更通用,兼容表面网格/表面模型两种入口)。
  - 闭合性校验:每无向边恰被 2 三角共享;否则 NotClosed(不生成)。
  - 质心 C = 表面所有点均值。
  - 体网格 handle:拷所有表面点;addPoint(C) → centroidIndex;对每表面三角 (a,b,c) addTet(a,b,c,centroidIndex)。tetCount==surface.triangleCount。
  - 边界面:每表面三角的边界面(a,b,c)继承 surface.triangleFaceId(t);按 faceId 归入 MeshBoundaryFace.cellIds(cellId = tet 索引 = 三角索引)。kind/capId 从 faces 里按 faceId 查。
  - 质量摘要:每 tet 有符号体积 V=dot(cross(b-a,c-a),d-a)/6;质量代理(如 |V| 归一,或 12*(3V)^(2/3)/Σedge² 的简化);min/mean/max + count。退化 |V|<eps 计入最差,不丢弃。
  - 建 XQMesh:setVolumeTets;setVolumeGrid 句柄 setCounts;addBoundaryFace;setQuality。
- `CommandResult buildVolumeMeshCommand(XQScene*, newMeshId, name, surface, faces, sourceNodeId, params)` → AddNodeWithSourceRelationCommand(source=sourceNodeId)。

## 步骤 6:transferBoundaryFaces

- `static void transferBoundaryFaces(const XQMesh& from, XQMesh& to)`:把 from 的 MeshBoundaryFace 的 faceId/name/kind/capId 复制到 to(cellIds 不跨网格搬,留空或由 to 自填)。放 VolumeMeshService 或独立 util,二选一,注明。

## 步骤 7:CMake 注册

文件:`XQ/CMakeLists.txt`
- xq_core 源加 XQTetVolumeMeshHandle.cpp(XQMesh.cpp 已在)。
- xq_services 源加 SurfaceMeshService.cpp、VolumeMeshService.cpp。
- 新增 test_surface_mesh_service / test_volume_mesh_service / test_meshing_integration(integration link xq_services + xq_io + 设 XQ_CTGR_DIR + offscreen 属性,仿 modeling_integration)。
- add_test 三条。

## 步骤 8:测试(全 CHECK 宏)

- `tests/services/meshing/SurfaceMeshServiceTest.cpp`:见 design 测试段。
- `tests/services/meshing/VolumeMeshServiceTest.cpp`:见 design;含开口表面 → NotClosed;闭合管道所有 tet 体积同号。
- `tests/services/meshing/MeshingIntegrationTest.cpp`:读真实 0007 .ctgr → loft → cap → surfaceMesh → volumeMesh → 校验闭合/face 全链路/质量。

## 完成门槛(worker 自检)

- 全新 build 目录构建零错误;全量 ctest 真绿(M0~M3 的 32 + M4 新增,预期 ~35+)。
- 自己做一次假绿抽查(篡改 volume 闭合校验或 tet 体积统计 → Release FAIL → 恢复 → PASS),把结果写进汇报。
- 零外部依赖(grep 确认 core/services/meshing 无 VTK/ITK include)。
- 不停下问小抉择:质量指标公式、Params 细节等一律参考 SimVascular/MITK 自定合理默认,直接做到门槛达成再汇报。
</content>
