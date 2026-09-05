# M9b-E payload 去 vector 化(并存 assetId 惰性驻留路径)

## Goal

让 reader **不必整块 load 几何 blob 进 payload vector**,从而降两笔内存账:① payload 常驻一份完整 points/tris/faceId(或 tets)vector;② `BlobStore::get` 整块解码期的瞬时 ~2× 双份。做法**已拍板 = 并存惰性路径**:

- `XQSurfaceModelPayload` / `XQMeshPayload` 增**可选 assetId 字段**,标记"几何走惰性,从该 asset 经 `GeometryResourceManager::acquireGeometrySource`(M9b-A 产出)按需取 `IGeometrySource`"。
- 与**现有 resident handle 路径并存**:不物理删 payload 里 `XQTriangleSurfaceGeometryHandle` / `XQTetVolumeMeshHandle` 字段、不破 `clone()` 语义、随时可回退到整块 load(老工程、无 manager、未开惰性选项时)。
- 消费者(渲染/网格服务/writer)从"收 handle"**渐进迁**到"能向 payload 要几何源":持 handle 的照旧,持 assetId 的经 manager 惰性解析。本子任务只把基座 + reader opt-in + 一处消费者打通,不强制全量迁移。

**只降线2(reader 常驻 + 瞬时双份),不碰线1(GPU 上传峰值)**——后者是 B/C 的事(父 prd §Goal 两线分账)。

## Background(为何现在做 + 调研依据)

- **这是 M8b-2 / M9a 显式 defer 项**(memory `m8a-wholefile-sha-vs-streaming`):`BlobStore` 整文件 SHA + `get()` 逐元素解码与懒映射结构性冲突,M9a 当时只迁消费者读路径、不改 payload 持有结构(M9a prd §Out of scope "SceneNode 去大型 vector 化 ... 留 M9b")。**现在做的理由**:M9b-A 已建好 `MappedGeometrySource`(map→verify→reinterpret 零拷贝)+ `acquireGeometrySource` 工厂 + borrow-faceId。惰性路径的"取数后端"已就绪,E 只需把 payload→assetId→manager 这条解析链接上,**顺势成本最低**;脱离 A 单独做要自建 mmap 源,成本翻倍。
- **现状实读**:reader `rebuild_triangle_geometry`(`XQProjectReader.cpp:966-1012`)对每个 surface/mesh 几何 `store.get` 出 `vector<double> points` / `vector<int> tris` / `vector<int> faceIds` 后**逐元素** `addPoint` / `addTriangle` 灌进 handle;`rebuild_mesh` 的 tet 分支(:1517-1539)同样整块 get + 逐元素 `addPoint`/`addTet`。payload 此后**长期持有**这份 handle vector。writer 侧(`put_payload_blobs:774-835`)已用 blob role 写出几何(surface=`points`/`tris`/`faceId`,mesh surf=`surfPoints`/`surfTris`/`surfFaceId`,vol=`volPoints`/`tets`),**惰性取数所需的 blob 已在 asset registry 里有 BufferRef**,无需新写格式。
- **分层硬约束实测**(`CMakeLists.txt:240-242`):`xq_services` 链 `xq_io` 为 **PRIVATE**,io 永不依赖 services。故 **reader(io)不能调 `GeometryResourceManager`(services)**。惰性"解析"只能落在 services/app;reader 侧只负责"记下 assetId + 不物化几何"。这条决定了整个架构形态(见 design 决策 1)。
- **完整性默认 FullVerify**(父 prd §Background:writer 不产几何 `.merkle`):惰性取的几何源走 A 的 FullVerify,无 sidecar 依赖,老工程几何 asset 不会静默 acquire 失败。

## Scope

### In scope
- **payload assetId 字段(core)**:`XQSurfaceModelPayload` / `XQMeshPayload` 各加可选 `geometryAssetId`(`AssetId`,core 已有)+ `setGeometryAssetId` / `hasGeometryAssetId` / `geometryAssetId` 访问器。纯追加,不动既有构造/`model()`/`mesh()`/`domainType()` 签名。
- **clone() 语义显式化(core)**:① 持 resident handle 的 payload — clone 照旧深拷 handle(现行为不变);② 持 assetId 且**无** resident handle 的 payload — clone 只拷 assetId 引用,**不深拷几何**(本就没有);③ 二者都有(理论态)— 以 handle 为准深拷 + 带上 assetId。**不引入 copy-on-write**,只在 clone 内按"有无 handle"分支。
- **reader 惰性 opt-in(io)**:新增读选项(`XQProjectReadOptions{ bool lazyGeometry = false }` + `load` 重载,"加不破")。当 `lazyGeometry==true` 且 asset 携带几何 blob 且绑定到节点时,reader **跳过 `rebuild_triangle_geometry` / tet 整块 get**,改为在 payload 上 `setGeometryAssetId(asset.id)`(几何 handle 留空);否则**回退整块 load**(默认路径,与今天逐字节一致)。io 不引入 services 依赖。
- **services 惰性解析助手(services)**:新增 `acquireGeometrySource(const XQPayload&, GeometryResourceManager&, ...)`(或等价自由函数/方法),把"payload 持 assetId → `manager.acquireGeometrySource(assetId, spec)` → `IGeometrySource` 句柄"封一处。spec(点/三角/tet 计数等)从 A 的 `GeometrySourceSpec` 取(见 design 决策 3 的 spec 来源)。
- **一处消费者打通 + 等价验证**:至少在 services 层证明"惰性路径取到的 `IGeometrySource` 的 points/tris/faceId(/tets)span 与整块 load 出的 handle 逐元素相等";渲染消费者(`XQMainWindow.cpp:275-292`)的迁移**视 A 的 renderer-source 入口就绪度**接入(否则留 Notes 标 follow-up,不阻塞 E 验收)。

