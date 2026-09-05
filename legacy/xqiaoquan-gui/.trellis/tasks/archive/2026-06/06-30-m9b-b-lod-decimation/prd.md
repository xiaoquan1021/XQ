# M9b-B LOD/降采样上传(P0,含 F:VTK RenderingLOD CMake 前置)

> 父任务:`06-30-m9b-large-scale-viz`。本子任务打 **瓶颈线1 — GPU 上传峰值 + 耗时**(scale-probe 实测 large=20M 三角:addSurface 上传 23515ms / 进程峰值工作集 3786 MiB,见 `XQ/probe/REPORT.md:13,16`)。**只用 LOD / 降采样打**,不碰 reader 常驻/双份(那是 A/E 的线2)。

## Goal

在不破已冻 **libXQ / Source 1.0** 公共签名、不改 `XQSceneRenderer` 现有 add* 公共契约语义的前提下,为大规模三角 surface 引入 **多级 LOD + 离线/异步 decimation + CPU 侧按预算选级**,把大规模 surface 渲染的 **进程峰值工作集 / GPU 上传量** 较 M9a 全量上传显著下降,同时保证:① still 级几何语义可识别不退化;② 在"保 faceId 级"上 faceId 着色/拾取仍正确(显式 cell→faceId 映射,不再依赖下标相等);③ headless 离屏可确定性验收(无 interactor 也能固定取级)。

## Background(实读依据,已核坐标)

- **真实根因(父任务校准)**:M9a 后 `build_surface`(`XQSceneRenderer.cpp:189-218`)已是 `vtkCellArray::SetData` + points `memcpy` 整块批量,**逐元素风暴已消除**;large 的 3786 MiB / 23.5s 来自 `vtkPolyDataNormals`(AutoOrient+Consistency,`:397-401`)对 20M 三角的 deep copy + `vtkPolyDataMapper` 一次性整块上 GPU。probe/REPORT.md:47 的"逐 InsertNextCell"归因是 M9a 重写前描述,**作废**。
- **当前 surface 管线**:`addSurface`(`:373-425`)→ `build_surface` 出 `vtkPolyData`(points + tris + faceId float cell scalar `:204-216`)→ `vtkPolyDataNormals`(`:397`)→ `vtkLookupTable` + `vtkPolyDataMapper`(ScalarModeToUseCellData,`:411-416`)→ `vtkActor`(`:418`)。faceId 是 **float cell scalar 喂 LUT 着色**(非 int,`:209-210` 逐元素 `SetValue` 转 float)。
- **RenderStats 契约**:`XQSceneRenderer.h:21-25` = `{bool ok; int actorCount; long long pointCount;}`。`addSurface` 返回 `pointCount = surface.pointCount()`(源点数,`:424`),headless 测试断言其等于输入点数(`test_scene_renderer.cpp:91-92`)。**pointCount 源义不可改**。
- **headless 取级坑**:`renderOffscreenToRgba`(`:595-667`)自建 throwaway `vtkRenderWindow`(`SetOffScreenRendering(1)`),**无 interactor**;`vtkLODActor` 的级选择由 `vtkRenderer::AllocatedRenderTime`(经 interactor DesiredUpdateRate/StillUpdateRate)驱动,离屏下不被驱动 → 默认走 still(full-res),**自动取级在 headless 不可测**。需在 design 给"确定性固定级"方案。
- **VTK 9.3 能力实编核实**(本机 `Externals/install/windows-x64/vtk-9.3.0`):`vtkLODActor.h` / `vtkQuadricDecimation.h` / `vtkQuadricClustering.h` / `vtkDecimatePro.h` / `vtkCompositePolyDataMapper2.h` 头均存在;`VTK::RenderingLOD` 模块 + `lib/vtkRenderingLOD-9.3.lib` 存在,但 **当前 `find_package(VTK ... COMPONENTS)`(`CMakeLists.txt:67-84`)未含 `RenderingLOD`** → F 前置必须补。`vtkLODActor` 公共 API:`AddLODMapper(vtkMapper*)` / `SetLowResFilter` / `SetMediumResFilter`,无 byte-budget API(显存只能 CPU 侧控量)。`vtkQuadricClustering` 有 `CopyCellData` 标志但 **合并 cell**(改拓扑 → 破 cell↔faceId 一一对应)。
- **decimation 离线/异步**:20M 三角单线程 `vtkQuadricDecimation` 预处理会卡主线程,父任务硬约束要求异步/后台。
- **cell→faceId 隐式契约**:当前 cellId↔faceId 依赖"cell 顺序 == 三角下标"(`build_cells` 不重排,`:166-187`;faceId 并行 span `:208-211`)。**decimation 抽稀/重排三角后下标相等失效** → 必须落显式 cell→faceId 映射表(父任务级硬契约,与 C/D 对齐)。

