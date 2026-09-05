# Research: SimVascular 从闭合三角表面生成体网格(四面体)的完整算法 + faceId 传递 + 外部依赖

- **Query**: 精读本机已 clone 的 SimVascular 源码,搞清「从闭合三角表面生成体网格(四面体)」的完整算法链路、边界面/faceId 怎么传、全部外部依赖
- **Scope**: internal(本机源码,以源码为准)
- **Date**: 2026-06-28

## 源码根

`C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/src/SimVascular/`
- 主算法类:`Code/Source/sv/Mesh/TetGenMeshObject/sv_TetGenMeshObject.cxx`(`cvTetGenMeshObject`)
- TetGen 接口工具:`Code/Source/sv/Mesh/TetGenMeshObject/sv_tetgenmesh_utils.cxx`
- 边界分面:`Code/Source/sv/Model/PolyDataSolidModel/sv_polydatasolid_utils.cxx` + 自定义 filter `sv_vtkGetBoundaryFaces.cxx`
- MMG 集成:`Code/Source/sv/Mesh/MMGMeshUtils/sv_mmg_mesh_utils.cxx`
- TetGen 1.5.1:`Code/ThirdParty/tetgen/simvascular_tetgen/`(`tetgen.h` / `tetgen.cxx`,AGPL v3)

---

## 核心结论(一句话)

**SimVascular 的体网格算法 = TetGen 的 PLC(分段线性复形)约束四面体化。** 把输入闭合三角表面的**每个三角作为 TetGen 的 facet 硬约束**喂进去,`tetrahedralize(tgb, in, out)` 填充内部,输入表面恒为输出体网格的边界 → faceId 天然守恒(用 facetmarker 携带 ModelFaceID 进、用 trifacemarkerlist 取回)。MMG 在 SimVascular 里**只做表面 remesh(MMGS_,非 MMG3D_),不参与体网格**。这正好对应实测裸 vtkDelaunay3D 失败的根因:Delaunay 是凸包、不保凹边界、无 facet 约束;TetGen 的 `-p`(plc)就是补这个缺口。

---

## Q1 完整算法链路:闭合三角表面 → 体网格

入口:`cvTetGenMeshObject::GenerateMesh()` @ `sv_TetGenMeshObject.cxx:1260`。仅看「纯体网格、无 VMTK / 无边界层」主路径(`volumemeshflag && !surfacemeshflag`):

1. **LoadModel(vtkPolyData\*)** @ `:647` — 把输入表面 DeepCopy 进成员 `polydatasolid_`(工作副本)和 `originalpolydata_`(原始备份,后面 faceId 回贴用)。
2. **GetBoundaryFaces(angle)** @ `:691`(调用方在 UI/上层先调,把表面切分成多个 face 并写 `ModelFaceID` 到 CellData,见 Q2)。
3. **表面合法性检查** `TGenUtils_CheckSurfaceMesh` @ utils `:1566`,在 `GenerateMesh` @ `:1319` 调:先 `vtkCleanPolyData` 清理,再用 `vtkConnectivityFilter`(ColorRegionsOn)数连通区域,逐边用 `GetCellEdgeNeighbors` 统计 **自由边(FreeEdges)** 和 **非流形边(NonManifoldEdges)**。任一不为 0 或多区域(非允许多区域)→ 终止(`GenerateMesh:1325-1345`)。**这是「闭合 + 流形」的前置门槛**。
4. **NewMesh()** @ `:747` — 表面 → tetgenio 输入结构(`inmesh_`)的转换:
   - `vtkCleanPolyData` 再清一遍(`:794`)。
   - **`TGenUtils_ConvertSurfaceToTetGen(inmesh_, polydatasolid_)`** @ utils `:105` — 见 Q2,核心:每个三角 = 一个 facet。
   - 可选 `TGenUtils_AddPointSizingFunction`(逐点尺寸度量,`pointmtrlist`,`:165`)。
   - **`TGenUtils_AddFacetMarkers(inmesh_, polydatasolid_, "ModelFaceID")`** @ utils `:206` — 见 Q2,把 ModelFaceID 写进 `facetmarkerlist`。
   - 可选 holes(`AddHoles` @ `:240`,空腔种子点)/ regions(`AddRegions` @ `:262`,多子域种子点 + 体积约束)。
   - `meshloaded_ = 1`。
