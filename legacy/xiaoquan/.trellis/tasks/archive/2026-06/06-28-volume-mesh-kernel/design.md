# Design — 两段式体网格 kernel(TetGen PLC + MMG3D remesh)

> 配合 `prd.md`。讲 kernel 适配方式、依赖边界、faceId 映射、体单元面级连接、降级。
> 全部决策依据已验证的 `research/simvascular-volume-mesh.md`(SV 源码逐行)+ findings + 阶段 0 实测,非脑补。

## 0. 设计原则

- **算法 = SimVascular 同款 TetGen PLC 约束四面体化(调研定论)**:输入闭合表面的每个三角作为 TetGen
  facet 硬约束(`-p`),`tetrahedralize` 填充内部,输入表面恒为输出边界。补 vtkDelaunay3D 三宗缺陷
  (凸包/不保凹边界/无 marker 通道,阶段 0 实测否决)。
- **两段式**:① TetGen 初始填充 + `-q`/`-a` 质量/尺寸控制 ② MMG3D remesh(`nosurf` 保 faceId,可选)。
- **kernel 包在 adapter,暴露纯 XQ 函数**:`tetgen.h` / `libmmg3d.h` 不出各自 adapter 的 .cpp。
- **`xq_services` 不 link kernel**:经 `ITetMesher` 接口注入(仿 M6 AiService Backend)。
- **星形剖分作 fallback**(无注入时),不删。
- **体单元面级 faceId(为大规模 CFD)**:用 TetGen `-nn` 的 `adjtetlist` 直接拿「边界面→相邻 tet」连接,
  O(1) 填,零几何查询。优于 SV 的「faceId 只在独立 surface、求解时 vtkCellLocator 重匹配」。

## 1. ITetMesher 接口(`src/services/meshing/ITetMesher.h`,零外部)

```cpp
namespace xq {
class XQTriangleSurfaceGeometryHandle;
class XQTetVolumeMeshHandle;

struct TetMeshParams {
    double targetEdgeLength = 0.0; // hmax / TetGen -a 反算的目标边长;0=kernel 默认
    double minEdgeLength = 0.0;    // hmin(MMG 段用)
    double minRadiusEdgeRatio = 0.0; // TetGen -q 半径/边长比;0=kernel 默认
    // 预留 grading 等
};

// 一条边界面记录:faceId(=输入三角 faceId)+ 它在体网格里所属的体单元 + 局部面序号。
// tetIndex/localFace 由 TetGen adjtetlist 直接给(neighout=2),零几何查询。
struct TetBoundaryFace {
    std::array<int, 3> tri;  // 边界三角的三个点索引(指向 result.mesh 的 points)
    int faceId = 0;          // 输入三角 faceId,经 facetmarker→trifacemarker 守恒
    int tetIndex = -1;       // 所属体单元索引(指向 result.mesh 的 tets)
    int localFace = -1;      // 0..3:该 tet 的哪个局部面是这个边界面
};

struct TetMeshResult {
    bool ok = false;
    std::string message;
    std::shared_ptr<XQTetVolumeMeshHandle> mesh;     // 含 tet
    std::vector<TetBoundaryFace> boundaryFaces;      // 边界面 + faceId + 体单元面级连接
};

// 闭合表面(每三角带 faceId)→ 体网格(边界面 faceId + 体单元面级连接)。纯 XQ 类型进出。
class ITetMesher {
public:
    virtual ~ITetMesher() = default;
    virtual TetMeshResult tetrahedralize(const XQTriangleSurfaceGeometryHandle& surface,
                                         const TetMeshParams& params) = 0;
};
} // namespace xq
```
- 定义在 `services`,**零外部依赖**。`xq_services` 仍只 link `xq_core`。
- `TetBoundaryFace` 是接口返回的中间结构;service 把它落进 `XQMesh` 的 `MeshBoundaryFace`(见 §4)。

## 2. 实现体(各 adapter)

### 2a. TetGen 填充 `adapters/tetgen/TetGenTetMesher.{h,cpp}`(实现 ITetMesher)

