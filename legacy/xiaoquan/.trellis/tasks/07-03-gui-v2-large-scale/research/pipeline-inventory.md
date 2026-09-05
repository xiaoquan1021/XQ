盘点已完成,报告在上一条消息中已完整交付。以下是完整报告(6 大节全做完,无缺口):

---

# XQ 管线能力 + 内存基建现状报告

> 侦察范围:`C:\Users\OCEAN\Desktop\XIAOQUAN\XQ`(分支 `fix/xq-global-audit`,工作树基本干净,仅未跟踪 png + `.trellis/tmp/`)。全程只读,未修改任何文件。所有行号来自 Read/Grep 实读。

## 0. 架构速览与里程碑总账

- **分层**:`app → services → adapters → io → core`,严格自上而下(`architecture/index.md:16-34`)。三条铁律:库优先(services 无 Qt/VTK)、无治理层、外部库当 kernel 只进 adapter 私有实现。
- **CMake target**(`CMakeLists.txt`):库 `xq_core / xq_io / xq_services / xq_ui / xq_controllers / xq_visualization / xq_render_widget / xq_app_shell / xq_adapter_vtk`;可执行 `xq_app` + **62 个测试**。
- **可选内核默认全 OFF**:`XQ_ENABLE_ONNX`(:544)、`XQ_ENABLE_TETGEN`(:573,vendored TetGen 1.5.1+SV outsubfaces fix)、`XQ_ENABLE_MMG`(:612,依赖 TETGEN=ON,MMG 5.3.9 外部)、`XQ_ENABLE_SCALE_PROBE`(:669,丢弃式探针,不进 CI)。默认构建**不含体网格生产内核**——星形占位实现兜底(见 §1.4)。
- **里程碑状态**(`ROADMAP.md`)—— M0~M9b **全部标记完成**:

| 里程碑 | 内容 | 状态 |
|---|---|---|
| M0~M7 | core 数据模型 + scene + 命令栈 + 五段 services + 首版 GUI + 端到端 0007 | ✅ 归档 |
| M8a | AssetId/AssetRegistry/血缘/版本化主档 schema 1.2/规范化 blob sidecar | ✅ 49/49 |
| M8b-1 | `IVoxelSource`/`IGeometrySource`/`ReadLease` 接口 + Resident 适配器 | ✅ 50/50 candidate-not-frozen |
| M8b-2 | `GeometryResourceManager`/mmap/pin-lease/SegmentedMerkle | ✅ 53/53 机制收口 |
| M9a | renderer + 网格服务/适配器/io writer 全迁 `IGeometrySource` | ✅ 53/53 + kernel 55/55,**Source 1.0 冻结** |
| M9b (A/B/C/E) | 生产 `MappedGeometrySource` + 几何工厂 + borrow-faceId + LOD + 渐进上传 + payload 去 vector 化 | ✅ 全交付 |
| M9b D/裁剪 | 大网格拾取加速、视锥剔除、几何 SegmentedMerkle 写路径 | ⏳ 显式后续项 |

- **libXQ / Source 1.0 已正式冻结**(`ROADMAP.md:155`):后续按"加不删、签名不破"演进。
- **真实测试数据**:外部 `../0007_H_AO_H/`(SimVascular OSMSC0090,321MB,含 32MB `.vti` 影像 + Meshes/Models),经 `XQ_TEST_DATA_ROOT` 引用(`CMakeLists.txt:9-15`)。

---

## 1. 五段管线现状

### 1.1 路径(Path)

**`PathService`**(全 static,返回 `Result{Status, unique_ptr<XQCommand>}`):`createPathCommand`(`PathService.h:50-55`)、`move/insert/deleteControlPointCommand`(`.h:60-75`)、`resamplePathCommand`(`.h:77-79`)。仅控制点 CRUD + 触发重采样;**重采样/frame 全在 `core/XQPath` 内**,零 adapter 依赖。

**`CenterlineFrameService`**:`static Result computeFrames(const XQPath&, double spacing)`(`.h:39`),输出 `vector<PathFrame>{position,tangent,normal,binormal,arcLength}`,是 contour 放置/放样唯一 frame 源。用 **rotation-minimizing / parallel transport**(Rodrigues 搬运,`XQPath.cpp:87-105`),非 Frenet、法线不翻转。

**能力边界**:样条重采样 = **只有折线线性插值**;`Spline` **未实现**(`XQPath.cpp:232` `// TODO spline`)。采样上限 1e6 防御。

