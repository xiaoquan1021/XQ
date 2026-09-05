# M9b-B 实现计划 — 分阶段 + 验收门 + 回滚点

> 顺序:**先 F(CMake 前置,独立可验)→ 再 vtkLODActor headless 烟囱(化解 R1/R5 最大不确定性)→ 再 decimation 多级 + faceId 保级 → 再 CPU 选级 + RenderStats 扩字段 → 最后异步 + 大规模 probe**。每阶段独立编译 + 相关测试绿后再进;全部完成跑 Release 全量 ctest。

## 前置:基线(阶段 0)

- 确认当前分支 Release 全量 ctest 绿(M9a 基线;`test_scene_renderer` 在内)。
- 留 surface 渲染基线:`test_scene_renderer` 的 surface 用例输出(`pointCount`/`actorCount`/非全黑)用于 AC-B6 等价对比。
- **回滚点 R0**:基线 commit 前干净树。

## 阶段 F:CMake RenderingLOD 前置(F,独立)

1. `CMakeLists.txt:67-84` `find_package(VTK 9 REQUIRED COMPONENTS ...)` 追加一行 `RenderingLOD`。
2. 确认 `xq_visualization`(`:109-127`)`vtk_module_autoinit(... MODULES ${VTK_LIBRARIES})` 随新模块带入(无需改 autoinit 调用)。
3. 确认运行期 `vtkRenderingLOD-9.3.dll` 在 ctest PATH(比照现有 vtk bin 注入方式,memory `ctest-environment-overrides-path`)。
4. **烟囱**:临时在 `test_scene_renderer` 加一处 `#include <vtkLODActor.h>` + `vtkNew<vtkLODActor>` 实例化 + AddActor + `renderOffscreenToRgba`,跑通(证 RenderingLOD 链接 + autoinit + dll 就位)。

**阶段 F 验收**:configure 通过(`find_package` 不报缺 RenderingLOD);烟囱测试编译 + 离屏渲染 ok 非全黑(AC-F)。
**回滚点 R1**:阶段 F commit。

## 阶段 1:LodOptions + RenderStats 扩字段 + 默认关不退化

1. `XQSceneRenderer.h`:`RenderStats` 追加 `long long uploadedPointCount = 0;`(决策 4)。新增 `struct LodOptions { bool enabled=false; int fixedLevel=-1; long long budgetTriangles=0; };`(VTK-free)。`addSurface` 增可选参数 `addSurface(const XQTriangleSurfaceGeometryHandle&, const LodOptions& = {})`(默认值 → 旧调用不变)。
2. `XQSceneRenderer.cpp`:`addSurface`/`build_surface` 路径在 LOD 关时 `uploadedPointCount = pointCount`,其余不变。
3. 跑 `test_scene_renderer`:既有断言全绿(AC-B6),输出与基线等价。

**阶段 1 验收**:默认行为零退化;新字段 LOD 关时 == pointCount。
**回滚点 R2**:阶段 1 commit。

## 阶段 2:SurfaceLodBuilder 多级 decimation + faceId 保级(B1/B4)

1. 新建 `XQ/src/visualization/SurfaceLodBuilder.{h,cpp}`(加入 `xq_visualization` 源列表 `CMakeLists.txt:109-114`)。
   - `struct CellFaceIdMap { std::vector<int> cellToFaceId; bool valid; };`
   - `struct LodLevel { vtkSmartPointer<vtkPolyData> poly; CellFaceIdMap faceMap; long long pointCount; long long triangleCount; };`
   - `LodLevels buildSync(vtkPolyData* full, const ReadSpan<int>& faceIds)`(测试用确定性同步)。