`.cpp` 流程(`tetgen.h` 不出 .cpp;字段名已核 `tetgen.h:195~282`):

```cpp
// ① 预处理:校验闭合+流形+单连通(见 §3),winding 定向。失败 → TetMeshResult{ok=false}。
// ② XQ 表面 → tetgenio in:
tetgenio in, out;
in.firstnumber = 0;
in.numberofpoints = N;
in.pointlist = new REAL[3*N];                 // 顶点 (LPS/mm 原样)
for (p) { in.pointlist[3i..] = {x,y,z}; }
in.numberoffacets = M;                          // M = 表面三角数
in.facetlist = new tetgenio::facet[M];
in.facetmarkerlist = new int[M];
for (tri i) {                                   // 每个三角 = 1 facet(PLC 硬约束),参 SV ConvertSurfaceToTetGen:128-155
    facet& f = in.facetlist[i];
    f.numberofpolygons = 1; f.numberofholes = 0; f.holelist = nullptr;
    f.polygonlist = new tetgenio::polygon[1];
    polygon& pg = f.polygonlist[0];
    pg.numberofvertices = 3; pg.vertexlist = new int[3]{a,b,c};
    in.facetmarkerlist[i] = triangleFaceId(i);  // faceId 进(参 SV AddFacetMarkers:206)
}
// ③ tetgenbehavior + 跑:
tetgenbehavior b;
b.plc = 1;            // -p:PLC 四面体化(算法核心)
b.neighout = 2;       // -nn:输出 trifacelist 的 adjtetlist(体单元面级连接来源)
b.quality = 1; b.minratio = params.minRadiusEdgeRatio>0 ? .. : 1.414; // -q
if (params.targetEdgeLength>0) { b.fixedvolume = 1; b.maxvolume = a*a*a/(6*sqrt2); } // -a(由边长反算,参 SV :1363)
b.nobisect = 1;       // -Y:不分裂输入边界面(保边界/保 faceId 对齐;按实测调,见风险)
try { tetrahedralize(&b, &in, &out); } catch (int code) { return {ok=false, "tetgen "+code}; }
// ④ out → XQ:
//   out.tetrahedronlist[4k..]            → XQTetVolumeMeshHandle::addTet
//   out.pointlist                        → addPoint(可能含 Steiner 点,> 输入点数)
//   out.trifacelist[3i..]                → TetBoundaryFace.tri
//   out.trifacemarkerlist[i]             → TetBoundaryFace.faceId(守恒,参 SV ConvertToVTK:513)
//   out.adjtetlist[2i..]                 → 该边界面相邻 tet;取非 hull 的那个 → tetIndex(参 SV :505 用 adjtetlist[2i+1])
//   localFace:在该 tet 的 4 个面里找顶点集合 == tri 的那个面序号(O(1),tet 只 4 面)
```

- **公开头零 tetgen 类型**(`tetgen.h` 仅 .cpp)。
- **Steiner 点**:TetGen `-p` 会在内部/边界插点,`out.numberofpoints > in.numberofpoints` 正常;
  `-Y` 抑制边界插点(保输入边界三角原样 → faceId/连接对齐最简单)。是否开 `-Y` 在阶段实测定
  (开了边界不细分但可能质量略降;SV 默认未开 `-Y`,靠 marker 继承)。
- **`tetrahedralize` 抛 int 错误码**:try/catch 包,转 Status。

### 2b. MMG3D remesh `adapters/mmg/MmgVolumeRemesher.{h,cpp}`(可选,装饰 ITetMesher)

吃 TetGen 初始体(`XQTetVolumeMeshHandle` + 边界面 faceId)→ MMG3D remesh → `XQTetVolumeMeshHandle`。