5. **设置 tetgenbehavior + 调 TetGen** @ `GenerateMesh:1350-1474`:
   - `tgb->plc = 1`(`-p`,PLC 四面体化,**算法核心开关**)。
   - `tgb->neighout = 2`(`-nn`,输出 trifacelist 的相邻 tet 邻接表 `adjtetlist`,回贴 GlobalElementID 用)。
   - 由 `meshoptions_` 映射的可选 flag(逐条见 Q1 表)。
   - `tetrahedralize(tgb, inmesh_, outmesh_)` @ **`:1465`**(try/catch 包,TetGen 抛 int 错误码)。
6. **TetGen 输出 → VTK** `TGenUtils_ConvertToVTK(outmesh_, volumemesh_, surfacemesh_, &numBoundaryRegions_, 1)` @ utils `:372`,在 `GenerateMesh:1507` 调:
   - `outmesh->tetrahedronlist` → `vtkUnstructuredGrid`(VTK_TETRA),加 `ModelRegionID` / `GlobalNodeID` / `GlobalElementID`(utils `:453-484`)。
   - `outmesh->trifacelist` + `trifacemarkerlist` → 边界 `vtkPolyData`,边界面 CellData 写回 `ModelFaceID`(utils `:490-546`)。

### tetgenbehavior flag 映射表(`sv_TetGenMeshObject.cxx`,flag 含义见 `tetgen.h:580-651`)

| 代码 | flag | TetGen 开关 | 含义 |
|---|---|---|---|
| `tgb->plc=1` `:1354` | plc | `-p` | **PLC 四面体化(把输入 facet 当硬约束)** |
| `tgb->neighout=2` `:1355` | neighout | `-nn` | 输出 trifaces 的邻接 tet 表 adjtetlist |
| `tgb->fixedvolume=1; maxvolume=(a³)/(6√2)` `:1363` | fixedvolume | `-a` | 全局最大体积约束(由 maxedgesize 反算) |
| `tgb->quality=1; minratio` `:1368` | quality | `-q` | 半径/边长比质量 |
| `tgb->mindihedral` `:1374` | quality | `-q` | 最小二面角 |
| `tgb->optlevel` `:1379` | — | `-O` | 优化级别 |
| `tgb->nobisect=1` `:1399` | nobisect | `-Y` | **不在输入边界面插点/不分裂边界**(保边界关键) |
| `tgb->regionattrib=1; varvolume=1` `:1427` | — | `-A`/`-a` | 多子域属性 + 变体积 |
| `tgb->nomergefacet/nomergevertex` `:1438` | — | `-M` | 不合并共面 facet/重合点 |

**重点**:核心只有 `-p`(plc)。`-q -a -Y` 等都是质量/尺寸/保边界增强,非必需。最小可跑就是 `plc=1`。

---

## Q2 边界面/faceId 怎么传(XQ 最关心,全链路守恒)

### (a) 表面如何分成多个边界面(生成 ModelFaceID)
`cvTetGenMeshObject::GetBoundaryFaces(angle)` @ `:691` → `PlyDtaUtils_GetBoundaryFaces` @ `sv_polydatasolid_utils.cxx:167`:
- 用自定义 filter **`vtkGetBoundaryFaces`**(基类 vtkFeatureEdges,源 `Code/Source/sv/Model/PolyDataSolidModel/sv_vtkGetBoundaryFaces.cxx`)。
- `SetFeatureAngle(angle)` @ `:176`:相邻三角法线夹角 > angle 视为不同 face 的分界(feature edge)。在角度阈值划出的连通区域内 flood-fill 着色,结果写进 **CellData 数组 `ModelFaceID`**(每个三角一个 int faceId)。
- `*numRegions = GetNumberOfRegions()`。
- 对照 XQ:wall / inlet / outlet 在 SV 里就是 `ModelFaceID` 的不同整数值(cap 面通常被分成独立 face)。**这是纯几何角度阈值分面,不是语义标注**;语义(哪个是 inlet)靠上层按 face 质心/位置再贴。

