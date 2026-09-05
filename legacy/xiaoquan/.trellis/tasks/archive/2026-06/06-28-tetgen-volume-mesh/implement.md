# Implement — 引入 TetGen/MMG 生产级体网格

> 执行计划。新对话接手:先读 `prd.md` + `design.md` + `PROGRESS.md` + `CLAUDE.md`。
> 构建/测试配方见 PROGRESS.md。每步做完跑测试,绿了再进下一步。

## Step 0 — 接手核对(不写代码)

- [ ] `task.py current`;若非本任务 → `task.py start tetgen-volume-mesh`。
- [ ] `git status` 干净;`git log -5` 确认在 master、A1 已归档。
- [ ] 读 `src/services/meshing/VolumeMeshService.{h,cpp}` 全文(星形剖分 + Params 扩展点 + transferBoundaryFaces)。
- [ ] 读 `src/core/XQTetVolumeMeshHandle.h` / `XQTriangleSurfaceGeometryHandle.h`(数据接口,prd §5 已列)。
- [ ] 读 `src/adapters/onnx/` 的 CMake option 范式(`XQ_ENABLE_ONNX`)作模板。

## Step 1 — 前置:TetGen 进 Externals(阻塞项,先解决)

- [ ] 拿 TetGen 1.6.0 源,CMake 编译装进 `Externals/install/windows-x64/tetgen-1.6.0`(含 lib + tetgen.h)。
- [ ] 加进 `CMAKE_PREFIX_PATH`(build 配方四根 → 五根),验证 `find_package`/`find_library` 能找到。
- [ ] 记录 TetGen 许可到本文件/notes,确认与项目分发兼容。
- [ ] **若装不进**:把本步骤作为独立前置卡点上报,不硬推后续。

## Step 2 — adapter:`adapters/tetgen/TetGenVolumeMesher`

- [ ] 写 `TetGenVolumeMesher.h`:按 design §1,公开签名零 tetgen 类型(`tetgenTetrahedralize(surface, params)`)。
- [ ] 写 `.cpp`:XQ surface → `tetgenio in`(pointlist + facetlist,facet marker = triangleFaceId)→
      `tetrahedralize("pq1.4YA", &in, &out)` → `out` → `XQTetVolumeMeshHandle` + 边界面 faceId 回填。
- [ ] CMake:`option(XQ_ENABLE_TETGEN OFF)` + `xq_adapter_tetgen`(link tetgen PRIVATE)。

## Step 3 — service 接 kernel(方案 A:接口注入)

- [ ] 定义 `ITetMesher` 纯虚接口(或 `std::function` 回调),零 tetgen,放 services(或 core)。
- [ ] `VolumeMeshService::buildVolumeMesh` 增可选 mesher 参数:有 → 走 kernel,无 → 星形剖分(默认)。
- [ ] **确认 `xq_services` 不 link tetgen**(审计 Step 6)。adapter 实现 `ITetMesher`,装配在 controller/app。

## Step 4 — 测试(AC3/AC4)

- [ ] `tests/adapters/test_tetgen_volume_mesh.cpp`(`XQ_ENABLE_TETGEN=ON` 才编):
      弯曲段表面 → TetGen 网格无翻转 tet、min 质量 > 阈值;同输入星形剖分有翻转(对照);
      faceId 集合一致。CHECK 宏式,副作用先取变量不进 assert。
- [ ] CMake 注册 `test_tetgen_volume_mesh`(仅 ON 时)。
- [ ] `XQ_ENABLE_TETGEN=ON` 构建 + `ctest -R test_tetgen_volume_mesh` 绿。

## Step 5 —(可选)MMG / 与原网格对照

- [ ] 若 prd 范围内:统计量对照 `0007/Meshes/0090_0001.{msh,vtu}`(经 MSHMeshReader 读入比量级)。
- [ ] MMG 列后续,本任务不强求。

## Step 6 — 反向依赖审计(AC5,铁律 1/2)

- [ ] `rg "#include.*tetgen" src/core src/services` → **必须空**。
- [ ] 确认 `xq_services` link 列表无 tetgen;tetgen 只在 `xq_adapter_tetgen`。

## Step 7 — 全量验收(主会话自己做,不信 worker)

- [ ] **OFF 路径**:全新 build 目录(删 build_verify)`XQ_ENABLE_TETGEN=OFF` 构建 + 全量 ctest → 原 **44 绿**
      (星形剖分 fallback 不破坏,AC6/AC8)。
- [ ] **ON 路径**:`-DXQ_ENABLE_TETGEN=ON` + tetgen 进 PREFIX → 构建零错误 + ctest(44 + tetgen 测试)全绿。
- [ ] **假绿抽查**:篡改 facet marker(全置 0)→ faceId 测试 Release FAIL → 恢复 → PASS;
      或篡改使产翻转 → 质量测试 FAIL。
- [ ] AC1~AC8 逐条对勾。

## Step 8 — 收尾

- [ ] 更新 `PROGRESS.md`:体网格 kernel 状态(TetGen 接入 / MMG 后续 / 许可记录)。
- [ ] 父任务 prd 滚动清单:`TetGen/MMG 未引入` 标已处理(或部分:TetGen done、MMG 后续)。
- [ ] `task.py archive --no-commit` → `git add -A` → 中文 commit:
      `feat: 体网格 — 引入 TetGen kernel(adapter)+ service 接口注入 + faceId 全链路`。

## 关键坑提醒

- **service 别 link adapter**:走接口注入,保 service 纯净可无头单测。
- **副作用不进 assert**;**git-bash 调 .bat 用 `cmd //c "绝对路径"`**;**ctest 前 offscreen**(本任务测试无 Qt 可不需要,但全量 ctest 含 Qt 测试要)。
- **TetGen 对烂输入会失败**:M3 winding 不一致/可能自交;失败返回明确 Status,不静默产烂网格。
- **TetGen 命令行参数**:`p`=PLC、`q`=quality、`Y`=不在边界面加点(保 faceId 映射)、`A`=区域属性/marker。按需调。