`.cpp` 流程(findings Q4 骨架,`libmmg3d.h` 不出 .cpp):
```c
MMG3D_Init_mesh(MMG5_ARG_start, MMG5_ARG_ppMesh,&mesh, MMG5_ARG_ppMet,&sol, MMG5_ARG_end);
MMG3D_Set_meshSize(mesh, np, ne/*>0,TetGen 初始体*/, 0, nt, 0, 0);
for(p) MMG3D_Set_vertex(mesh, x,y,z, 0, i+1);
for(t) MMG3D_Set_tetrahedron(mesh, a,b,c,d, 0, i+1);
for(b) MMG3D_Set_triangle(mesh, a,b,c, faceId/*=ref*/, i+1);   // ref 承载 faceId
MMG3D_Set_iparameter(mesh, sol, MMG3D_IPARAM_nosurf, 1);        // 保边界 → faceId 守恒
// 可选 MMG3D_Set_dparameter hmax/hmin
int ier = MMG3D_mmg3dlib(mesh, sol);                           // SUCCESS/LOWFAILURE/STRONGFAILURE
// 取回:Get_meshSize / Get_tetrahedron / Get_triangle(ref→faceId)
MMG3D_Free_all(...);
```
- **守恒前提(findings 实证)**:喂 MMG 的边界三角必须与体网格实际边界面**精确吻合**
  (被一个 tet 独占的面)。TetGen 输出的 `trifacelist` **天然就是体网格边界面**(被一个 tet 独占),
  直接喂即守恒——比 findings 里 bend_vol2「从 tet 边界面导出」更省事(TetGen 已经给了)。叠加 `nosurf=1` 保险。
- **返回码**:`MMG5_SUCCESS` 才 ok;`LOWFAILURE` 按策略接受/告警;`STRONGFAILURE` 失败。
- **dll 运行时**:测试/app 需 `mmg-5.3.9/bin` 在 PATH。

### 2c. 组合 mesher `TetGenThenMmg`(实现 ITetMesher)

内部先调 2a 得初始体 + 边界面,再调 2b remesh。faceId 两段守恒:facetmarker→trifacemarker→MMG ref→Get_triangle。
MMG 段后 `adjtetlist` 不再可用(MMG 不出邻接),体单元面级连接需在 MMG 输出上**重建一次**
(从 tet 提取被一个 tet 独占的面,匹配回带 ref 的边界三角;O(tet) 一次,仍零 vtkCellLocator)。

## 3. 输入预处理(TetGen 硬门槛)

TetGen `-p` 要求输入闭合 + 流形 + 单连通(SV `CheckSurfaceMesh:1566`:FreeEdges/NonManifoldEdges=0)。

- **闭合/流形校验**:复用现状 `VolumeMeshService::isClosedManifold`(每边恰 2 三角)。不满足 → Status::NotClosed。
- **winding 定向**:M3 表面 winding 全局不一致(memory `xq-surface-winding-not-consistent`)。
  TetGen 的 facet 是无向多边形(只用顶点集合,不依赖三角朝向),**winding 不一致对 PLC 约束本身无害**;
  但若后续要一致法线/有符号体积判定,按有符号体积自定向(铁律,别假设同向)。
  → 本任务:**不做完整表面修复**(out of scope);只做闭合校验,winding 交给 TetGen 的无向 facet 容忍。
- **自交**:TetGen 对自交失败抛错;依赖 M3/M4 无自交假设,失败 → Status::InvalidSurface + message。

## 4. faceId 全链路 + 体单元面级连接(铁律 3 + 大规模 CFD 优化)

```
表面三角 triangleFaceId(i)
  → [TetGen] in.facetmarkerlist[i] = faceId(facet 顺序 = 三角顺序,隐式对齐,参 SV AddFacetMarkers:206)
  → tetrahedralize(-p):输入 facet 恒为输出边界 subface,marker 继承
  → out.trifacemarkerlist[i] → TetBoundaryFace.faceId(守恒,SV 实证 + 本任务小用例实测)
  → [体单元面级连接] out.adjtetlist[2i..] → 取非 hull 相邻 tet → tetIndex;
       在该 tet 4 个局部面找顶点集 == trifacelist[3i..] → localFace
  → service 落进 XQMesh.MeshBoundaryFace:faceId 分组,cellIds 存 (tetIndex 编码) ;
       kind/capId/name 按 faceId 从 ModelFace faces 查(同现状 byFaceId 逻辑)
```

