# Implement — 执行计划:两段式体网格 kernel(TetGen PLC + MMG3D)

> 配合 `prd.md` / `design.md`。分阶段执行,每阶段可独立验证、可回退。
> 阶段 0 实测(Delaunay 否决)已完成;本计划从「TetGen 落地实测」开始。

## 阶段 0:前置实测 ✓ 已完成

- [x] **vtkDelaunay3D 弯段实测**:已否决(凸包、不保凹边界、20% 域外 tet、75% 输入三角对不回、faceId 无从继承)。
      结论 `.trellis/workspace/ocean/delaunay-probe/RESULTS.md`,经主会话独立核实。
      → 填充段改为 TetGen PLC(SV 同款),见 design §0。

## 阶段 1:TetGen 落地 + marker/邻接守恒实测(降风险,先于写 adapter)

- [ ] **取官方 TetGen 1.6**(tetgen.org),装进 `Externals/install/windows-x64/tetgen-1.6.0/`(或 vendoring 源码进 XQ)。
      编库需 `-DTETLIBRARY`(`tetgen.cxx` + `predicates.cxx`)。库名/产物按官方实际核。
- [ ] **打 SV `outsubfaces` 修复**(design §6):在 1.6 的 `tetgenmesh::outsubfaces(tetgenio* out)` 里
      定位「取边界面两侧相邻 tet」逻辑,应用等价修改(`abuttingtet2 = fsym(abuttingtet)`,两侧独立 `ishulltet`+`elemindex`)。
- [ ] **小用例实测**(临时探针,放 workspace,不入库):cube_vol / 弯段带初始体闭合表面 →
      `tetrahedralize("pq...nn", in, out)` → 验 ① `trifacemarkerlist` 守恒(输入 facetmarker == 输出 trifacemarker 集合)
      ② `adjtetlist` 两侧 tet 索引正确(每边界面相邻 tet 的该局部面顶点 == 边界三角顶点)。
      —— **这是接入前置**:守恒不成立则修复没打对/版本不兼容,先解决再写 adapter。
- [ ] **验证**:实测结论(marker 守恒 + 邻接正确)主会话独立核实,写进 workspace 结论文件。

## 阶段 2:ITetMesher 接口 + service 注入点(无外部依赖,可先全绿)

- [ ] `src/services/meshing/ITetMesher.h`:`ITetMesher` / `TetMeshParams` / `TetBoundaryFace` / `TetMeshResult`
      (纯 XQ 类型,见 design §1)。
- [ ] `VolumeMeshService::buildVolumeMesh` 增可选 `ITetMesher* mesher = nullptr` 参数:
      - `nullptr` → 现状星形剖分(原逻辑不动,fallback)。
      - 非空 → 调 `tetrahedralize`,result 的 faceId + (tetIndex,localFace) 回填 `MeshBoundaryFace`
        (复用现有 `byFaceId` 逻辑;cellIds 填实,localFace 按 design §4 决定是否扩字段)。
- [ ] `buildVolumeMeshCommand` 透传 mesher。
- [ ] **验证**:此阶段不引入任何 kernel;全量 ctest 仍 44 绿(默认 nullptr=星形,行为不变)。AC10。

## 阶段 3:TetGen adapter(adapters/tetgen,XQ_ENABLE_TETGEN)

- [ ] `src/adapters/tetgen/TetGenTetMesher.{h,cpp}` 实现 ITetMesher(design §2a):
      - `.cpp`:预处理校验(§3)→ XQ 表面 → `tetgenio`(facet + facetmarker=faceId)→
        `tetrahedralize("pq..a..nn")` → `tetrahedronlist` + `trifacelist`/`trifacemarkerlist`/`adjtetlist`
        → `XQTetVolumeMeshHandle` + `TetBoundaryFace`(faceId + tetIndex + localFace)。
      - 公开头零 tetgen 类型(`tetgen.h` 仅 .cpp)。
- [ ] CMake:`option(XQ_ENABLE_TETGEN OFF)`,find/vendoring + `-DTETLIBRARY`(design §7)。
      ON 才编 + 注册 `test_tetgen_volume_mesh`。
