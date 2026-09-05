# PRD — 引入 TetGen/MMG 生产级体网格

> ⚠️ **作废(2026-06-28)**:本任务把 TetGen 当必做 kernel,与 spec 不符。
> `architecture/external-libs.md` 选型是 **MMG 5.3.9**(LGPL),TetGen(AGPL)从未在依赖基线里出现。
> 已由调研任务 `06-28-mmg-volume-mesh-research` 取代。本目录仅留档,勿照此实现。

> 任务 id:`06-28-tetgen-volume-mesh` | 优先级 P1 | package XQ
> 前置:M0~M7 主线已完成。本任务把"质心星形剖分"替换为生产级体网格 kernel。

## 1. 背景与问题

`VolumeMeshService::buildVolumeMesh` 当前用 **质心星形剖分**(centroid star tetrahedralization):
把闭合表面顶点质心 C 加为一点,每个表面三角 (a,b,c) 生成一个 tet (a,b,c,C)。

- **只对相对 C 星形可见的域正确**。弯曲主动脉若质心落体外/壁附近 → 产生**翻转或退化四面体**,
  靠质量摘要(min/符号)暴露但不剔除。
- tetCount == 表面三角数,网格密度无法控制,没有内部加点/优化。

这不是生产级网格。本任务引入 **TetGen**(必做)/ **MMG**(可选)作为 kernel,生成高质量体网格,
并保持 faceId 边界面全链路、保持 `core/services` 公开 API 零外部依赖的铁律。

## 2. 范围

### In scope

1. **新建 `src/adapters/tetgen/`**:把闭合三角表面(`XQTriangleSurfaceGeometryHandle`)+ 边界面元数据
   送 TetGen,生成体网格,**转回 `XQTetVolumeMeshHandle`**(XQ 自有类型)。
   - 输入:表面 points/triangles/faceId、网格控制参数(目标边长/体积约束/质量比)。
   - 输出:tet 顶点 + tets + 每个边界面三角的 faceId 映射(TetGen 保留输入面标记)。
2. **`VolumeMeshService` 接 kernel**:利用其 `Params` 已预留的 "kernel selection" 扩展点,
   新增 kernel 选择(Star / TetGen / MMG),默认在 kernel 可用时走 TetGen,不可用时**降级回星形剖分**
   (现有行为不删,作 fallback)。
3. **CMake**:`option(XQ_ENABLE_TETGEN OFF)`(仿 `XQ_ENABLE_ONNX` 范式),ON 时编 `adapters/tetgen`
   + link tetgen;OFF 时整个 adapter 不编,`VolumeMeshService` 只剩星形剖分。
4. **依赖 manifest**:TetGen 源/库怎么进 `Externals`(见 plan/09 提及的 kernel manifest);
   本任务需把 TetGen 装进 Externals 并写进构建配方,或明确记为前置依赖项。
5. **测试**:TetGen 路径的单测(`XQ_ENABLE_TETGEN=ON` 才注册):一个弯曲段表面 → 体网格质量优于星形剖分
   (无翻转 tet、min 质量 > 阈值);faceId 全链路保留;闭合校验。

### Out of scope(记后续)

- MMG 重网格(remeshing)/ 各向异性 —— MMG 列为可选,TetGen 跑通后再加。
- 表面修复(自交/非流形修复)—— 输入假设是 M3/M4 产的闭合 2-流形。
- 与原 `0007/Meshes/0090_0001.{msh,vtu}` 的逐单元对照(那是另一条技术债,可在本任务顺带做统计量对照)。
- 体网格自适应加密 / 边界层网格。

## 3. 铁律约束(违反即 BLOCKER)

1. **kernel 只在 adapter 私有实现出现**:`tetgen.h` / TetGen 类型只能进 `src/adapters/tetgen/*.cpp`。
   `core` / `services` 的公开 `.h` 不得 `#include` tetgen,公开 API 只暴露 XQ 类型
   (`XQTriangleSurfaceGeometryHandle` / `XQTetVolumeMeshHandle` / `ModelFace`)。
   审计:`rg "#include.*tetgen" src/core src/services` 必须空。
2. **依赖方向**:`services → adapters/tetgen → core`(若 service 直接调 adapter)
   **或** controller/上层组装(若不让 service link adapter)。**确认 service 是否允许 link adapter**——
   现有 `xq_services` 只 link `xq_core`;若让它调 TetGen 会引入 adapter 依赖。
   **设计决策见 design.md §2**(倾向:adapter 暴露纯 XQ 类型的函数,service 通过接口/函数指针调,
   或把 TetGen 装配挪到 controller 层,保 service 纯净)。