**为什么要面级连接(对照 SV / 现状的不足):**
- SV 的 faceId 只在配套 surface `vtkPolyData` 上(`ConvertToVTK:541`),体单元 `vtkUnstructuredGrid` 不带;
  求解器要施加边界条件时,需 vtkCellLocator 把 surface 三角**几何匹配**回体单元面——大规模网格 O(N log N) 性能坑。
- 本任务用 TetGen `-nn` 的 `adjtetlist`(它**已经免费算好**边界面↔相邻 tet),O(1) 直接建 `(tetIndex, localFace)`,
  零几何查询。服务你的大规模血流 CFD(inlet/outlet/wall 边界条件组装、WSS/OSI 壁面后处理按面遍历)。

**`MeshBoundaryFace.cellIds` 语义**:现状字段(`XQMesh.h:59`)本就是「关联体单元」的位置,现状星形未认真填。
本任务把它填实为「该 faceId 的边界面所属体单元索引列表」;localFace 若需独立存,扩 `MeshBoundaryFace`
增 `std::vector<int> localFaces`(与 cellIds 平行)或在 design review 时定是否够用。**最小改动优先:先填 cellIds,
localFace 按需扩**。

## 5. 依赖边界(关键决策)

**方案 A(定):依赖注入 ITetMesher。**
- `VolumeMeshService::buildVolumeMesh` 增可选参数 `ITetMesher* mesher = nullptr`。
  - `nullptr` → 现状星形剖分(fallback,铁律 4)。
  - 非空 → 调 `mesher->tetrahedralize(...)`,result 的 faceId + 面级连接回填 `MeshBoundaryFace`。
- `xq_services` 只定义 `ITetMesher` + 不变的星形路径,**不 link TetGen/MMG/VTK**。
- 装配在 controller/app 层:`adapters/tetgen` 提供 `TetGenTetMesher`,`adapters/mmg` 提供 remesh,
  组合 `TetGenThenMmg`,上层 new 出来注入 service。与 M6 AiService Backend 范式一致。

依赖方向:`app/controllers → adapters/{tetgen,mmg} → core`;`services` 只定义接口 + 星形 fallback。

## 6. TetGen 引入 + 修复(官方 1.6 + SV outsubfaces 修复)

- **取官方 TetGen 1.6**(tetgen.org),装进 `Externals/install/windows-x64/tetgen-1.6.0/`(头+lib);
  或 vendoring 源码(`tetgen.cxx`+`predicates.cxx`+`tetgen.h`)进 XQ `third_party/tetgen/`。编库需 `-DTETLIBRARY`。
- **打 SV `outsubfaces` 修复**:SV 对 `tetgenmesh::outsubfaces(tetgenio* out)` 的边界面邻接修正
  (`README.simvascular` 记录):原版用 `stpivot`+`fsymself` 取边界面两侧相邻 tet 有缺陷;
  修复引入 `abuttingtet2 = fsym(abuttingtet)`,两侧各自独立 `ishulltet` 判定再 `elemindex`,
  保证 `adjtetlist` 两侧 tet 索引正确。**这直接关系体单元面级连接(§4)的正确性,必须打。**
  在官方 1.6 的 `outsubfaces` 里定位同一逻辑(行号会不同)应用等价修改。
- **前置实测(AC1/AC4)**:装 1.6 + 打修复后,小用例(cube_vol / 弯段带初始体)实测
  `trifacemarkerlist` 守恒 + `adjtetlist` 两侧 tet 正确,再写 adapter。

## 7. CMake(仿 XQ_ENABLE_ONNX)

