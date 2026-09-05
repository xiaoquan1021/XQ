# M9b-C 实现计划 — 分阶段 + 验证门 + 回滚点

> 顺序原则:**先把 surface 渐进通路打透(分块 + 多 actor + 进度回调 + stats + 段偏移 + 无悬垂,裸 actor 即可验),再接 B 的 LOD actor + still/interactive 切级,最后 tet 通路复用骨架**。每阶段独立编译 + 跑相关测试绿后进下一阶段;全部完成跑 Release 全量 ctest。依赖 B:阶段 3(LOD/切级)在 B 合入后做,阶段 1/2/4 不依赖 B。

## 前置:基线(阶段 0)
- 确认当前分支 Release 全量 ctest 绿(记基数)。
- 留 `test_scene_renderer` 现有 surface/tet 断言为不退化基线(旧 `addSurface`/`addVolumeMesh` 不动)。
- 准备/复用合成大几何生成器(scale-probe R1 已有 20M 三角 + 10M tet 生成,可弃码参考;本期可用更小的确定性几何做正确性核,大规模仅做"首块可见"计时)。
- **回滚点 R0**:基线 commit 前干净树。

## 阶段 1:RenderStats 字段 + 渐进入口骨架 + ChunkPlan(裸 actor,surface)
**目标 AC1/AC3/AC4/AC6/AC7(surface,非 LOD)。**

1. `XQSceneRenderer.h`:`RenderStats` 追加 `int chunkCount=0; int completedChunkCount=0;`(决策 5);新增 VTK-free `struct ChunkUploadSpec { std::size_t maxCellsPerChunk = <默认,待 B 对齐>; };`;新增公共方法声明 `addSurfaceProgressive(const IGeometrySource&, const ChunkUploadSpec&, const ChunkProgressFn& = {})`,`using ChunkProgressFn = std::function<void(const RenderStats&)>;`(加 `<functional>`)。
2. `XQSceneRenderer.cpp`:
   - 内部 `ChunkPlan`:按 `maxCellsPerChunk` 把 `[0, triangleCount)` 切区间,记 `{begin,end}`。
   - 内部 `build_surface_chunk(source, {begin,end}, globalFaceRange, remapScratch)`:决策 2 重映射点 + 复用 `upload_points`/`build_cells` 批量原语(copy-on-upload)+ 本块 faceId 段转 float(决策 3 全局 range)。
   - `Impl::addSurfaceProgressive`:`meta()` guard → `acquire_triangles()`/`acquire_points()`(全量,入口内 RAII)→ 算 `globalFaceRange` → 切块 → for 块:build_chunk → `vtkPolyDataNormals`→mapper(全局 LUT)→**裸 `vtkActor`**(阶段 1)→ `AddActor` → 填 cumulative 调 `onChunk`。返回末次 cumulative。
   - 公共转发 `XQSceneRenderer::addSurfaceProgressive`。
3. **段偏移正确性**:块内局部 cell j → 全局 faceId = `faceIds[begin+j]`(显式段基址,父级契约)。
4. **风险先行(R1)**:先用最小确定性几何(如 make_tet_surface 复制成多块规模)跑通切块路径,确认 remap/局部索引/段偏移正确,再上大规模。

**阶段 1 验收**(新增 `test_scene_renderer_progressive` 或扩 `test_scene_renderer.cpp`):
- 多块几何 `clear()` 后 `addSurfaceProgressive`:`onChunk` 触发 N 次;首次触发 `completedChunkCount==1 && chunkCount>1 && actorCount>=1`,此刻 `renderOffscreenToRgba` 非黑(AC1)。
- 完成后 `pointCount==源总点数`、`actorCount==chunkCount`、`completedChunkCount==chunkCount`、`ok==true`(AC3)。
- 块内 j 经段基址映射的 faceId == `acquire_triangles().view().faceIds[begin+j]` 逐元素核(AC4,小规模 dump)。
- 渐进后释放 source/handle 再 render 正确(AC6)。
- grep 确认渐进入口内无 `SetPoint(` 逐点 / `InsertNextCell` 逐元素 / 借用 source 指针进 actor。

**回滚点 R1**:阶段 1 commit。

## 阶段 2:渐进等价整块 + 全局 LUT 颜色一致(surface)
**目标 AC2。**

1. 同一大几何分别走旧 `addSurface`(整块)与新 `addSurfaceProgressive`(分块),`renderOffscreenToRgba` 两路输出:非黑 + 覆盖(非黑像素占比 / 包围盒)一致(语义等价,不强求 bit 等,R2)。
2. 跨块 faceId 颜色:用多 faceId 值几何,核分块路径下同一 faceId 在不同块像素颜色一致(全局 `globalFaceRange` 生效,无断层)。

**阶段 2 验收**:等价对比测试绿;假绿抽查——篡改一块的 faceId 段使其颜色应变,确认对应断言变红(`acquire_*` 不进 assert)。
**回滚点 R2**:阶段 2 commit。

