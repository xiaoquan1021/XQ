# Findings — MMG 体网格能力边界与接入方案

> 任务 `06-28-mmg-volume-mesh-research`(调研,非实现)。逐条回答 PRD §4 Q1~Q7,
> 每条标来源:本地头文件 / 实跑 demo / 官方文档。本机已装 MMG,**优先实证**。

## TL;DR(先给结论)

1. **MMG3D 不能从闭合三角表面初始生成体网格**——它是纯 **remesher**,输入**必须已含四面体**,
   否则直接报 `MISSING DATA` 退出(主会话亲跑实证,见 Q1)。PRD 的核心担忧**成立**。
2. 因此方案必须是 **「初始填充器 → MMG 优化/细化」两段**,不能只引 MMG。
   初始填充器推荐 **VTK `vtkDelaunay3D`**(项目已有 VTK,零新依赖),MMG 作可选二阶段质量提升。
3. **faceId 可经 ref 守恒,但默认会丢**:MMG 默认会重网格化表面,过程会抹平三角 ref。
   要保 faceId 必须 `MMG3D_IPARAM_nosurf=1`(不动表面)或把边界三角设为 required(实证见 Q3)。
4. **MMG 已装** `Externals/install/windows-x64/mmg-5.3.9`(头/lib/dll/exe 齐),但**无 CMake config**,
   接入靠 `find_library` + include 手动(Q6)。**实测二进制版本号是 5.3.8**(目录名 5.3.9,需留意,Q6)。
5. **重要修正**:PRD §6 写「MMG 当前未装」是**过时的**——本机 Externals 已装。

---

## Q1 初始生成:MMG3D 能否从闭合三角表面初始生成填充四面体?

**结论:不能。MMG3D 是纯 remesher,输入必须已含四面体(`ne>0`),否则报错退出。**

**实证(主会话亲跑,最硬证据):**
```
$ mmg3d_O3.exe cube.mesh out.mesh      # cube.mesh = 8 顶点 + 12 三角,无 Tetrahedra 段
  ** MISSING DATA.
 Check that your mesh contains points and tetrahedra.
 Exit program.
```
- 纯表面输入(仅 Vertices + Triangles)→ **直接 `MISSING DATA` 退出,不生成任何输出文件**。
- 对照:带初始体的输入(`cube_vol.mesh`:8 顶点 + 12 三角 + 6 tet)→ 正常输出,tet 6→24(细化)。
  (`.../mmg-demo/cube_vol.mesh` → `cube_vol.o.mesh`)
- 对照:弯曲管段带初始体(`bend_vol.mesh`:106 顶点 + 216 tet)→ 输出 379 tet。

**头文件佐证(`include/mmg/mmg3d/libmmg3d.h:275`):**
```c
int MMG3D_Set_meshSize(MMG5_pMesh mesh, int np, int ne, int nprism, int nt, int nquad, int na);
```
`ne` = 四面体数,是声明输入网格规模的必填项;remesh 流程默认从已有四面体出发。
没有任何 "tetrahedralize from surface" / "fill from boundary" 的 API。

> `MMG3D_mmg3dls`(level-set 离散)也是在**已有体网格**上按隐式函数切分,不是从表面填充。

**来源:** 实跑 `mmg3d_O3.exe`(Externals 本地二进制)+ 本地头 `libmmg3d.h`。

---

## Q2 初始填充:体网格从哪来?

Q1=否 → 必须有初始填充器先把闭合表面填成四面体,再交 MMG。候选对比:

| 选项 | 质量 | 许可 | 复杂度 / 依赖 | 评价 |
|---|---|---|---|---|
| **(a) VTK `vtkDelaunay3D`** | 一般(凸包式 Delaunay,血管凹腔/弯段会在域外生成需 `Alpha`/`BoundingTriangulation` 控制,可能产域外 tet,需后处理裁剪到边界) | BSD(VTK 9.3.0 已在基线) | **零新依赖**,`adapters/vtk` 已存在 | **推荐作初始填充**:项目已有 VTK,无新许可/构建负担;质量靠 MMG 二阶段补 |
| (b) MMG 配套工具 | — | — | **无**——MMG 不含从表面填充的工具(Q1 已证) | 不可行,排除 |
| (c) 现状星形剖分 | 差(弯段翻转) | 自有 | 已实现 | 作 fallback 保留(铁律 4),不作为 MMG 前置 |
| (d) 其他轻量填充器(如 fTetWild/TetGen) | 高 | fTetWild=MPL2 / TetGen=AGPL | 新依赖 + 许可风险(TetGen AGPL 已被 spec 否决) | 不在基线,本任务不引入 |