### Out of scope(明确划界)
- **物理删 payload 的 handle vector 字段** → 父 prd §Out of scope 明定不做;E 走并存,字段保留可回退。
- **消费者全量迁 `IGeometrySource&`**(renderer/网格服务/writer 全改吃源)→ 渐进,非本子任务;E 只打通解析链 + 一处消费者。
- **renderer `addSurface(const IGeometrySource&)` 新入口** → 属 A/B(renderer 侧);E 不改 renderer 公共签名。
- **几何 sidecar 写路径 / SegmentedMerkle** → 父级后续项;E 惰性源默认 FullVerify。
- **writer 改造**:writer 已用 blob role 写几何,E 不改写出格式;writer 是否消费惰性源(避免 save 时整块物化)留 follow-up。
- **node 级 assetId 语义变更**:`XQDataNode` 已有 node→asset 绑定(`XQDataNode.h:40-51`),E 不改其语义;payload 的 geometryAssetId 是 payload 自带、为 clone 存活与几何解析服务。
- 不复用 `XQ/probe/` 弃码。

## Constraints
- **不破已冻 Source 1.0 公共签名**:`IGeometrySource` 3 纯虚(`IGeometrySource.h:19-26`)不加纯虚;惰性解析复用 A 的 `acquireGeometrySource` + `MappedGeometrySource`,E 不新增 Source 接口纯虚。`ReadLease`/`TriangleLease` 签名不改(borrow-faceId 是 A 的事)。static_assert 仍成立。
- **分层不变**:payload assetId 字段在 core(`AssetId` 已 core);reader opt-in 在 io,**io/core 不依赖 services、不出现 `vtk*`**;惰性解析助手在 services(可见 manager + io 的源,符合现有 `services→io` PRIVATE 边)。新增 link 边方向合法无环。
- **clone 语义不破**:现有"持 handle 的 payload clone 深拷几何、与原对象不共享可变几何"必须保持(edit-style 命令依赖,见 payload.h 注释);assetId 分支只是新增不破旧。
- **向后兼容老工程**:`lazyGeometry` 默认 `false`;无 manager / 无 assetId / 老归档(无 assets 段)一律走整块 load,载入结果与今天逐字节一致(round-trip 测试不退化)。
- **最小改动**:不加没要求的兜底/降级分支(CLAUDE.md 铁律);惰性是显式 opt-in 叠加,不偷偷改默认行为。
- **依赖 A**:必须在 M9b-A(`MappedGeometrySource` + `acquireGeometrySource` + `GeometrySourceSpec`)合入后开工;A 的 spec 形态最终决定 E 的解析助手入参。
- 验收铁律:Release + 全量 ctest + 假绿抽查;`acquire_*` / `store.get` 等副作用调用不进 assert(`/DNDEBUG` 删掉=假绿 segfault,memory `no-sideeffect-in-assert`)。

