# PRD — 引入生产级体网格:TetGen PLC 约束四面体化 + MMG3D remesh

> 任务 id:`06-28-volume-mesh-kernel` | 优先级 P1 | package XQ
> **依据(全部已验证):**
> - `archive/2026-06/06-28-mmg-volume-mesh-research/findings.md`(MMG 能力边界,实证)。
> - `research/simvascular-volume-mesh.md`(SimVascular 体网格算法精读,源码逐行证据)。
> - 阶段 0 实测 `.trellis/workspace/ocean/delaunay-probe/RESULTS.md`(vtkDelaunay3D 在弯段否决,实证)。

## 1. 背景与问题

`VolumeMeshService::buildVolumeMesh` 现用**质心星形剖分**:仅星形域正确,弯曲血管段(质心落体外)
产翻转/退化四面体(实测弯段 128/224 = 57% tet 落域外),密度不可控、无内部加点。这不是生产级网格。

**关键调研结论(本任务方案的实证基础):**

1. **裸 `vtkDelaunay3D` 不可用于血管腔(阶段 0 实测否决)。** Delaunay3D 是**凸包**四面体化、
   无 3D PLC 约束能力(API 只吃点集),弯段上 20% 域外 tet、**75% 输入三角对不回边界面**、
   faceId 无从继承。质心裁剪/Alpha 都救不了(根因是不保凹边界,非参数可调)。
   见 `delaunay-probe/RESULTS.md`。
2. **MMG3D 不能从表面初始建体(纯 remesher,findings 实证)。** 只能优化已有体网格。
3. **SimVascular 体网格 = TetGen 的 PLC 约束四面体化(源码实证)。** 把输入闭合表面的每个三角
   作为 TetGen 的 **facet 硬约束**(`-p`),`tetrahedralize` 填充内部,输入表面恒为输出边界 →
   faceId 经 facetmarker→trifacemarker 守恒。这正补 Delaunay 的三宗缺陷。SimVascular / VMTK /
   FEBio / 3D Slicer 的开源档体网格都收敛到 TetGen,因为血管腔是凹域必须 PLC 约束。
   见 `research/simvascular-volume-mesh.md`。

**方案(用户已定):采用 SimVascular 同款算法 —— 引入 TetGen 做 PLC 填充,MMG3D 做二段 remesh。**

## 2. 范围

### In scope

1. **引入 TetGen(官方 1.6 + SV `outsubfaces` 修复)**:装进 `Externals/install`,`find_library` 链接。
   打上 SimVascular 对 `tetgenmesh::outsubfaces` 的边界面邻接修复(`adjtetlist` 两侧 tet 独立取,
   见 design §6),小用例实测 `trifacemarkerlist`/`adjtetlist` 守恒后再接入。
2. **`adapters/tetgen`(新,可选 `XQ_ENABLE_TETGEN`)**:`TetGenTetMesher` 实现 `ITetMesher`。
   `.cpp` 内部:XQ 表面三角 → `tetgenio` facet + facetmarker(=faceId)→ `tetrahedralize("pq...a...nn")`
   → `tetrahedronlist` + `trifacelist`/`trifacemarkerlist`/`adjtetlist` → `XQTetVolumeMeshHandle`
   + 边界面 faceId + **体单元面级连接**。`tetgen.h` 不出 adapter。
3. **`adapters/mmg`(新,可选 `XQ_ENABLE_MMG`)**:`MmgVolumeRemesher` 吃 TetGen 初始体 → MMG3D remesh
   (`MMG3D_mmg3dlib`,`nosurf=1` 保 faceId,findings 实证)→ `XQTetVolumeMeshHandle`。`libmmg3d.h` 不出 adapter。
   组合 mesher `TetGenThenMmg`(实现 `ITetMesher`:先 TetGen 再 MMG3D)。
4. **`VolumeMeshService` 经 `ITetMesher` 接口注入 kernel**:纯虚接口(吃/吐 XQ 类型,零外部),
   定义在 services。默认无注入 → 现状星形剖分 **fallback 不删**。注入 TetGen / TetGen+Mmg → 走生产路径。
   装配在 controller/app 层。