## Scope

### In scope

- **F(前置)** `CMakeLists.txt:67-84` 的 `find_package(VTK ... COMPONENTS)` 追加 `RenderingLOD`;确认 `xq_visualization` 的 `vtk_module_autoinit`(`:104-107` 同款,`:122-127`)随 `${VTK_LIBRARIES}` 带入新模块的 object factory 注册。
- **B1 离线/异步多级 decimation 工厂**(visualization,新组件):对单个 surface `IGeometrySource` 生成 N 级低模 `vtkPolyData`(level 0 = full,逐级降),在 **后台线程**计算,主线程不阻塞。
  - **中模(参与拾取/着色级)**:**保 faceId** —— 用 vtkQuadricDecimation,且 **按 faceId region 分块独立 decimate 再合并**,使每个输出三角仍唯一归属一个 faceId region,重建该级显式 cell→faceId 映射表。
  - **远景(纯显示级)**:用 vtkQuadricClustering(最快,改拓扑)—— **不保 faceId**,平色,仅极远景,不参与拾取/着色。
- **B2 LOD actor 装配 + CPU 侧选级**(visualization):`vtkActor`→`vtkLODActor`,把各级 mapper `AddLODMapper` 挂上;CPU 侧 `LodBudget`(目标最大上传三角/点数,模拟显存预算)选级。
- **B3 headless 确定性取级 + RenderStats 扩字段**:新增 **固定级覆盖入口**(测试/无 interactor 路径直接取指定级 mapper 渲染);`RenderStats` **追加** `uploadedPointCount`(实际上传级点数)字段,**不改 `pointCount` 源义**。
- **B4 cell→faceId 显式映射契约落地**:full 级映射 = 恒等(cell i → faceId[i]);中模级 = 分 region 重建表;远景级 = 标记"无 faceId"。映射表作为 LOD 级的随附产物。

### Out of scope

- **渐进/分块上传**(多 actor 边建边显):C 子任务,本任务只做"单 actor 多级 LOD",不做分块。
- **大网格拾取**(屏幕点→cell→faceId 前半段):D / M9b',本任务只**约定并产出** cell→faceId 映射表供 D 用,不实现拾取。
- **tet volume mesh 的 LOD**:`addVolumeMesh`(`:427-468`)走 boundary→edges wireframe,瓶颈形态不同,本任务范围仅 surface;volume 留后续。
- **几何 mmap 源 / borrow-faceId / 工厂**:A 子任务。B 用现成 `ResidentSurfaceSource` 即可喂(`addSurface` 现状 `:379-381`)。
- **裁剪/视锥剔除**:父任务已显式后置。
- 不复用 `XQ/probe/MappedGeometrySource.*` 弃码。

## Constraints(父级硬约束 + 本任务)

- [ ] **不破 Source 1.0 公共签名**:`IGeometrySource` 3 纯虚(`IGeometrySource.h:19-26`)绝不加纯虚;static_assert 仍成立。LOD/decimation 逻辑全封在 visualization 的新组件 + `.cpp`,经现有 `acquire_points()`/`acquire_triangles()` 消费。
- [ ] **不破 `XQSceneRenderer` 公共契约**:`addSurface(const XQTriangleSurfaceGeometryHandle&)` 签名与既有行为保留;LOD 经 **新可选入口/可选参数 + 默认关**接入,默认行为与 M9a 等价(回归测试不动)。
- [ ] **分层**:LOD/decimation → visualization;VTK 只在 visualization 私有实现;io/core 永无 `vtk*`。
- [ ] **RenderStats**:`pointCount` 恒报源点数(`test_scene_renderer.cpp:91` 断言不变);LOD 实际上传量经 **新增 `uploadedPointCount`** 暴露,不改旧义。
- [ ] **mapper 锁定**:若引入 composite mapper,锁 `vtkCompositePolyDataMapper2`(全里程碑统一);本任务单 actor 多级优先用 `vtkLODActor` + 多 `vtkPolyDataMapper`,如需 composite 显式锁定并写入 design。
- [ ] **decimation 离线/异步**:20M 预处理必须后台,主线程不阻塞;副作用调用(acquire_* / decimate)不进 assert(验收铁律)。
- [ ] **cell→faceId 显式映射**:抽稀/重排后拾取/着色靠显式表,不依赖下标相等(父级硬契约)。
- [ ] 验收铁律:Release + 全量 ctest + 假绿抽查。

## Acceptance Criteria