- [ ] `tests/adapters/test_tetgen_volume_mesh.cpp`(design §8):
      弯段 → 无翻转/无域外 tet/min 质量>0.1(AC1/AC2);对照星形有翻转(AC2);
      faceId 守恒(AC3);体单元面级连接正确(AC4)。
- [ ] **验证**:AC1~AC4。`rg "#include.*tetgen" src/core src/services` 空(AC7)。

## 阶段 4:MMG3D remesh(adapters/mmg,XQ_ENABLE_MMG)

- [ ] `src/adapters/mmg/MmgVolumeRemesher.{h,cpp}`(design §2b):吃 TetGen 初始体+faceId → remesh → 体网格。
      - `.cpp`:Init_mesh → Set_meshSize(ne>0) → Set_vertex/triangle(ref=faceId)/tetrahedron
        → `IPARAM_nosurf=1` → mmg3dlib → Get_*(ref→faceId) → Free_all。
      - TetGen 的 `trifacelist` 天然是体网格边界面(被一个 tet 独占),直接喂 MMG(免 findings 的导出步骤)。
      - 公开头零 mmg 类型(`libmmg3d.h` 仅 .cpp)。
- [ ] 组合 mesher `TetGenThenMmg`(实现 ITetMesher:先 TetGen 再 remesh,design §2c);
      MMG 段后体单元面级连接重建一次(从 tet 提边界面匹配回 ref 边界三角)。
- [ ] CMake:`option(XQ_ENABLE_MMG OFF)`,`find_library(mmg3d)` + include `mmg-5.3.9/include`(无 CMake config)。
      ON 才编 + 注册 `test_mmg_volume_mesh`;ctest 环境 PATH 含 `mmg-5.3.9/bin`(dll)。
- [ ] `tests/adapters/test_mmg_volume_mesh.cpp`:remesh tet 数随 hmax 变、质量不降、ier==SUCCESS;
      **faceId 守恒**(wall/inlet/outlet 集合一致,nosurf,AC5)。
- [ ] **验证**:AC5。`rg "#include.*mmg" src/core src/services` 空(AC7)。

## 阶段 5:装配 + 合规 + 全量验收  ✓ 已完成

> 阶段 1~4 已完成并 commit(c7f9e18 / e4e5678 / c984a30 / 38e80e3,源码实证)。
> 阶段5 三处装配 + 合规 + 验收已落地并主会话独立全新 build 验收通过(OFF/OFF 45 绿、ON/ON 47 绿)。

### 5.1 MeshingController 透传 mesher(`src/ui/controllers/MeshingController.{h,cpp}`)

- [x] `.h`:前向声明 `class ITetMesher;`;构造签名 `MeshingController(XQScene*, XQCommandStack*, ITetMesher* mesher=nullptr)`
      (末参默认 nullptr → 旧两参调用不破);私有加成员 `ITetMesher* mesher_;`(借用,非 owning)。
- [x] `.cpp`:构造存 `mesher_(mesher)`;`buildVolumeMesh` 调 `buildVolumeMeshCommand(...)` 末参透传 `mesher_`。
      .cpp 只传指针不解引用,前向声明够,不加 include。

### 5.2 app 层装配(`src/app/XQMainWindow.{h,cpp}`)—— design §5 方案A

- [x] `.h`:前向声明 `class ITetMesher;`;成员区在 `meshingController_` **之前**加
      `std::unique_ptr<ITetMesher> volumeMeshKernel_;`(析构逆序→controller 先析构,kernel 后,避免悬空)。
- [x] `.cpp`:
      - include 区:`#include "services/meshing/ITetMesher.h"`(services 层零外部,OFF 也能 include,
        `~XQMainWindow()=default` 需 unique_ptr<ITetMesher> 的完整类型);
        守卫 `#if defined(XQ_ENABLE_MMG)` → `#include "adapters/mmg/TetGenThenMmg.h"`;
        `#elif defined(XQ_ENABLE_TETGEN)` → `#include "adapters/tetgen/TetGenTetMesher.h"`。
      - 装配处(`attachWorkflow` 内):先按守卫 new kernel 进 `volumeMeshKernel_`(MMG 开→`TetGenThenMmg`;
        仅 TetGen 开→`TetGenTetMesher`;都关→保持 null),再 `meshingController_.reset(new MeshingController(scene, stack, volumeMeshKernel_.get()));`。