### (b) ModelFaceID 怎么塞进 tetgenio
两步,都在 `NewMesh()`:
1. **`TGenUtils_ConvertSurfaceToTetGen`** @ utils `:105`:
   - 点:`polydatasolid->GetPoints()` → `inmesh->pointlist`(每点 3 REAL,`:119-126`)。
   - **面:每个三角 = 一个 facet,facet 含 1 个 polygon,polygon 含 3 个顶点索引**(`:128-155`)。即 `numberoffacets = GetNumberOfPolys()`,`f->polygonlist[0].vertexlist = {ptIds 0,1,2}`。**这就是把输入三角作为 PLC facet 硬约束。**
2. **`TGenUtils_AddFacetMarkers(inmesh, pd, "ModelFaceID")`** @ utils `:206`:
   - 读 CellData 的 `ModelFaceID`(int 数组),逐 facet 写 `inmesh->facetmarkerlist[i] = ModelFaceID[i]`(`:223-230`)。
   - 一一对应(facet i ↔ 三角 i ↔ ModelFaceID[i]),**因为 facet 顺序 = polys 顺序,守恒靠这个隐式对齐**。

### (c) TetGen 出来后 faceId 怎么取回
`TGenUtils_ConvertToVTK` @ utils `:372`,`getBoundary=1`:
- TetGen 输出 `outmesh->trifacelist`(边界三角)+ `outmesh->trifacemarkerlist`(每个边界三角的 marker)。
- `:513-523`:`boundaryScalars[i] = outmesh->trifacemarkerlist[i]`,最后 `:541` 命名为 **`ModelFaceID`** 写进 surface 的 CellData。
- **守恒机理**:`-p`(plc)模式下 TetGen **保留输入 facet 作为输出边界 subface**,且把该 facet 的 marker 继承给输出边界三角的 trifacemarker。所以输入 ModelFaceID → 输出 ModelFaceID 端到端守恒(只要边界没被 `-Y` 之外的开关分裂;SV 默认未开 `-Y`,但边界三角只会被细分、marker 继承不变)。
- 注意:输出体网格 `vtkUnstructuredGrid`(volumemesh_)本身只带 `ModelRegionID`(子域)、`GlobalElementID/NodeID`;**faceId 在配套的 surface `vtkPolyData`(surfacemesh_)上**,不是体单元 CellData。XQ 若要体网格直接带 faceId,需自行把边界三角的 ModelFaceID 关联到对应体单元的面。

### (d) 关键判断(明确回答)
**是的**:SimVascular 把输入表面三角作为 TetGen 的 PLC facet 硬约束(`:128-155` ConvertSurfaceToTetGen + `tgb->plc=1` @ `:1354`),输入边界恒为体网格边界,faceId 经 facetmarker→trifacemarker 守恒。这正是裸 Delaunay3D 缺的:Delaunay 求凸包、不认 facet 约束、不保凹边界、无 marker 通道。

---

## Q3 外部依赖全清单(这条体网格链路)

来源:`TetGenMeshObject/CMakeLists.txt`(`target_link_libraries` @ `:53`)+ 各 `#include`。