## 阶段 3:接 B 的 LOD actor + still/interactive 切级(依赖 B)
**目标 AC5。** B 合入后做。

1. 块 actor 从裸 `vtkActor` 换 B 的 `vtkLODActor`(或 B 约定的 LOD 封装),`ChunkUploadSpec` 接 B 的 LOD 级提示。
2. `XQRenderWidget`(`cpp:27-31` mount 处 / 41-47 render):接交互器 `GetInteractor()->SetDesiredUpdateRate()` / `SetStillUpdateRate()`,或监听 Start/EndInteractionEvent 切级;作用于全部块 actor。
3. `onChunk` 回调里(真实窗口)调 `XQRenderWidget::render()` 刷新进度。

**阶段 3 验收**:headless 核块 actor 均为 LOD actor 类型 + `DesiredUpdateRate` 接线存在(AC5 可核部分);真实窗口手动验 interactive 低模 / still 高模并记录。
**回滚点 R3**:阶段 3 commit。

## 阶段 4:tet 通路 + 全量验收
**目标 AC1-7(tet)+ 全局。**

1. `addVolumeMeshProgressive`:复用阶段 1 骨架,`acquire_tetrahedra()` 切 tet 区间,`build_volume` 风格(`SetCells(VTK_TETRA)`,`cpp:227-234`)+ 边线管线(`vtkGeometryFilter`→`vtkExtractEdges`,`cpp:443-457`)每块独立;tet 无 faceId,跳 LUT。
2. tet 渐进测试:首块可见 / stats 语义 / 等价整块 / 无悬垂(对应 AC1/AC2/AC3/AC6)。
3. **AC7 分层**:grep io/core 无新增 `vtk` include;`IGeometrySource` 头 `static_assert` 编译通过;`RenderStats` 仅追加字段,旧测试不受影响(默认 0)。
4. **Release 全量 ctest**(`xq-build-recipe` 配方:vcvars64 + `CMAKE_PREFIX_PATH` + offscreen;新 build 目录,run exe 需 vtk-9.3.0/bin 在 PATH):全绿。
5. **假绿抽查**:篡改一处块数据确认对应断言变红;核增量编译 exe 时间戳真变(memory `ninja-target-incremental-fakegreen-trap`),用完整 `cmake --build <dir>` 不用单 target。

**回滚点 R4**:阶段 4 commit。

## 验证命令
```bash
# Release 配置 + 构建 + 全量 ctest(CRLF .bat;比照 build_m9a.bat 复制改 build 目录名)
cmd //c "C:\\Users\\OCEAN\\Desktop\\XIAOQUAN\\XQ\\build_m9b_c.bat"

# 渐进入口无逐元素/无借用核查
rg -n 'SetPoint\(|InsertNextCell|SetArray\(' XQ/src/visualization/XQSceneRenderer.cpp
# RenderStats 新字段 + 渐进入口签名核查
rg -n 'chunkCount|completedChunkCount|addSurfaceProgressive|addVolumeMeshProgressive|ChunkUploadSpec' XQ/include/visualization/XQSceneRenderer.h XQ/src/visualization/XQSceneRenderer.h XQ/src/visualization/XQSceneRenderer.cpp
# 分层:io/core 无 vtk
rg -n 'vtk' XQ/src/io XQ/src/core/source
```
(注:`XQSceneRenderer.h` 实读在 `XQ/src/visualization/`;若仓库另有 `include/` 镜像以实际为准——本期文件落 `src/visualization/`。.bat 用 Python 转 CRLF 防 LF 假报找不到 Ninja,memory `lf-bat-fake-ninja-not-found`。)

## 回滚策略
- 各阶段独立 commit(R1-R4),任一阶段验收失败 `git reset --hard` 回上一回滚点(destructive,需用户确认)。
- 阶段解耦:阶段 1/2/4 不依赖 B;阶段 3(LOD/切级)独立,B 缺位可跳过阶段 3 先交付裸 actor 渐进(AC1-4/6/7),阶段 3 后补不影响已完成阶段。

## 子代理派发(可选)
- 阶段 0 基线 dump / 合成几何生成读代码可派子代理并行;实现改动主会话亲自做(VTK 管线 + 连续编译验证)。
- B 的 LOD actor 接口/约定确认可派子代理读 B 的 design/实现(阶段 3 前置)。

## 依赖与关联
- 依赖:B(LOD/decimation)合入后做阶段 3;M9a 已归档(copy-on-upload 决策、批量上传核)。
- 关联 memory:`renderer-source-copy-on-upload-not-borrow`、`m8b1-consumer-access-patterns`、`xq-surface-winding-not-consistent`、`xq-build-recipe`、`no-sideeffect-in-assert`、`ninja-target-incremental-fakegreen-trap`、`lf-bat-fake-ninja-not-found`、`ctest-environment-overrides-path`。
- 完成后:渐进/分块上传打掉一次性上传的"无反馈墙",与 B 的 LOD 联动达成 still/interactive 切级,父任务 AC-C 达成。