**推荐:VTK `vtkDelaunay3D` 作初始填充 → MMG 作二阶段优化/细化/保边界。**
理由:VTK 已是基线依赖(零新增),`vtkDelaunay3D` 能从点集/带约束生成四面体;
其质量缺陷正好由 MMG 的 `optim`/重网格弥补。组合方案见末尾「接入方案」。

> ⚠️ 限度:`vtkDelaunay3D` 对**非凸 / 弯曲血管腔**会在凹处生成域外四面体,需用 `Alpha` 参数或
> 输出后按"质心是否在闭合表面内"裁剪。此点**未实证**,留给实现任务验证(列为风险)。

**来源:** Q1 实证(MMG 无填充工具)+ VTK 9.3.0 在基线(`external-libs.md`)+ `vtkDelaunay3D` 为 VTK 公知滤波器(实现任务需实测其在血管腔的表现)。

---

## Q3 faceId 保留:ref 能否承载 faceId 并经 remesh 守恒?

**结论:能承载,但默认 remesh 会丢——必须显式保边界(`nosurf` 或 required 三角)才守恒。**

**头文件依据:** MMG 的三角/四面体 API 都带 `ref` 整数标记字段:
```c
int MMG3D_Set_triangle  (MMG5_pMesh, int v0,int v1,int v2,           int ref, int pos);  // libmmg3d.h:438
int MMG3D_Set_tetrahedron(MMG5_pMesh, int v0,int v1,int v2,int v3,   int ref, int pos);  // :346
```
→ 输入时每个边界三角的 `ref` 可设 = XQ 的 faceId;输出用 `MMG3D_Get_triangle` 取回 ref。

**实证(关键,分两种情形,结果分裂):**

- **守恒情形** `cube_vol`(边界未被重网格 / required):
  - 输入 triangle ref 直方图 `{wall=1:8, top=2:2, bottom=3:2}`
  - 输出(24 tet)triangle ref 直方图 `{1:8, 2:2, 3:2}` — **完全一致,按几何平面分类也守恒 ✓**
- **不守恒情形** `bend_vol`(v1:输入边界三角与体网格的实际边界面**不精确吻合**):
  - 输入 `{wall=1:192, inlet=2:8, outlet=3:8}`
  - 默认输出 `{1:210}` — **inlet/outlet 标记全被抹平为 1 ✗**
  - `-nosurf` 输出 `{1:204}` — 同样丢
- **改进后守恒** `bend_vol2`(v2:边界三角**直接从 tet 边界面导出**——被恰好一个 tet 使用的面):
  - 输入 `{wall=1:192, inlet=2:6, outlet=3:6}` → 输出 `{1:192, 2:10, 3:8}` — **inlet/outlet 标记保住 ✓**
  - v2 脚本注释原文:把边界三角"DIRECTLY from the tet boundary faces ... so the input triangles
    are guaranteed to coincide with mmg3d's reconstructed boundary. This is the FIX for the ref-loss seen in v1."

**根因(核实生成脚本得到,接入方案硬约束):**
ref 丢失的真因**不是**"没声明 required",而是 **v1 的输入边界三角与体网格实际边界面对不上号**——
MMG 内部按体网格重建边界面,只有当**输入 Set_triangle 的三角与 tet 的实际边界面(被一个 tet 独占的面)精确一致**时,
ref 才能落到正确的边界面上守恒。接入实现**必须**:
1. **边界三角由初始体网格的边界面导出**(而非沿用原表面三角),保证与 MMG 重建边界一一对应 → ref 守恒(**关键**)。
2. 叠加 **`MMG3D_IPARAM_nosurf=1`**(`libmmgtypes.h:447` 有 `nosurf` 字段;`libmmg3d.h:82` 枚举):不改表面,
   ref 直接守恒、faceId 映射最简单(**推荐**:血管壁表面来自 M3,不需 MMG 再改表面)。
> ⚠️ 即便 `nosurf=1`,若输入边界三角与 tet 边界面不吻合(v1 的 -nosurf 仍丢),ref 也会丢——
> 所以"边界三角从 tet 边界面导出"是更根本的前提,nosurf 是叠加保险。