| 库 | 角色 | 必需性 | 许可 |
|---|---|---|---|
| **TetGen 1.5.1** | **体网格 kernel(PLC 四面体化)** | **不可替代核心** | **AGPL v3**(双授权,商用需付费;`LICENSE` 头确认)。版本见 `Code/ThirdParty/tetgen/.../README:1` |
| **VTK** | vtkPolyData/UnstructuredGrid 数据结构、清理(vtkCleanPolyData)、连通性(vtkConnectivityFilter)、表面提取(vtkDataSetSurfaceFilter)、法线(vtkPolyDataNormals)、定位(vtkCellLocator)、IO(XML reader/writer) | 必需(数据载体) | BSD |
| zlib | `compareAdjacency.xadj.gz` 邻接文件写出(`writeDiffAdj`) | 可选(`SV_USE_ZLIB`,缺了 fallback 到 fopen,`utils:70-82`) | zlib |
| SV 内部库 | `sv_polydatasolid_utils`(GetBoundaryFaces)、`sv_misc_utils`、`sv_vtk_utils`、globals/utils/mesh 基类 | 内部 | MIT-like |
| **VMTK** | 表面 remesh / 边界层网格 | **可选**(`SV_USE_VMTK`,`CMakeLists:45`)。纯体网格路径不需要 | — |
| **MMG** | 表面 remesh(见 Q4) | **可选**(`SV_USE_MMG`,`CMakeLists:49`)。纯体网格不需要 | LGPL |

**没有依赖**:MeshSim(商业)、vtkSV 的体网格(没有)、任何其他四面体器。体网格链路上**除 TetGen + VTK 外无其他第三方刚需**。

---

## Q4 MMG 在 SimVascular 里的角色

`sv_mmg_mesh_utils.cxx` + `.h`:
- **只 include `mmg/mmgs/libmmgs.h`**(`.h:42`,mmgs = **surface** remesh),**全文零 `MMG3D_` / 体网格**(rg 全 `Code/Source` 确认无任何 `MMG3D_` / `libmmg3d` / `mmg3d` 引用)。
- 核心调用:`MMGS_mmgslib(mesh, sol)` @ `.cxx:361`(纯表面重网格)。参数走 `MMGS_Set_iparameter/dparameter`(hmin/hmax/hausd/hgrad/angle/localParameter,`.cxx:109-219`),ridge 用 `MMGS_Set_ridge` @ `:236`。
- 调用点:`GenerateMeshSizingFunction` 路径里 `VMTKUtils_SurfaceRemeshing` / `MMGUtils_SurfaceRemeshing`(`sv_TetGenMeshObject.cxx:1744-1769`),在喂 TetGen **之前**对表面做尺寸驱动的 remesh,提升表面三角质量。

**结论**:SimVascular 用 MMG **只 remesh 表面(MMGS),不做体网格**。这跟我们 findings `[[mmg-cannot-tetrahedralize-from-surface]]` 锁的「两段式:VTK Delaunay3D 填充 → MMG3D 优化」是**不同路线** —— SV 根本不用 MMG3D,体网格全交 TetGen。可借鉴的 MMG 经验:SV 把 ModelFaceID/ridge 在 remesh 前后用 `MMGUtils_PassCellArray`/`PassPointArray`(`.h:52-56`)按最近邻回贴(对应我们 ref/faceId 守恒难题),但那是**表面**层面的,不是体。

---

## Q5 对 XQ 的可移植性结论