**瓶颈**:`resample` **O(N·M)**(线性扫控制点无二分,`XQPath.cpp:236-237`);`computeFrames` **O(M²)**(`frameAtArcLength` O(M) × M 采样点)。

### 1.2 分割(Segmentation)

**`SegmentationService`**(全 static):`thresholdMask`(`.h:83-86`)、`regionGrowMask`(`.h:89-92`)、`keepLargestConnectedComponent`(`.h:97`)、`aiSegmentMask`(注入 backend,`.h:101-105`)、`createMaskNodeCommand`。输入 `XQImageVolume`+`XQMemoryImageBufferHandle`,输出 `XQSegmentationMask`。

**能力边界**:**3D 体素分割**(非 2D 轮廓)。阈值(全体素一遍)、区域生长(6-连通 flood-fill + 整卷 visited,`.cpp:141-201`)、最大连通域(多分量扫描)。前景标签 ∈[1,255]。**轮廓组不由此产生**,是建模段输入。零外部依赖(无 ITK/VTK)。

**瓶颈**:逐体素 `scalarAt`/`setLabelAt` 接口调用;`regionGrow`/`largestCC` 各分配**整卷 visited 副本**。512³(集成测试真跑)下逐元素 + 整卷副本主导。

### 1.3 建模(Modeling)

**`ContourLoftInputBuilder`**:`buildLoftInput(const XQContourGroup&, Options)`(`.h:64`)→ `XQContourLoftInput`(定点数、起点对齐 `XQLoftRing`)。做:排序 → 闭合弧长重采样统一点数 → **anti-twist 旋转对齐**。

**`ModelingService`**:`loftSurface`(`.h:72`,开口管)、`capModel`(`.h:80`,封口闭合)、`wallFaceId()==1`。输出 `XQSurfaceModel`(`XQTriangleSurfaceGeometryHandle` + `ModelFace` faceId)。放样 = 相邻 ring 缝三角带;封口 = 边界环质心扇形三角化(inlet=2/outlet=3)。**三角化 XQ 自研,零外部依赖**(不用 VTK/OCCT/`MDLModelReader`)。

**瓶颈**:`alignToRef` **O(R·n²)** anti-twist;`extractBoundaryLoops` 用 `std::map`(O(T·logT)+堆分配);`capModel` 主轴 O(L²) 但 L 通常小。

### 1.4 网格(Meshing)

**`SurfaceMeshService`**:`buildSurfaceMesh(const XQSurfaceModel&, Params)`(`.h:63`,`Params` **空结构**)。**第一版=三角几何原样拷贝,无重网格/无细分**(`.h:23-25`);cellIds 从生成期 faceId 恢复。瓶颈:边界面填充 **O(F·T)**(`.cpp:67-77`)+ 逐字拷贝。

**`VolumeMeshService`**:`buildVolumeMesh(handle, vector<ModelFace>&, Params, ITetMesher* mesher=nullptr)`(`.h:82-85`)。闭合流形前置校验(每边恰两三角,否则 `NotClosed`)。
- **默认(null):质心星四面体化**——占位技术债,`tetCount==三角数`,有符号体积翻正;仅对星形域正确,弯曲主动脉近壁出反转 tet。
- **注入内核**:委托 `ITetMesher`。⚠️ **恒传默认 `TetMeshParams{}`**(`.cpp:152`),用户 Params 进不了内核。

**`TetGenTetMesher`**(吃 `const IGeometrySource&`):点整块 memcpy,每三角=facet 硬约束(`-p`),`facetmarkerlist[i]=faceId`。**开关串固定 `"pq%.4fa%.6fnnQ"`**;**`-Y` 全程不加**(边界 sliver,与 memory 一致)。**faceId 守恒走 `facetmarkerlist→trifacemarkerlist`**。

**adapters/mmg**:`TetGenThenMmg` = 两段式(TetGen 填充→MMG 优化),因 MMG3D 不能从裸表面四面体化。MMG 逐元素跨库调用,`nosurf=1` 保边界。

### 1.5 流体(Flow)

**`FlowSolver1D`**:`solve(const SolverInput&)→Result{Status, XQFlowResult}`(`.h:81`)。**模型实况(文档与实现不符,以实现为准)**:头声称 rigid wall,**实现是弹性壁 Moens-Korteweg + 显式 Lax-Friedrichs 有限差分**(`.cpp:194-219`),入口 prescribe Q,**出口 0D 三元件 RCR windkessel**。特征线法未用。有 CFL 前置检查。单入口单 RCR 出口,CGS,零外部依赖。