**来源:** 本地头 `libmmg3d.h` / `libmmgtypes.h` + 实跑 `cube_vol` / `bend_vol` / `bend_vol2` 三组对照
+ **核实生成脚本 `gen_bend_vol.py` vs `gen_bend_vol2.py` 的 diff**(`.../mmg-demo/`)。

---

## Q4 API 形态:C API 够不够包成「XQ surface → XQ tet mesh」一个函数?

**结论:够。一套标准 `MMG3D_*` C API,可包成一个吃 XQ 类型、内部不暴露 MMG 类型的 adapter 函数。**

**实证关键 API(均在本地 `libmmg3d.h`,`grep` 确认存在):**
`MMG3D_Init_mesh` / `MMG3D_Set_meshSize` / `MMG3D_Set_vertex` / `MMG3D_Set_triangle` /
`MMG3D_Set_tetrahedron` / `MMG3D_Set_iparameter` / `MMG3D_mmg3dlib`(remesh)/ `MMG3D_mmg3dls`(level-set)/
`MMG3D_Get_meshSize` / `MMG3D_Get_tetrahedron` / `MMG3D_Get_triangle`。

**调用序列骨架(伪代码,adapter `.cpp` 私有实现,MMG 类型不出 adapter):**
```c
MMG5_pMesh mesh = NULL; MMG5_pSol sol = NULL;
MMG3D_Init_mesh(MMG5_ARG_start, MMG5_ARG_ppMesh,&mesh, MMG5_ARG_ppMet,&sol, MMG5_ARG_end);

// 注意 ne(tet 数)必须 > 0 —— 初始体由 Q2 的填充器(VTK Delaunay3D)先生成
MMG3D_Set_meshSize(mesh, np, ne, 0, nt, 0, 0);
for (i: points)    MMG3D_Set_vertex(mesh, x,y,z, ref, i+1);
for (i: tets)      MMG3D_Set_tetrahedron(mesh, a,b,c,d, regionRef, i+1);
for (i: bdryTris)  MMG3D_Set_triangle(mesh, a,b,c, faceId/*ref*/, i+1);  // ref = XQ faceId

MMG3D_Set_iparameter(mesh, sol, MMG3D_IPARAM_nosurf, 1);   // 保边界 → faceId 守恒(Q3)
// 可选:MMG3D_Set_dparameter(..., MMG3D_DPARAM_hmax/hmin, ...) 控制目标边长

int ier = MMG3D_mmg3dlib(mesh, sol);     // remesh/优化;返回 MMG5_SUCCESS / LOWFAILURE / STRONGFAILURE

MMG3D_Get_meshSize(mesh, &np2,&ne2,..,&nt2,..);
for (...) MMG3D_Get_tetrahedron(mesh, ...);  // → XQTetVolumeMeshHandle::addTet
for (...) MMG3D_Get_triangle(mesh, ..&ref..);// ref → 边界面 faceId
MMG3D_Free_all(MMG5_ARG_start, ...);
```
公开签名仿作废的 tetgen design(方案 A):
```cpp
struct MmgParams { double hmax=0, hmin=0; bool noSurf=true; /*...*/ };
struct MmgResult { bool ok; std::string message; std::shared_ptr<XQTetVolumeMeshHandle> mesh; /* + 边界面 faceId 映射 */ };
MmgResult mmgRemeshVolume(const XQTetVolumeMeshHandle& initial, /*边界面 faceId*/, const MmgParams&);
```
→ 输入/输出全 XQ 类型,`libmmg3d.h` 只进 `adapters/mmg/*.cpp`(铁律 1)。

**来源:** 本地头 `libmmg3d.h`(API 名 + `Set_meshSize`/`Set_triangle`/`Set_tetrahedron` 签名亲验)。

---

## Q5 输入要求:MMG 对输入表面的要求 vs M3 winding 不一致 / 自交

**结论(部分实证 + 部分文档推断,限度已标):**

- MMG remesh 的输入是**体网格**(不是裸表面),所以"表面 winding"问题转化为"**初始填充器(VTK Delaunay3D)**对输入表面的要求"——
  即 winding/自交问题主要落在 **Q2 的填充阶段**,不在 MMG 阶段。
- MMG 阶段对输入体网格的要求:tet 不能退化/翻转(否则 remesh 失败或质量崩)。
  实证:`cube_vol`(干净体)→ 成功;弯段干净体 → 成功。**未实证**喂入含翻转 tet 的体网格会如何(留风险)。