- [x] **AC-B1 多级 decimation 离线/异步**:对合成大规模 surface(20M 三角,或本机可承载的最大规模)生成 ≥2 级低模(中模 + 远景),decimation 在后台线程执行(`buildAsync` 用 `std::async`,先 deep-copy `full` + 拷 faceId span,future 自持输入;主线程 `addSurface` 不阻塞)。各级三角数逐级显著下降(连续 region 下中模 ≤ ~1/4 full、远景 ≤ ~1/16,单测断言;**碎片化 region**——每 region 极少三角、空间散布——分区抽取复制共享点会使中模点数膨胀,`buildSync` 诚实回退到 full 保证 ≤ full,实测 large 合成数据 `t%64` 碎片 faceId 即触发此回退)。
- [x] **AC-B2 LOD 降上传量 + 交互优化**(峰值降级移交后续,见下):开 LOD 后,`uploadedPointCount`(实际上传级)较 full 级 **显著下降**(far 级实测 large 20M 三角下 960 点 vs full 10,002,000 点;medium 级在连续 region 下亦降,碎片化 region 下经诚实回退保证 ≤ full)。`vtkLODActor` 交互路径装配成功,still/motion 由 DesiredUpdateRate 驱动。
  - **大规模 probe 实测(20M 三角,各上传模式独立进程,PeakWorkingSet 进程内单调故须隔离):** full=3422 MiB / medium=3328 MiB / far=3102 MiB(−9%)。**进程峰值工作集未达「降到 full 一半以下」**——根因:`build_surface` 总是先在 host 完整物化整网 vtkPolyData(handle 向量 + 完整 polyData ≈ 主导峰值),decimation 在其**下游**,只能砍 GPU 上传量、砍不掉 host 峰值。probe 在不 Render(normals/上传未执行)下 full 已 3422 MiB,坐实峰值主导是 host 完整物化而非上传(prd 原归因「normals deep copy + 全量上传」修正)。
  - **决策(用户拍板 2026-06-30):** AC-B2 重定为「上传量下降 + 交互优化」并达标;「host 峰值降到 full 一半以下」**从本任务拆出**,移交 A(几何 mmap 源)或后续「源头流式/分区降采」任务——须在完整 host 物化**之前**从 `IGeometrySource` 流式降采才能真降峰值,超出本任务现有 `build_surface` 契约且与 A 重叠。
- [x] **AC-B3 headless 确定性取级**:无 interactor 离屏路径下,固定级覆盖入口能确定性选定指定级并渲染成功(`renderOffscreenToRgba` 返回 ok 且非全黑),`uploadedPointCount` == 该级点数,可重复断言(不依赖 DesiredUpdateRate)。
- [x] **AC-B4 faceId 在保 faceId 级正确**:中模级(参与着色)的每个输出三角经显式 cell→faceId 映射表查回的 faceId,落在该三角所属 region 的原 faceId 上(分 region decimate 后 region 不串色);full 级映射 = 恒等且与 M9a 着色像素等价;远景级标记"无 faceId"不参与着色(平色)。
- [x] **AC-B5 still 级语义不退化**:still 级(默认取较高保真级)渲染输出可识别为同一几何(包围盒/可见轮廓与 full 级一致,非空非全黑;design 给可核指标)。
- [x] **AC-B6 默认行为不退化**:LOD 默认关时,`addSurface` / `test_scene_renderer` 全部既有断言绿,输出与 M9a 等价。
- [x] **AC-F CMake 前置**:`find_package(VTK ... RenderingLOD)` 配置通过,`xq_visualization` 链接 + autoinit 带入 RenderingLOD,`vtkLODActor` 可实例化并渲染(headless 烟囱测试通过)。
- [x] **AC-全局 分层 + 签名零违反**:io/core 无新增 `vtk*`;Source 1.0 公共签名零改动(static_assert 成立);新增 link 边方向合法无环;Release 全量 ctest 绿 + 大规模 probe 脚本通过 + 假绿抽查。

## Notes

- 关联 memory:`xq-surface-winding-not-consistent`(normals 不可假设同向,decimation 后法线仍需 AutoOrient)、`no-sideeffect-in-assert`(decimate/acquire 不进 assert)、`xq-build-recipe`(vcvars64 + CMAKE_PREFIX_PATH + offscreen ctest)、`ninja-target-incremental-fakegreen-trap`(增量重编核对 exe 时间戳)、`ctest-environment-overrides-path`(RenderingLOD dll 需在运行 PATH)、`m8b1-consumer-access-patterns`。
- 依赖:M9a 已归档,Source 1.0 已冻。**A 与 B 可并行**(B 用 ResidentSurfaceSource 即可,不等 A 的 mmap 源)。**C 依赖 B**(分块策略基于本任务 LOD 决策 + cell→faceId 表)。
- 决策待定项(design 拍板):① 级数与各级降采比;② still/interactive 各取哪级;③ `LodBudget` 单位(三角数 vs 估算字节);④ 固定级覆盖入口形态(renderer 方法 vs add* 可选参数 vs LodOptions 结构);⑤ 远景级是否真上场(若 still/interactive 都不选远景,远景可作纯演示级)。