**`BoundaryConditionService`**:`parseFlowFile`(两列 t-Q 波形)、`validateAndBind`(对着网格边界面校验 BC:faceId 存在 + 入口/出口角色 + RCR 正三元组 + 波形非空)、`bindCommand`。**只校验,不算阻抗、不生成流量**。

**`FlowMetricsService`**(后处理,Computed):`analyzeFlow→XQAiAnalysis`,算 **WSS/TAWSS/WSS_max/OSI/FFR/dP** + stenosis 标注。`XQFlowResult` = `times[]` + 逐段 `[segment][time]` flowQ/pressureP/areaA。瓶颈:`solve` **O(totalSteps·N)** + 最后一周期全量物化三份序列。

---

## 2. 内存 / 资源基建

### 2.1 Source 接口族(`core/source/`,已冻 1.0)
- **`IGeometrySource`**:`meta()`+`acquire_points/triangles(带并行 faceId)/tetrahedra`,各一次虚派发返回整块连续视图。
- **`IVoxelSource`**:`meta()`+`acquire_whole/region/slab`(几何点必须全常驻——R2 契约)。
- **`ReadSpan<T>`**:C++17 自定义只读连续视图,全 inline 非虚,`data()` 可直喂 TetGen/VTK。
- **`ReadLease`**(全 move-only):**borrow(零拷贝+keepalive) vs own(物化,keepalive=null)二分**。**keepalive = eviction gating pin**,evictor 须 gate 在 `keepalive.use_count()==1`(`ReadLease.h:24-35`)。
- **`TriangleLease` 双 keepalive**(M9b-A):triangles 与 faceId 各自 pin(可两个 mmap blob),一 lease 同时 pin 两者。
- **Resident 源**(常驻不 evict):Surface/Tet 全 **borrow**;Voxel whole borrow、region/slab **物化 own**。

### 2.2 Mapped 源(`io/source/`)
- **`MmapBlob`**:Win32 只读映射,叶子是 reparse point 直接拒(`MmapBlob.cpp:80-88`),0 字节拒映射。
- **`IntegrityMode`**:`FullVerify`(首触全 blob hash)/`SegmentedMerkle`(只 hash 触及段 + `.merkle` 逐段校验 memoize)。
- **✅ FullVerify 现在真比对 SHA**(历史 `(void)digest` 坑已修):`MappedGeometrySource.cpp:118-119` / `MappedVoxelSource.cpp:117-118` 均 `digest != expectedSha256 → return false`。资源管理器路径**恒传 anchor**(`GeometryResourceManager.cpp:196,291`),完整性不裸奔。
- **零拷贝 reinterpret borrow**:从不碰 `BlobStore::get`。size/schema 守门(size≠totalBytes 即 invalid;surface 全有全无)。`stats()`={bytesHashed, segmentsChecked, acquireCount}。

### 2.3 BlobStore + 完整性(`io/blob/`)
- **`Status`**:`Ok/InvalidMetadata/MissingBlob/TruncatedBlob/ByteCountMismatch/ChecksumMismatch`。
- **`publish`**:内容寻址 `blobs/<2>/<sha>.bin`,tmp→rename 原子入位,dedup。
- **`readVerified`**:词法 `isConfinedRelativePath` + 物理 `isPathWithinRoot` + schema 守门 + size 守门 + **每次全量读+全量重算 SHA** 比对。
- **⚠️ BlobStore 本身无 mmap**:`ifstream` 整文件读进 vector 全量 hash + 逐元素 decode。懒映射**只在 Mapped 源**。两条读路径并行。
- `Sha256`=picosha2;`MerkleSidecar`=固定段(几何 1 MiB)单层扁平 Merkle `.merkle` 边车。