2. **中模(level 1,保 faceId)**:按 faceId region 切分 full(`vtkThreshold`/手工按 cell scalar 分组),各 region 子网格独立 `vtkQuadricDecimation`(设目标减面比,如 0.25),合并输出;按 region 拼接顺序重建 `CellFaceIdMap.cellToFaceId`(valid=true)。R3:region 过碎(三角数 < 阈值)的 region 原样保留不 decimate。
3. **远景(level 2,不保 faceId)**:`vtkQuadricClustering`(CopyCellData off,目标减面比如 1/16),`CellFaceIdMap.valid=false`。
4. **full(level 0)**:复用 `build_surface` 输出;`CellFaceIdMap` = 恒等(cellToFaceId = faceIds span 拷贝,valid=true)。
5. 各级仍接 `vtkPolyDataNormals`(AutoOrient+Consistency+SplittingOff)—— winding 坑(memory `xq-surface-winding-not-consistent`)。

**阶段 2 验收**:小多 region 网格(每 region 不同 faceId)下,buildSync 出 3 级;medium 级逐三角经 faceMap 查回 faceId 落所属 region 原值(region 不串色);各级三角数逐级下降(medium ≤ ~1/4,far ≤ ~1/16);full 着色与 M9a 像素等价(AC-B1/B4)。假绿抽查:篡改 region faceId → 断言变红。
**回滚点 R3**:阶段 2 commit。

## 阶段 3:CPU 选级 + 固定级渲染 + headless 确定性(B2/B3/B5)

1. 新建 `LodLevelSelector`(纯函数,可在 SurfaceLodBuilder.cpp 内):`int select(const LodOptions&, const LodLevels&)` —— `fixedLevel>=0` 直接用;否则按 `budgetTriangles` 选最高保真且 ≤ 预算的级(无满足则取 far)。
2. `addSurface`(LOD 开):build levels → select k → 取 level[k] 建 mapper(k<2 走 faceId LUT 路径 `:406-416`;k==2 `ScalarVisibilityOff` 平色)→ 普通 `vtkActor`(决策 1 固定级直挂,headless 确定性)→ `uploadedPointCount = level[k].pointCount`。
3. 新建 `tests/visualization/test_surface_lod.cpp`(加 add_executable + add_test,比照 `:793-797,867`):
   - 中等规模多 region surface,LOD 开 + fixedLevel=1(medium):断言 `uploadedPointCount < pointCount` 且 ≤ full 的 ~1/4;`renderOffscreenToRgba` ok + 非全黑;pointCount 仍 == 源点数。
   - fixedLevel=2(far):断言更低 uploadedPointCount;平色仍可见(非全黑)。
   - still 级语义:medium 级包围盒 / 非空 与 full 一致(AC-B5)。

**阶段 3 验收**:固定级确定性取级 + 降量可断言(AC-B2/B3/B5);pointCount 源义不变。
**回滚点 R4**:阶段 3 commit。

## 阶段 4:异步 decimation + 交互路径 vtkLODActor(B1 异步 + 交互)

1. `SurfaceLodBuilder::buildAsync`:`std::async(std::launch::async, ...)`(或核对 services 既有线程封装)在 full `vtkPolyData` 副本上算 levels,返回 future;**回填到 actor 在主/渲染线程**(R4 线程安全)。
2. `addSurface`(交互/异步):提交 buildAsync,full 级先装配返回;低模就绪后主线程把各级 `AddLODMapper` 挂到 `vtkLODActor`。`XQRenderWidget` 路径 DesiredUpdateRate 驱动 still/motion。
3. 测试:断言 buildAsync 提交后 addSurface 立即返回(future 未就绪 / 计时);完成回调后 levels 就绪(AC-B1 异步)。

**阶段 4 验收**:异步不阻塞主线程;交互 vtkLODActor 装配成功(headless 仅验装配 + full 可见,取级不在 headless 测)。
**回滚点 R5**:阶段 4 commit。

## 阶段 5:大规模 probe 量化 + 全量验收(B2 峰值)