### 5.3 app_shell 条件 link(`CMakeLists.txt`)

- [x] 在 adapter 目标定义之后(`XQ_ENABLE_MMG` 段尾)追加条件段:ON 时 `xq_app_shell` link
      `xq_adapter_tetgen` / `xq_adapter_mmg`,并 `target_compile_definitions` 传 `XQ_ENABLE_TETGEN` /
      `XQ_ENABLE_MMG`。放 adapter 段后(而非现 162 行)是因为目标须先定义才能 link。OFF 时绝不引用 adapter。

### 5.4 合规(AC11)

- [x] MMG **LGPLv3** 官方文本入库:`third_party/mmg/{LICENSE,COPYING,COPYING.LESSER}`(从
      `Externals/src/mmg-5.3.9/` 原样拷,别自拼)。链接方式:TetGen 静态 vendoring、MMG dll 动态。

### 5.5 假绿抽查(AC9)

- [x] 阶段4 已对 faceId/面级连接 facetmarker/ref/翻转做过抽查(见阶段4)。阶段5 装配的真实非假绿证据:
      ON/ON 初次 `test_main_window` 因 app_shell 链入 mmg → 缺 mmg3d.dll 报 `0xc0000135`(DLL_NOT_FOUND)FAIL
      → 修 CMake 注入 mmg bin → PASS。这直接证明 ON 档装配真把 mmg kernel 链进了 app_shell(非摆设)。
- [x] 现有调用方:`test_workflow_controllers` / `test_workflow_integration` 经 `attachWorkflow`(两参不变)间接
      构造,不直接 new `MeshingController`;加默认第三参后 ON/OFF 两档均编过、PASS。

### 5.6 全量验收(主会话独立全新 build 目录,铁律)✓

- [x] `XQ_ENABLE_TETGEN=OFF` 且 `XQ_ENABLE_MMG=OFF`:全新 build → **45/45 ctest 全绿**(== 基线,星形 fallback 不变,AC8/AC10)。
- [x] TETGEN+MMG `=ON`:全新 build → **47/47 ctest 全绿**(多编 tetgen+mmg adapter + 2 测试 + app 装配,AC8)。
- [x] 构建配方见 memory `xq-build-recipe`:vcvars64 + CMAKE_PREFIX_PATH(ON 档加 mmg-5.3.9 根)+ offscreen ctest。

### 5.7 途中新坑(已沉淀 memory)

- ctest `set_tests_properties(... ENVIRONMENT "PATH=...")` 覆盖式钉死 PATH,外部 `set PATH` 进不去 →
  ON 档 GUI 测试找不到 mmg dll(`0xc0000135`)。根治:CMake 里从 `MMG3D_LIBRARY` 反推 bin 注入
  `test_main_window` 的 ENVIRONMENT(`_xq_mmg_bin`)。memory `ctest-environment-overrides-path`。

## 注意(踩过的坑,memory)

- 副作用调用别进 assert(Release /DNDEBUG 删掉=假绿 segfault)。
- 假绿抽查重编用完整 `cmake --build <dir>`,别用单 target(可能静默跳链接=跑旧 exe 假 Passed),核对 exe 时间戳。
- CMAKE_PREFIX_PATH 要把 TetGen 根 + `Externals/.../mmg-5.3.9` 根加进去;ctest 跑 mmg 测试 PATH 含对应 bin(dll)。
- TetGen 编库必须 `-DTETLIBRARY`,否则 `tetrahedralize` 库接口不可见。
- `tetrahedralize` 抛 int 错误码(非返回值),用 try/catch 接。
- 别凭记忆猜 TetGen/MMG API——以本地头(`tetgen.h` / `libmmg3d.h`)+ findings + SV 源码为准。