- **M3 winding 全局不一致**(memory `xq-surface-winding-not-consistent`:闭合 ≠ 法线一致):
  - 对 VTK Delaunay3D:Delaunay 本身不依赖三角朝向(只用点集 + 约束),winding 不一致影响较小;
    但**边界面归属 / faceId 贴回**依赖三角身份,需保留输入三角→faceId 映射。
  - 建议预处理:按现状 `VolumeMeshService` 的 `isClosedManifold` 校验(已有,每边恰 2 三角)把关;
    自交检测**当前无**,M3/M4 产物假设无自交,若有则填充阶段会失败 → 返回明确 Status(不静默产烂网格)。
- **自交**:MMG 与 Delaunay 对自交表面都会失败。**未实证**,依赖 M3 输出质量;列为风险/前置校验。

**来源:** 部分实证(cube/bend 干净体成功)+ memory winding 结论 + Delaunay 性质(文档推断,**自交/翻转输入未实证**)。

---

## Q6 装进 Externals:已装,链接方式与坑

**结论:已装齐,可用;无 CMake config,靠 find_library + include;有版本号/dll 运行时坑。**

**已装内容**(`Externals/install/windows-x64/mmg-5.3.9/`):
- `include/mmg/{mmg3d,mmgs,mmg2d}/*.h` + `include/mmg/libmmg.h` — 头齐
- `lib/mmg3d.lib`(+ mmg.lib/mmgs.lib/mmg2d.lib) — 导入/静态库
- `bin/mmg3d.dll` + `bin/mmg3d_O3.exe` + 一票 MSVC 运行时 dll(msvcp140 等)

**链接方式:**
- **无 `*.cmake` / `MMGConfig.cmake` / `*.pc`**(find 确认为空)→ 不能 `find_package(mmg CONFIG)`。
  接入用 `find_library(MMG3D_LIB mmg3d PATHS .../mmg-5.3.9/lib)` + `find_path(... include/mmg)` 手动。
- 仿 `XQ_ENABLE_ONNX`:`option(XQ_ENABLE_MMG OFF)`,ON 时编 `adapters/mmg` + link mmg3d。

**坑:**
1. **版本号不符**:目录名 `mmg-5.3.9`,但 `mmg3d_O3.exe` 自报 **`Release 5.3.8 (Apr. 10, 2017)`**。
   头文件 API 以本地为准即可,但写 lock/manifest 时按二进制实际版本核对(别盲信目录名)。
2. **dll 运行时**:exe/dll 依赖 `mmg3d.dll` + MSVC runtime,跑测试/app 需把 `bin/` 加 PATH 或拷 dll 同目录
   (与现有 VTK/Qt dll 部署一致)。
3. **许可证文件 install 里未带**(find LICENSE/COPYING 为空)→ 见 Q-合规。

**来源:** `ls`/`find` 本地 install 目录 + 实跑 exe 自报版本。

---

## Q7 综合推荐

**推荐:`VTK Delaunay3D(初始填充) → MMG3D remesh(nosurf 保边界 + 优化/细化)` 两段方案。**

依据(全部来自上面各 Q 的实证):
- MMG **不能**单独从表面建体(Q1 铁证)→ 必须配初始填充器。
- 初始填充器选 VTK Delaunay3D:**零新依赖**(VTK 已在基线)、许可干净(BSD)(Q2)。
- MMG 的价值在**二阶段**:把 Delaunay 的粗糙/域外 tet 优化、按目标边长细化、`nosurf` 保 faceId(Q3/Q4)。
- 相比"只用 VTK Delaunay3D":MMG 二阶段能显著提质量(实证:cube 6→24、bend 216→379 且细化均匀),
  对弯曲血管段(星形剖分会翻转的场景)更有意义。

**何时可只用 VTK 不上 MMG:** 若 Delaunay3D 输出质量在血管腔已达标(需实测),MMG 可作可选增强。
建议实现任务**先做 VTK 填充打通主链路**,MMG adapter 作 `XQ_ENABLE_MMG` 可选二阶段,分步落地、风险可控。

---

## 接入方案(AC2:可落地数据流)