1. 扩 `XQ/probe/main.cpp`(`XQ_ENABLE_SCALE_PROBE` OFF 档,`CMakeLists.txt:632-645`):large(20M 三角)走 LOD 路径(fixedLevel=medium / far),用 psapi 量进程峰值工作集,对照 M9a 全量 3786 MiB 基线。
2. **AC-全局**:`rg` 核 io/core 目录无新增 `vtk*`;Source 头 static_assert 编译通过;link 边方向(visualization→VTK::RenderingLOD)无环。
3. **Release 全量 ctest**(`build_m9b_b.bat`,比照 `build_m9a.bat` 改 build 目录名,CRLF,vcvars64 + CMAKE_PREFIX_PATH + offscreen + vtk bin 在 PATH):全绿,新增 `test_surface_lod` 计入。
4. **假绿抽查**:篡改一处源/faceId,确认对应断言变红(decimate/acquire 不进 assert,memory `no-sideeffect-in-assert`)。

**阶段 5 验收**:large 开 LOD 峰值较 3786 MiB 显著下降(数量级达标,AC-B2);全量 ctest 绿 + 分层/签名零违反。
**回滚点 R6**:阶段 5 commit。

## 验证命令

```bash
# Release 配置 + 构建 + 全量 ctest(CRLF .bat,vcvars64 + CMAKE_PREFIX_PATH + offscreen;vtk bin 含 RenderingLOD dll 在 PATH)
cmd //c "C:\\Users\\OCEAN\\Desktop\\XIAOQUAN\\XQ\\build_m9b_b.bat"

# 分层核查:io/core 无 vtk
rg -n 'vtk' XQ/src/io XQ/src/core

# RenderingLOD 已进 find_package
rg -n 'RenderingLOD' XQ/CMakeLists.txt

# 大规模 probe(OFF 档单独配置,XQ_ENABLE_SCALE_PROBE=ON)
cmd //c "C:\\Users\\OCEAN\\Desktop\\XIAOQUAN\\XQ\\build_probe.bat"   # 比照现有,跑 large LOD vs 全量峰值
```
(build_m9b_b.bat 比照 build_m9a.bat 复制改 build 目录名;CRLF;run exe 需 vtk-9.3.0/bin 含 vtkRenderingLOD-9.3.dll 在 PATH —— memory `ctest-environment-overrides-path`、`lf-bat-fake-ninja-not-found`、`cmake-bad-cache-ninja-loop`、`ninja-target-incremental-fakegreen-trap` 增量重编核对 exe 时间戳。)

## 回滚策略

- 各阶段独立 commit(R1–R6),任一阶段验收失败 `git reset --hard` 回上一回滚点(destructive,需用户确认)。
- F / 阶段1(默认关)与后续解耦:LOD 功能未达标可保留 F + RenderStats 字段,LOD 默认关回退到 M9a 等价行为。

## 子代理派发(可选)

- 阶段 2 的"VTK 9.3 vtkQuadricDecimation/Clustering 是否携带 cell data、SetTargetReduction 行为"实编核对可派子代理跑最小 probe(别凭记忆,memory 准则)。
- 实现改动(跨文件接口 + 连续编译验证)主会话亲自做。

## 依赖与关联

- 依赖:M9a 已归档,Source 1.0 已冻;**A 可并行**(B 用 ResidentSurfaceSource)。
- 下游:**C 依赖本任务**(分块策略基于 LOD 级 + cell→faceId 表);**D**(拾取)消费本任务产出的显式 cell→faceId 映射。
- 关联 memory:`xq-surface-winding-not-consistent`、`no-sideeffect-in-assert`、`ctest-environment-overrides-path`、`xq-build-recipe`、`ninja-target-incremental-fakegreen-trap`、`lf-bat-fake-ninja-not-found`、`cmake-bad-cache-ninja-loop`、`m8b1-consumer-access-patterns`、`token-cost-from-long-sessions`(按里程碑开新会话 + 多用 subagent)。