5. **体单元面级 faceId(为大规模 CFD 优化)**:`MeshBoundaryFace` 填实「边界面 → (tetIndex, localFace)」
   连接,用 TetGen `-nn` 的 `adjtetlist` O(1) 填(零几何查询)。区别于现状/SV 的「faceId 只在独立
   surface 面片集、求解时再 vtkCellLocator 几何重匹配」——那在大规模网格是性能坑。见 design §4。
6. **CMake**:`option(XQ_ENABLE_TETGEN OFF)` + `option(XQ_ENABLE_MMG OFF)` 仿 `XQ_ENABLE_ONNX`。
   ON 才编对应 adapter + 注册测试。AGPL/LGPL 文本入库。
7. **测试**:见 AC,弯段质量优于星形 + faceId 守恒 + 面级连接正确 + 反向依赖零渗透 + OFF 路径不回归。

### Out of scope(记后续)

- MMG 各向异性 / 边界层 / level-set(`mmg3dls`)、TetGen 边界层(`-Y` + 偏移)—— 本任务先打通各向同性。
- 表面修复(自交/非流形/winding 修复)—— TetGen 要求闭合+流形+单连通(硬门槛);
  输入假设 M3/M4 闭合 2-流形;烂输入返回明确 Status 不静默产烂网格。**M3 winding 全局不一致**需在
  喂 TetGen 前定向/清理(见 design §3 预处理 + 风险)。
- 与 `0007/Meshes/*.{msh,vtu}` 逐单元对照(另一条债)。

## 3. 铁律约束(违反即 BLOCKER)

1. **kernel 只进 adapter 私有实现**:`tetgen.h` 只在 `adapters/tetgen/*.cpp`;`libmmg3d.h` 只在
   `adapters/mmg/*.cpp`。`rg "#include.*tetgen" src/core src/services` 与 `rg "#include.*mmg" src/core src/services`
   必须空。公开 API 只暴露 XQ 类型。
2. **service 不 link kernel**:`xq_services` 仍只 link `xq_core`,经 `ITetMesher` 注入。无头单测能力不破。
3. **faceId 全链路**:表面三角 faceId → `facetmarkerlist`(=faceId)→ TetGen `-p` 保 facet →
   `trifacemarkerlist` → 边界面 faceId;`adjtetlist` → 体单元面级连接。kind/capId 按 faceId 从 `faces` 查(同现状)。
4. **降级不删**:星形剖分作默认 fallback(无注入时)保留,原 M4 测试不破坏。
5. **坐标系/单位不变**:LPS/mm,kernel 不引入单位转换。
6. **许可合规**:TetGen = **AGPL v3**,MMG = LGPL。XQ 接受 AGPL/GPL 开源(用户已确认)。
   AGPL + LGPL 文本入库;链接方式记 design/notes。

## 4. 验收标准(逐条可证伪)

- [ ] **AC1 TetGen 落地(XQ_ENABLE_TETGEN=ON)**:官方 1.6 + `outsubfaces` 修复装进 Externals,
      `adapters/tetgen` 把闭合表面转 `XQTetVolumeMeshHandle`,公开签名只含 XQ 类型,`tetgen.h` 仅在 .cpp。
      弯段输入产出**无翻转 tet、无域外 tet**(PLC 保边界)。
- [ ] **AC2 质量优于星形**:弯曲血管段(质心落体外用例)→ TetGen `-q`/`-a` 产出 min 归一化质量 > 阈值
      (如 0.1)、无翻转;同输入星形剖分产翻转/域外 → 对照可见差异。
- [ ] **AC3 faceId 全链路守恒**:输入 wall=1/inlet=2/outlet=3 → 输出边界面 faceId 集合一致、不混;
      篡改 facetmarker 映射 → 测试 FAIL。(SV 实证 PLC + facetmarker 守恒。)
- [ ] **AC4 体单元面级连接正确**:每个边界面带 `(tetIndex, localFace)`,该 tet 的该局部面三个顶点
      == 边界面三角顶点(几何校验);用 `adjtetlist` 填、零 vtkCellLocator。篡改连接 → 测试 FAIL。