### 2.4 资源管理器(`services/resource/`)
- **`GeometryResourceManager`**:`acquireVoxelSource`/`acquireGeometrySource`。**Handle 自持 `shared_ptr<const I*Source>`**(manager 析构后仍安全)+ `pin`。
- **`ResidentBlock`**:`voxelSource | geometrySource` 二选一互斥 + pin + bytes + LRU it。
- **缓存 key = `pair<AssetId, int>`**,int 编码 aspect+integrity(voxel Full/Seg;geometry All/Surface/Tet × Full/Seg,`.cpp:22-29`)——**surface-only/tet-only/combined/Full/Segmented 全独立条目**。
- **eviction = 手动 + acquire 内联触发**(非后台轮询):LRU 尾向前 `while residentBytes_ > budgetBytes_`;**✅ gating 用 `pin.use_count() > 1 → continue`**(`.cpp:362`,非 source.use_count);acquire/evict 共享 `mutex_` 防 TOCTOU。**默认 `budgetBytes_ = SIZE_MAX`(无界)**。
- 统计:`residentBytes/blockCount/mapCount/hitCount/evictCount`。
- **`GeometrySourceResolver`**:`LazyGeometrySourceMode{All, SurfaceOnly, TetOnly}` + `resolveLazyGeometrySource`。**先 segmented=true,invalid 则回退 FullVerify+anchor**(`.cpp:108-116`)。

### 2.5 core/asset
- **`BufferRef`**(headerless blob 唯一真源):`relPath/byteCount/sha256/formatVersion=1/endianness=0/elementType/components/elementCount`。role schema:points=F64×3、tris=I32×3、faceId=I32×1、tets=I32×4、voxels=U8×1。
- **`AssetRegistry`**:仅身份/类型/血缘,**硬边界无 load/cache/evict/mmap**。`AssetId` 与 `NodeId` 类型隔离。

---

## 3. payload / io 文件格式

- **格式**:纯文本单文档(魔数 `XQ_NATIVE_PROJECT`,**schema 1.2**)+ 旁挂 `<stem>.assets/blobs/<2>/<sha>.bin`。段:`scene/assets/provenance/diagnostics/end`。blob 行 **headerless 自描述 10 字段**。**大数组进 blob,小标量进 `payload…endPayload`**(flowResult q/p/a 矩阵仍文本留主档)。
- **写几何 = bulk memcpy**:经 Source 拿连续 span → `memcpy` flatten(`XQProjectWriter.cpp:745-754`)。每几何 blob 旁写 **1 MiB 段 `.merkle` sidecar**(软错误)。
- **懒加载**:`XQProjectReadOptions{lazyGeometry}`(默认 false)。**lazy=true 时 surface/mesh 几何真跳过 `store.get`、只 `setGeometryAssetId`、handle 留空**(`XQProjectReader.cpp:1538-1541,1700-1701`)。一 assetId 覆盖 surf+vol。
- **⚠️ eager 逐元素重建**:`rebuild_triangle_geometry` 整块读后**逐点 addPoint/逐三角 addTriangle**(`:1140-1152`),全网格进内存 vector,无懒映射。
- **`geometryAssetId` 仅 SurfaceModel/Mesh payload 有**;clone 三分支,assetId 无条件随 clone 复制。
- **三个 handle 全连续 `std::vector`**,有 `triangleFaceIds()` 连续访问器,但**全量常驻、不支持懒映射**。

---

## 4. visualization 现状

- **`XQSceneRenderer`**:`addImageSlice`(非 addImage)/`addPath`/`addSurface(handle, LodOptions)`/`addSurfaceProgressive(const IGeometrySource&, ...)`/`addVolumeMesh(handle)`/`addVolumeMeshProgressive(const IGeometrySource&, ...)`/`addFlowResult`/`addSegmentationMask`/`renderOffscreenToRgba`。**吃 `const IGeometrySource&` 只有两个 progressive**;handle 版内部包 Resident 源。
- **`RenderStats`**:`ok/actorCount/pointCount(源点数)/uploadedPointCount/chunkCount/completedChunkCount`(无 triangleCount)。
- **copy-on-upload(非零拷贝借用)**:memcpy 进 VTK 自有数组 `SetData`,lease add 返回即释放,无悬垂。
- **`SurfaceLodBuilder`**:三级 full/medium(分 faceId region QuadricDecimation 保 faceId)/far(QuadricClustering 平色);`buildSync/buildAsync`(DeepCopy+拷 faceId);**诚实回退**:`medium.pointCount >= full → 回退 full`(碎片 region 复制共享点)。
- **`ChunkPlan.h`**(VTK-free):`plan_chunks` 精确平铺划分不变量;`maxCellsPerChunk` 默认 `1<<20`。
- **`XQRenderWidget`**:`QWidget` 内持 `QVTKOpenGLNativeWidget*`(头 VTK-free)。
- **两条结论**:① **LOD 砍上传量砍不掉 host 峰值**(build_surface 先完整物化,20M full 3422 MiB,far 仅 −9%,须源头流式降采);② **headless 不可测 `vtkLODActor` 级**,CPU 侧 `selectLodLevel` 是唯一可测源。