1. **SimVascular 体网格核心 = TetGen 的 PLC 四面体化** — 有源码证据:`TGenUtils_ConvertSurfaceToTetGen:128-155`(三角→facet)+ `tgb->plc=1 @ :1354` + `tetrahedralize @ :1465`。
2. **「用 SimVascular 的算法」本质 = 引入 TetGen 做 PLC 填充**,替代被否决的 vtkDelaunay3D。**正确**。Delaunay 失败三宗(凸包/不保凹边界/faceId 无从继承)正是 TetGen `-p` + facetmarker 解决的。
3. **最小可移植链路存在,不必搬整个 cvTetGenMeshObject**。最小四步(全部在 `sv_tetgenmesh_utils.cxx`,可直接照抄):
   - **填输入**:点 → `in.pointlist`;每个表面三角 → `in.facetlist[i]`(1 polygon,3 顶点);faceId → `in.facetmarkerlist[i]`(参 `ConvertSurfaceToTetGen:105` + `AddFacetMarkers:206`)。
   - **跑**:`tetgenbehavior b; b.plc=1; b.neighout=2;`(+ 可选 `b.quality=1;b.minratio; b.fixedvolume=1;b.maxvolume;`)→ `tetrahedralize(&b, &in, &out);`(参 `:1354-1465`)。
   - **读回**:`out.tetrahedronlist` → 四面体单元;`out.trifacelist` + `out.trifacemarkerlist` → 边界面 + faceId(参 `ConvertToVTK:372`)。
   - 这条链路 **C++ 几十行**,只 link TetGen(+ 自己的数据结构,VTK 可选)。`tetgenio` 结构定义 `tetgen.h:110-309`,`tetrahedralize` 声明 `tetgen.h:2261`。
   伪代码:
   ```cpp
   tetgenio in, out;
   in.firstnumber = 0;
   in.numberofpoints = N;  in.pointlist = new REAL[3*N];   // 填顶点
   in.numberoffacets = M;  in.facetlist = new tetgenio::facet[M];
   in.facetmarkerlist = new int[M];
   for (each tri i) {
     facet &f = in.facetlist[i]; f.numberofpolygons=1; f.numberofholes=0; f.holelist=nullptr;
     f.polygonlist = new tetgenio::polygon[1];
     polygon &p = f.polygonlist[0]; p.numberofvertices=3; p.vertexlist=new int[3]{a,b,c};
     in.facetmarkerlist[i] = faceId_of_tri_i;     // ← faceId 进
   }
   tetgenbehavior b; b.plc=1; b.neighout=2; /* +可选 q/a/Y */
   tetrahedralize(&b, &in, &out);                  // 四面体化
   // out.tetrahedronlist[4*k..] = 体单元; out.trifacelist + out.trifacemarkerlist = 边界面+faceId(守恒)
   ```
4. **有没有「不引 TetGen 也能保边界填充」的非 AGPL 线索**:**没有**。`Code/Source` 全仓体网格只有 TetGen 这一条 kernel(rg 确认无自实现四面体器、无其他保约束填充器);vtkSV 模块也无体网格(只 GetBoundaryFaces 等表面工具);MMG 只表面;Delaunay3D 未被 SV 用于体网格。**SimVascular 里保边界体网格 = TetGen,无第二选择。**

---

## Caveats / 关键提醒

- **TetGen 是 AGPL v3**:静态/动态链接进 XQ 会传染 AGPL(网络服务也触发 §13)。商用需向 WIAS 买商业授权(`LICENSE:11-18`)。这是引入 TetGen 的**最大决策点**,需用户确认许可可接受性,否则整条 SV 路线不可用。
- **输入必须先满足闭合 + 流形 + 单连通**(`TGenUtils_CheckSurfaceMesh:1566` 是硬门槛:FreeEdges/NonManifoldEdges 必须为 0)。XQ 的 M3 表面 winding 全局不一致(`[[xq-surface-winding-not-consistent]]`)+ 可能的自由边,喂 TetGen 前需先清理/补洞/定向,否则 TetGen 会 `-p` 失败抛错。
- **faceId 守恒依赖 facet 顺序与 polys 顺序隐式对齐**(`AddFacetMarkers` 无显式 key,靠 i 索引),且依赖 TetGen `-p` 模式保留输入 facet 为边界 subface。若中途对 polydatasolid 做了重排/clean 去点(`vtkCleanPolyData @ NewMesh:794` 会改点索引但不改面顺序),需确保 ModelFaceID 数组同步。
- **体网格 vtkUnstructuredGrid 本身不带 faceId**;faceId 只在配套 surface polydata 上(utils `:541`)。XQ 若需体单元面级 faceId,要额外建「边界三角 ↔ 体单元面」映射(SV 没现成的)。
- SV 的 MMG 路线与我们 findings `[[mmg-cannot-tetrahedralize-from-surface]]` 不冲突也不互证:SV 用 MMGS 表面、我们要的是 MMG3D 体优化,是两个不同 API 家族。SV 不能用来佐证 MMG3D 的体网格用法。