3. **faceId 全链路**:TetGen 必须保留输入表面的面标记(`-A` 或 region/facet marker),
   转回时每个边界三角的 faceId 不丢——与现有 `transferBoundaryFaces` / 星形路径语义一致。
4. **降级不删旧路径**:星形剖分作为 `XQ_ENABLE_TETGEN=OFF` 的 fallback 保留,原 M4 测试不破坏。
5. **坐标系/单位不变**:LPS/mm,TetGen 不引入单位转换。

## 4. 验收标准(逐条可证伪)

- [ ] **AC1 adapter 落地**:`src/adapters/tetgen/` 把闭合表面转 `XQTetVolumeMeshHandle`,
      公开签名只含 XQ 类型,`tetgen.h` 仅在 .cpp 出现。
- [ ] **AC2 kernel 选择**:`VolumeMeshService` 能选 TetGen kernel(可用时);不可用时降级星形剖分,行为可证。
- [ ] **AC3 质量优于星形**:一个弯曲血管段表面(质心落体外的用例)→ TetGen 体网格**无翻转 tet**、
      min 归一化质量 > 阈值(定一个,如 0.1);同一输入星形剖分会产翻转 → 对照可见差异。
- [ ] **AC4 faceId 全链路**:TetGen 输出的边界面 faceId 与输入表面一致(wall/inlet/outlet 不丢、不混)。
- [ ] **AC5 反向依赖零渗透**:`rg "#include.*tetgen" src/core src/services` 空;`xq_core`/`xq_services`
      公开头不暴露 tetgen 类型。
- [ ] **AC6 CMake option**:`XQ_ENABLE_TETGEN=OFF`(默认)全量构建+ctest 与现状一致(44 绿);
      `=ON` 时多编 adapter + 新增 tetgen 测试,全绿。
- [ ] **AC7 假绿抽查**:篡改 faceId 映射(故意错标)→ AC4 测试 Release FAIL → 恢复 → PASS;
      或篡改使产翻转 tet → AC3 测试 FAIL。
- [ ] **AC8 主线不回归**:OFF 路径下原 44 测试全绿;星形剖分 fallback 不破坏。

## 5. 已知现状(实测,供执行者接手)

- `VolumeMeshService`(`src/services/meshing/VolumeMeshService.{h,cpp}`,241+98 行):
  - `buildVolumeMesh(surface, faces, params)` → `Result{status, mesh}`,星形剖分。
  - `Params` 注释已预留 "target edge length, grading, **kernel selection**" —— 本任务的扩展点。
  - `Status`:Ok / NotClosed / InvalidSurface / NullScene。要求输入闭合 2-流形(每边恰 2 三角)。
  - `transferBoundaryFaces(from,to)` 已有,搬 faceId/name/kind/capId。
- `XQTetVolumeMeshHandle`:`addPoint` / `addTet(a,b,c,d)` / `points()` / `tets()`(`array<int,4>`)/ `is_valid()`。
- `XQTriangleSurfaceGeometryHandle`:`points()` / `triangles()` / `triangleFaceId(i)`。
- `src/adapters/` 现有 `onnx/`(`XQ_ENABLE_ONNX` 范式可仿)、`vtk/`;**无 tetgen/mmg 目录**。
- `Externals/install/windows-x64`:目前装了 tinyxml2/qt/vtk;**TetGen 未装**——这是前置依赖缺口。

## 6. 风险 / 前置

- **TetGen 未装**:Externals 无 TetGen,本任务第一步要解决 kernel 怎么进 Externals
  (源码编译入 install,或找现成库)。若装不进,本任务**阻塞**——把"装 TetGen"作为独立前置步骤先做。
- **service 是否 link adapter**:见铁律 2,这是关键设计决策,先在 design 定清,别让 `xq_services` 直接依赖 tetgen。
- **TetGen 许可**:TetGen 是 AGPL/特定许可,确认许可是否符合项目分发要求(记录,非本任务阻塞但要标)。
- **退化输入**:M3 表面 winding 全局不一致 / 可能有自交,TetGen 对烂输入会失败;
  失败时返回明确 Status,不静默产烂网格。