- [ ] **AC5 MMG3D remesh(XQ_ENABLE_MMG=ON)**:`adapters/mmg` remesh TetGen 初始体,`nosurf=1`;
      `libmmg3d.h` 仅在 .cpp;输出 tet 数随 hmax 变化(细化生效);**faceId 守恒**(ref 集合一致)。
- [ ] **AC6 ITetMesher 注入**:service 默认走星形 fallback;注入 TetGen(+Mmg)走生产路径,行为可证。
- [ ] **AC7 反向依赖零渗透**:`rg "#include.*tetgen" src/core src/services` 空;
      `rg "#include.*mmg" src/core src/services` 空。
- [ ] **AC8 CMake option**:`XQ_ENABLE_TETGEN=OFF` 且 `XQ_ENABLE_MMG=OFF`(默认)全量构建+ctest 与现状一致(44 绿);
      `=ON` 多编 adapter + 测试,全绿。
- [ ] **AC9 假绿抽查**:篡改 facetmarker/ref/面级连接 → 对应测试 Release FAIL → 恢复 → PASS。
- [ ] **AC10 主线不回归**:OFF 路径原 44 测试全绿;星形 fallback 不破坏。
- [ ] **AC11 合规**:AGPL v3 + LGPL 文本入库,链接方式(TetGen 静态/动态、MMG dll 动态)记录在 design/notes。

## 5. 已知现状(实测,供执行者接手)

- `VolumeMeshService`(`src/services/meshing/VolumeMeshService.{h,cpp}`):星形剖分;`Params` 注释预留
  "kernel selection";`isClosedManifold`(每边恰 2 三角)已有;`transferBoundaryFaces` 搬 faceId/name/kind/capId;
  `Status` Ok/NotClosed/InvalidSurface/NullScene。
- `XQTetVolumeMeshHandle`:`addPoint`/`addTet(a,b,c,d)`/`points()`/`tets()`。
- `XQTriangleSurfaceGeometryHandle`:`points()`/`triangles()`/`triangleFaceId(i)`/`addTriangle(..,faceId)`。
- `XQMesh`:`MeshBoundaryFace { faceId; name; kind; capId; cellIds }` —— `cellIds` 字段已在但现状未认真填,
  本任务用它承载体单元面级连接(见 design §4)。`boundaryFaceById` 已有。
- `adapters/` 现有 `vtk/`、`onnx/`(`XQ_ENABLE_ONNX` 范式);**无 tetgen/、无 mmg/**。
- TetGen:**Externals 未装**(单文件库 `tetgen.cxx`+`predicates.cxx`+`tetgen.h`,编库需 `-DTETLIBRARY`)。
  SV 自带 1.5.1+修复在 `Externals/src/SimVascular/Code/ThirdParty/tetgen/simvascular_tetgen/`(参考修复内容)。
- MMG 已装 `Externals/install/windows-x64/mmg-5.3.9`(头/lib/dll;无 CMake config;二进制实 5.3.8;LGPL)。
- VTK 9.3.0 已在基线。

## 6. 风险 / 前置

- **TetGen 1.6 边界面 marker 导出**:官方 1.6 是否含 SV 1.5.1 的 `outsubfaces` 邻接修复未知。
  **前置:装 1.6 后打修复 + 小用例实测 `trifacemarkerlist`/`adjtetlist` 守恒**,再接入(AC1/AC4 依赖)。
- **TetGen 输入门槛**:要求闭合 + 流形 + 单连通(SV `CheckSurfaceMesh` 硬门槛:FreeEdges/NonManifoldEdges=0)。
  **M3 winding 全局不一致** + 可能自由边 → 喂 TetGen 前需清理/定向(VTK 清理或自实现);烂输入返回明确 Status。
- **MMG 输入需干净体**:喂含翻转 tet 的初始体给 MMG 未实证;TetGen 输出先保证无翻转再交 MMG。
- **AGPL 法务**:不商用 + 开源场景 AGPL 合规(XQ 接受 AGPL/GPL 开源);若日后商用/闭源需向 WIAS 购商业授权。
- **版本号**:MMG 目录 5.3.9 / 二进制 5.3.8,写 lock 按实际核;TetGen 用官方 1.6。