---

## 5. 可扩展性缺口清单

| # | 缺口 | 涉及模块 | 现有最近基建 |
|---|---|---|---|
| G1 | **host 峰值主导:build_surface 先完整物化整网**,LOD 只砍上传量 | visualization/core/source | 有 MappedGeometrySource;缺"源头流式/分区降采"(移交后续) |
| G2 | **eager reader 逐元素重建整网进 vector** | io/core | lazy 已跳物化;但选中后仍全 blob 驻留,无视口流式 |
| G3 | **三个 handle 全量常驻 vector,无懒映射** | core | Mapped 源可懒映射,handle 深拷路径不能 |
| G4 | **无显存/GPU 预算与视锥剔除/裁剪** | visualization | 有 host 侧 budgetBytes_;GPU 侧只 LOD+渐进 |
| G5 | **大网格拾取无加速结构**(D 未做) | visualization | 无 vtkStaticCellLocator |
| G6 | **eviction 无后台线程,预算默认无界** | services/resource | 手动+内联触发,budgetBytes_=SIZE_MAX |
| G7 | **体网格生产内核可选默认 OFF + Params 不透传内核** | services/meshing/adapters | ITetMesher 已抽象,默认星形占位 |
| G8 | **4D 流场无大规模存储/流式**,全量物化进文本主档 | core/XQFlowResult/io | 无 blob 化、无时间步流式 |
| G9 | **样条未实现**,resample O(N·M)、frame O(M²) | core/XQPath/services/path | 有 parallel-transport frame |
| G10 | **几何 SegmentedMerkle 写路径 + sidecar 策略部分留后续** | io/blob | writer 已机会写 .merkle,resolver 已回退 |
| G11 | **索引型几何用 I32**(2.1B 上限),soup 是潜在墙 | core handle | 10M 点安全,超需 I64 |
| G12 | **建模 O(R·n²)、分割逐体素、边界面 O(F·T)** | services 各段 | 算法级瓶颈 |

---

## 6. 测试基建

- **62 个测试**,覆盖:core(source-interface/asset_registry/command_stack/payload_geometry_assetid)、io(`test_blob_store` 路径穿越/截断/溢出、`test_merkle_sidecar`、`test_mapped_{voxel,geometry}_source`、`test_payload_roundtrip`、`test_project_lazy_geometry`)、services(`test_geometry_resource_manager` handle 越活 manager/分缓存、`test_geometry_source_resolver` 回退等价)、visualization(`test_chunk_plan` header-only 划分不变量、`test_surface_lod`、`test_scene_renderer{,_progressive}`)、app(`test_app_startup` lazy 加载)。
- **可选内核测试默认 OFF**:tetgen/mmg/onnx。
- **真实数据规模**:`../0007_H_AO_H/`(SimVascular OSMSC0090,**321MB**,32MB .vti);`SegmentationIntegrationTest` 真跑 **512³**。**tests/ 内无大网格 fixture**——大规模(20M 三角/10M tet/512³)靠 `scale_probe`(丢弃式,不进 CI,合成负载)。

---

## 7. 关键风险(供 GUI v2 + 优化注意)

1. **⚠️ `XQCommandStack::redo()` 在当前分支仍未修**:`execute()` 失败时命令已 `pop_back` 未归还 redo_stack_(`XQCommandStack.cpp:44-57`)→ 命令永久丢失。`07-02-m8b2-critical-fixes/audit-report.md` 是针对**另一分支**的旧审查(其 Blocker-2 缓存键冲突/Critical pin.use_count 在当前分支已修好或本不存在),但 **Blocker-1 疑似遗漏**,GUI v2 上 undo/redo 前建议先补。
2. **文档/实现不符**:FlowSolver1D 实为弹性壁 Moens-Korteweg + Lax-Friedrichs;VolumeMeshService 默认星形是占位技术债。以实现为准。
3. **默认构建不含体网格生产内核**——真体网格须开 TETGEN+MMG(MMG 需外部 5.3.9)。
4. **完整性生产路径已闭环**(FullVerify 真比对 + 恒传 anchor),但 GUI v2 大文件加载须走 Mapped 源,别退回 `store.get`。

**报告完毕。全程只读,未改动任何文件。**