```
XQTriangleSurfaceGeometryHandle (闭合表面 + 每三角 faceId)   [来自 M3/M4,LPS/mm]
        │  (依赖注入,service 不 link kernel —— 仿 tetgen design 方案 A / M6 AiService Backend)
        ▼
┌─────────────────────────────────────────────────────────────┐
│ adapters/vtk : surfaceToInitialTetMesh()                     │  ← vtkDelaunay3D
│   闭合表面 → 初始 XQTetVolumeMeshHandle(+边界三角 faceId)    │     (VTK 类型不出 adapter)
│   预校验:isClosedManifold(已有);失败→明确 Status          │
└─────────────────────────────────────────────────────────────┘
        │  XQTetVolumeMeshHandle (含初始体 + 边界面 faceId)
        ▼ (可选,XQ_ENABLE_MMG=ON)
┌─────────────────────────────────────────────────────────────┐
│ adapters/mmg : mmgRemeshVolume()                            │  ← MMG3D_mmg3dlib
│   ref = faceId 喂入;IPARAM_nosurf=1 保边界(Q3)            │     (MMG 类型不出 adapter)
│   → 优化/细化后的 XQTetVolumeMeshHandle,ref→faceId 取回    │
└─────────────────────────────────────────────────────────────┘
        │  XQTetVolumeMeshHandle (高质量体 + 边界面 faceId 守恒)
        ▼
VolumeMeshService(纯 C++,不 link kernel)
   - 走注入的 ITetMesher 接口(Star / VtkDelaunay / VtkDelaunay+Mmg)
   - 默认无注入 → 现状星形剖分 fallback(铁律 4 不删)
   - transferBoundaryFaces 语义不变;faceId 为 key
        ▼
XQMesh → AddNodeWithSourceRelationCommand → scene(命令栈,undo 可回初始态)
```

**依赖边界(铁律 1/2):**
- `xq_services` 仍**只 link xq_core**,定义 `ITetMesher` 纯虚接口(吃/吐 XQ 类型)。
- `adapters/vtk`(已有)加初始填充;`adapters/mmg`(新,可选)加 remesh。两者 PRIVATE link VTK/MMG。
- 装配在 controller/app 层(谁 link adapter 谁组装),service 保无头单测能力。
- 审计:`rg "#include.*mmg" src/core src/services` 必须空。

**faceId 全链路:** 表面三角 faceId → Delaunay 输出边界三角 faceId → MMG triangle `ref`(nosurf 守恒)
→ `MMG3D_Get_triangle` ref → 回填 `MeshBoundaryFace.faceId`。kind/capId 从 `faces` 按 faceId 查(语义同现状)。

---

## 合规(AC5:许可与链接)

- **MMG 许可:LGPL**(spec `external-libs.md` 与官网 mmgtools.org 均记 MMG = LGPL v3)。
  install 目录**未带 LICENSE 文件**,实现任务需从 MMG 源/官网取 LGPL 文本入库存档。
- **链接方式与 LGPL 兼容性:** LGPL 允许闭源程序**动态链接**;本机有 `mmg3d.dll`(动态)。
  - 推荐**动态链接**(dll),最稳妥满足 LGPL(用户可替换 mmg dll)。
  - 若静态链接 `mmg3d.lib`,LGPL 要求提供可重新链接的目标文件/换库途径——更麻烦。
  - **结论:用 dll 动态链接**,随产品分发 mmg dll + LGPL 文本。VTK 是 BSD,无此约束。
- ⚠️ 限度:LGPL 版本(v2.1 还是 v3)与"动态链接是否足够"的最终合规判断**未做法务核**;
  按常规理解 LGPL 动态链接对闭源分发安全,正式分发前建议法务确认。

---

## 实证产物(可复现)

工作目录 `C:/Users/OCEAN/Desktop/XIAOQUAN/.trellis/workspace/ocean/mmg-demo/`:
- `cube.mesh` — 闭合立方体纯表面(无 tet);喂 mmg3d → `MISSING DATA` 退出(Q1 铁证)
- `cube_vol.mesh` / `cube_vol.o.mesh` — 带初始体输入/输出,6→24 tet,ref 守恒(Q3 守恒侧)
- `bend_vol.mesh` / `bend_vol.o.mesh` / `bend_vol.ns.mesh` — 弯段;默认模式 ref 被抹平(Q3 丢失侧)
- `bend_vol2.mesh` / `bend_vol2.o.mesh` — 改进输入,ref 守恒(Q3 守恒侧)
- `check_refs.py` — 按几何平面校验 ref 守恒的脚本
- 复跑命令(需先把 `Externals/.../mmg-5.3.9/bin` 加 PATH):`mmg3d_O3.exe <in>.mesh <out>.mesh`

> 注:版本目录名 5.3.9 但二进制自报 5.3.8;头文件 API 以本地为准。