```cmake
option(XQ_ENABLE_TETGEN "Build TetGen volume mesh adapter" OFF)
option(XQ_ENABLE_MMG "Build MMG3D volume remesh adapter" OFF)

if(XQ_ENABLE_TETGEN)
    # 官方 1.6 装 Externals:find_path/find_library;或 vendoring 源码 add_library(tetgen STATIC ...) + TETLIBRARY
    find_path(TETGEN_INCLUDE_DIR tetgen.h PATHS ${TETGEN_ROOT}/include)
    find_library(TETGEN_LIBRARY tet PATHS ${TETGEN_ROOT}/lib)   # 库名按官方产物核
    add_library(xq_adapter_tetgen STATIC src/adapters/tetgen/TetGenTetMesher.cpp)
    target_include_directories(xq_adapter_tetgen PRIVATE ${TETGEN_INCLUDE_DIR})
    target_compile_definitions(xq_adapter_tetgen PRIVATE TETLIBRARY XQ_ENABLE_TETGEN)
    target_link_libraries(xq_adapter_tetgen PUBLIC xq_core PRIVATE ${TETGEN_LIBRARY})

    add_executable(test_tetgen_volume_mesh tests/adapters/test_tetgen_volume_mesh.cpp)
    target_link_libraries(test_tetgen_volume_mesh PRIVATE xq_adapter_tetgen xq_services)
    add_test(NAME test_tetgen_volume_mesh COMMAND test_tetgen_volume_mesh)
endif()

if(XQ_ENABLE_MMG)
    find_path(MMG_INCLUDE_DIR mmg/libmmg.h PATHS ${MMG_ROOT}/include)   # mmg-5.3.9/include
    find_library(MMG3D_LIBRARY mmg3d PATHS ${MMG_ROOT}/lib)             # 无 CMake config
    add_library(xq_adapter_mmg STATIC src/adapters/mmg/MmgVolumeRemesher.cpp)
    target_include_directories(xq_adapter_mmg PRIVATE ${MMG_INCLUDE_DIR})
    target_compile_definitions(xq_adapter_mmg PRIVATE XQ_ENABLE_MMG)
    target_link_libraries(xq_adapter_mmg PUBLIC xq_core PRIVATE ${MMG3D_LIBRARY})

    add_executable(test_mmg_volume_mesh tests/adapters/test_mmg_volume_mesh.cpp)
    target_link_libraries(test_mmg_volume_mesh PRIVATE xq_adapter_mmg xq_adapter_tetgen xq_services)
    add_test(NAME test_mmg_volume_mesh COMMAND test_mmg_volume_mesh)
    # 运行需 mmg3d.dll 在 PATH(ctest 环境设 PATH 含 mmg-5.3.9/bin)
endif()
```
- OFF(默认):不编 tetgen/mmg adapter/测试 → 全量与现状一致(44 绿)。
- ON:多编 adapter + 测试;app 层注入 TetGen+Mmg。

## 8. 测试设计

`tests/adapters/test_tetgen_volume_mesh.cpp` / `test_mmg_volume_mesh.cpp`(ON 才编):
- **弯曲段用例**(质心落体外,复用阶段 0 探针的 makeCurvedTube,或真实 M3 loft/cap):
  `TetGenTetMesher` → 断言 ok、所有 tet 有符号体积同号(无翻转)、无域外 tet(质心在表面内)、min 质量 > 0.1。
- **对照**:同输入星形剖分 → 有翻转/域外 tet → 量化差异(证生产路径更优,AC2)。
- **faceId 守恒**:wall=1/inlet=2/outlet=3 → 输出边界面 faceId 集合 == 输入、不混(AC3)。
- **体单元面级连接**:每个 TetBoundaryFace 的 (tetIndex, localFace) → 该 tet 局部面顶点 == tri 顶点(AC4)。
- **MMG remesh**:TetGen 初始体 → remesh,断言 tet 数随 hmax 变化、质量不降、`ier==SUCCESS`、faceId 守恒(AC5)。
- **CHECK 宏式**,副作用先取变量不进 assert(铁律,memory `no-side-effect-in-assert`)。
- **假绿点**:篡改 facetmarker/ref/面级连接 → 对应测试 FAIL;制造翻转 tet → 质量测试 FAIL(AC9)。

## 9. 不做 / 后续

- MMG 各向异性 / 边界层 / level-set;TetGen 边界层(`-Y` 偏移)→ 后续。
- 表面修复(自交/非流形/winding 修复)→ 后续;本任务烂输入返回明确 Status。
- 逐单元对照原 .msh/.vtu → 可顺带统计量对照,不强求。