## Acceptance Criteria
- [x] **AC1 payload assetId 字段(加不破)**:`XQSurfaceModelPayload` / `XQMeshPayload` 有可选 `geometryAssetId`,`hasGeometryAssetId()` 默认 false;既有构造/`model()`/`mesh()`/`domainType()` 签名零改动,既有测试不改即过。 — `test_payload_geometry_assetid::test_field_additive_default_absent`;既有 `test_payload_and_groups`/`test_payload_roundtrip`/Modeling/Meshing 测试不改即过。
- [x] **AC2 clone 语义正确**:持 handle 的 payload clone 仍深拷几何(原/克隆不共享可变几何,沿用 payload.h 既有断言);持 assetId 无 handle 的 payload clone 只拷 assetId、不分配几何;克隆后 `hasGeometryAssetId()` / `geometryAssetId()` 与原一致。 — `test_clone_handle_deep_copy_not_shared`/`test_clone_assetid_only_no_geometry_alloc`/`test_clone_handle_and_assetid_both`/`test_mesh_clone_carries_assetid`;假绿抽查:clone 不带 assetId → 转红。
- [x] **AC3 reader 惰性不强制常驻**:`lazyGeometry==true` 载入带几何 asset 的工程后,对应 payload `hasGeometryAssetId()==true` 且**未物化几何 handle**(`hasTriangleGeometry()`/`hasVolumeTets()`==false),reader 未对该几何 blob 调 `store.get`(可测:无整块解码)。 — `test_project_lazy_geometry::test_lazy_stamps_assetid_no_materialize` + 决定性 `test_lazy_skips_blob_get`(删光 blob 后 lazy 仍成功、eager 失败,反证 lazy 不读几何 blob);假绿抽查:让 lazy 仍物化 → 转红。
- [x] **AC4 惰性路径取源正确**:经 services 助手 `payload.geometryAssetId → manager.acquireGeometrySource → IGeometrySource`,其 `acquire_points()/acquire_triangles()/acquire_tetrahedra()` 的 span 与"同工程整块 load 出的 handle"逐元素相等(points/tris/faceId/tets 全等)。 — `test_geometry_source_resolver::test_lazy_resolver_equivalent_to_eager`(surface points+tris、mesh surf + tet 各比对应 eager handle,逐元素全等;副作用 acquire_* 先取再断言);假绿抽查:resolver triCount-1 → 句柄无效 → 转红。
- [x] **AC5 resident 旧路径并存不退化**:`lazyGeometry==false`(默认)载入与今天逐字节一致;持 handle 的 payload 经现有 `ResidentSurfaceSource`/`ResidentTetSource` 取数路径不变;round-trip / 既有渲染、网格测试全绿。 — `test_eager_default_materializes`;`test_payload_roundtrip`/`test_project_roundtrip`/`test_project_versioned_save` 默认档不退化(60/60 绿)。
- [x] **AC6 向后兼容老工程**:无 assets 段的老归档、无 assetId 的 asset、无 manager 的调用方一律走整块 load,无新报错、无行为变化。 — `load(path,out)` 旧重载转发 `{}`(eager),调用方零改;无几何 blob 的 asset 走原路径;`resolveLazyGeometrySource` 对无 assetId payload 返回无效句柄(`test_no_assetid_resolves_invalid`),调用方回退 resident。
- [x] **AC7 分层 + 签名零违反**:io/core 无 `vtk*`、无 services 依赖;Source 1.0 公共签名零改动(static_assert 成立);新增 link 边方向合法无环。 — `rg` 核 io/core 无 `vtk`/`services/` include;`IGeometrySource` 仍 meta + 3 acquire 纯虚未动;resolver 落 services(可见 manager + core payload),无新反向边。
- [x] **AC8 全量绿 + 假绿抽查**:Release 全量 ctest 绿(新增本子任务测试计入);篡改一处源/blob 数据令对应等价断言变红(`acquire_*`/`get` 不进 assert)。 — Release 全量 **60/60 绿**(新增 test_payload_geometry_assetid / test_project_lazy_geometry / test_geometry_source_resolver);三处假绿抽查(clone 丢 assetId、lazy 仍物化、resolver 错 triCount)均实测转红再还原;副作用 acquire_*/get 取出后再断言。

## Notes
- 复杂改动面,`task.py start` 前补 `design.md`(架构 + 关键决策:① 惰性解析为何落 services 而非 reader;② payload assetId 字段 vs 复用 node assetId;③ spec 来源 与 A 的 `GeometrySourceSpec` 对接;④ clone 分支)与 `implement.md`(分阶段:payload 字段+clone → reader opt-in → services 解析助手+等价测试 → 消费者打通)。
- **依赖**:M9b-A(`MappedGeometrySource` + `acquireGeometrySource` + `GeometrySourceSpec` + borrow-faceId)必须先合入。A 的 spec 形态最终决定 E 助手入参,开工前对齐 A 的 design。
- **改动面最大、放最后**(父 prd §依赖序):配全量 ctest;reader 改造触碰 surface/mesh 两条 rebuild 路径 + 读 API 重载,ripple 到所有 `XQProjectReader::load` 调用方(默认参数保兼容)。
- 关联 memory:`m8a-wholefile-sha-vs-streaming`(本子任务根由)、`m8b1-consumer-access-patterns`(消费者访问粒度)、`xq-build-recipe`、`no-sideeffect-in-assert`、`xq-section-header-peek-needs-first-token`(reader 段解析坑)、`commit-check-gitignore-present`。
- 完成后:线2(reader 常驻 + 瞬时双份)在 opt-in 下被打掉;父里程碑 A/B/C/E 中改动面最大一项收口。

## 收口记录(2026-06-30)
- 阶段1/2/3 各独立 commit(payload 字段+clone / reader opt-in / services resolver),Release 全量 **60/60 绿**,三处假绿抽查均实测转红再还原。
- **消费者渲染接入按 prd In scope 明定留 follow-up**:本子任务在 services 层证 AC4 取源逐元素等价即收口;`XQMainWindow` 渲染 dispatch 迁到"持 assetId → 经 resolver 取 `IGeometrySource` → renderer"需 renderer 吃源公共入口(属 A/B renderer 侧),未就绪不阻塞 E。惰性解析链(payload→assetId→manager→MappedGeometrySource)已端到端打通且等价验证。
- spec 已落 `XQ/spec/.../payload-lazy-geometry`(见 spec 目录)。
