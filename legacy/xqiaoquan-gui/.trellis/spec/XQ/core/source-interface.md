# Source 只读访问接口(M8b-1,consumer-facing read contract)

> 来源:M8b-1(`06-30-source-interface`)。消费侧(services / io / visualization / adapters)访问几何与体素数据的稳定只读契约。写"消费 voxel/triangle/tet 数据"的代码前先读本篇。

---

## 为什么存在

三种真实几何 handle(`XQMemoryImageBufferHandle` 体素、`XQTriangleSurfaceGeometryHandle` 三角面、`XQTetVolumeMeshHandle` 四面体)被 5 个子系统消费。理解阶段全量调研结论:**消费侧 100% 只读,核心诉求是"整块一次拿到"**,逐元素访问(`scalarAt`/`triangleFaceId(i)`/稠密三重循环)是必须消灭的虚调用热点。

Source 接口把这些散落的访问收敛到统一只读契约之后,使后续可替换底层驻留策略(内存 / 磁盘 / 懒加载)而不动消费者。

## 核心定调(一句话)

**每块一次虚调用,返回整块连续只读视图。** 虚边界只落在 `acquire_*`(每次一次虚派发);视图内部访问全部 non-virtual inline。**逐元素虚调用零容忍。**

## 形态(`core/source/`,零外部依赖,C++17)

- `ReadSpan<T>`:只读连续视图(`data()/size()/operator[] const`),全 inline 非虚。C++17 无 `std::span` 故自定义。`data()` 暴露连续指针 → 可直喂外部库连续数组(TetGen `REAL[3n]`、VTK 标量缓冲)。
- `IVoxelSource`:`meta()` + `acquire_whole/region(extent[6])/slab(z)`。region/slab 是 voxel 专属(服务 ONNX roi / 流式渲染);triangle/tet 无窗口化需求。
- `IGeometrySource`:`meta()` + `acquire_points/triangles(+并行 faceId)/tetrahedra`。
- `ReadLease`(`VoxelLease`/`GeometryLease<T>`/`TriangleLease`):RAII 只读借用句柄,**move-only**。

## 两条不变量(改 Source 代码必须守)

1. **借用 vs 物化二分**:底层有连续 vector 访问器的(points/triangles/tets/voxel-whole)→ 零拷贝借用,`ReadSpan` 指向 handle 的 `vec.data()`;没有的(faceId 只有 `triangleFaceId(i)`、region/slab 子块)→ 在 `acquire_*` 内**一次性**物化。物化是一次 O(N),非虚,不违反零虚调用。
2. **租约 = 驱逐 gating pin**:`borrow` 持 `shared_ptr<const void> keepalive`(handle 的 shared_ptr 隐式转),`own` 把缓冲存进 lease 自身的 `vector`(move 保持 data 指针不变)。租约存活期间 span 指针稳定有效;释放后视图失效。keepalive 不只是"延寿":它是**驱逐 gating pin**——任何可驱逐存储(mmap / 预算 / LRU 驱逐)的 `IVoxelSource`/`IGeometrySource` 实现者,其 evictor **必须**在 `keepalive.use_count() == 1`(无活跃 lease 持有拷贝)时才回收底层。borrow lease 存活即 pin 住底层不可驱逐;own lease 自持 buffer(keepalive 为 null)免疫驱逐。Resident baseline(常驻、永不驱逐)不受影响。

## 协作式 use_count gating(可驱逐存储实现者的硬约束)

可驱逐存储的实现者(典型:io 的 `MappedVoxelSource` + services 的资源 manager)必须遵守协作式 `use_count` gating,这是 M8b-2 定稿的并发契约,**lease/接口的公共 C++ 签名全部冻结不变,机制全落实现者侧**:

- **keepalive 所有权收敛**:每个可驱逐驻留块的 keepalive 控制块所有权收敛到实现者(manager)独占基线——除 manager 持的那一份 + 活跃 lease 拷贝外,禁止第三方长持该 `shared_ptr`,否则 use_count 永 >1 无法驱逐。
- **evictor honor use_count==1**:evictor 只在目标块 `keepalive.use_count() == 1` 时回收;`>1` 表示尚有 live lease(被 pin),必须跳过。
- **共享锁防 TOCTOU**:acquire(注入 keepalive、抬 use_count)与 evict(判定 use_count==1 后回收)必须共享同一把锁,杜绝"判定==1 后又被新 acquire 抬到 2 才回收"的悬空。
- **并发≠可拷贝**:多线程各自 `acquire_*` 各得独立 move-only lease(各拷一份 keepalive,shared_ptr 计数原子),move-only 语义保持冻结。


## 约束 / 边界

- 落 `core` 最底层(被 services/io/visualization/adapters 共同消费),零外部依赖(无 VTK/TetGen/Qt)。
- 错误语义沿用 core "explicit result, no exceptions through UI":越界 region/slab 返回 `valid=false` 的空 lease,不抛异常。
- 一个 source 实例可只持部分几何(surface 无 tet;tet 无 triangle):缺失种类 `meta()` 报 count=0、`acquire_*` 返回空 lease,**不是错误**。
- **只读**:`ReadLease`/`ReadSpan` 无任何写回 API。写回 / ingest / 重建走独立批量构造路径(M8b-1 不做)。

## 演进边界

- M8b-1 是纯增量:**不改三种 handle 的任何 API、不迁移任何现有消费者、不动驻留行为**。faceId 走物化而非给 handle 加访问器。
- 后续(M8b-2+)才把现有消费者迁到 Source、接入真实存储驱动;届时补两处已知测试缺口:AC6 no-alloc 独立断言、多分量(components>1)体素路径。

## 消费者迁移落地(M9a,`06-30-m9a-source-migration`)

M9a 把 renderer + 网格服务/适配器/io writer 全部迁到 `IGeometrySource` 边界,驻留路径(`ResidentSurfaceSource`/`ResidentTetSource` 包现有 handle)。写"消费几何"的新代码照此模式:

- **调用点包源**:消费者拿到 `XQTriangleSurfaceGeometryHandle&`/`XQTetVolumeMeshHandle&`(栈引用或 shared_ptr)后,构造 `ResidentSurfaceSource`/`ResidentTetSource` 传给下游。栈引用用**别名 shared_ptr**(`std::shared_ptr<const T>(std::shared_ptr<const void>(), &ref)`,no-op deleter):源不拥有 handle、不延寿,适用于"消费者不持有 handle 生命周期"的场景。
- **连续整块喂外部库**:TetGen pointlist 用 `acquire_points().span().data()` + `std::memcpy` 喂 `REAL[3n]`(`static_assert(sizeof(Point3)==24)` 在 cast/memcpy 点);facetmarker 走 `TriangleView::faceIds` 并行 span;tet/tri 连接走 `SourceTet`/`SourceTriangle`(`array<int,4/3>`)整块 reinterpret。io writer flatten 同理(memcpy 整块,不再逐元素 push)。
- **renderer = copy-on-upload(关键坑)**:renderer 的 VTK 管线(`vtkPolyDataNormals`/`vtkGeometryFilter`/`vtkExtractEdges`)经 `GetOutputPort()` 连接,**在 render 时惰性执行**,而 actor/mapper 被 renderer 长期持有、没有地方存 lease。所以**不能零拷贝借用**(`SetArray(save=1)` 会悬垂);必须从 lease span **拷进 VTK 自有数组**(`vtkDoubleArray::SetNumberOfTuples`+memcpy / `vtkCellArray::SetData(offsets+int32 connectivity)` / `SetCells(VTK_TETRA)`),lease 在 `build_*` 返回即释放。这消除了逐元素 `SetPoint`/`InsertNextCell`/per-tet `vtkNew<vtkTetra>` 分配风暴(探针实测瓶颈),又不引入悬垂。零拷贝借用留待 M9b 的 mmap 几何源 + 渐进上传(届时数据不驻留、借用才有内存意义)。
- **出范围(查实)**:flow 全链路不碰几何(`FlowSolver1D` 吃标量、`addFlowResult` 无几何);MMG 适配器(`MmgVolumeRemesher` 内核逐元素 set、且吃上一阶段产出 handle 非 project Source);`path/`(只用 `Point3` 数学)——三者不迁。
- **接口零改动**:M9a 全程未改 Source 公共签名(static_assert 仍成立),纯消费者迁移。faceId 借用变体(`TriangleLease` 现每次 O(N) 物化)留待真大规模(20M 三角)成热点时再加。
- **仍缺(留 M9b)**:生产级 `MappedGeometrySource`(磁盘 mmap + 完整性)+ 几何资源管理器工厂(`acquireGeometrySource`:assetId→source/预算/驱逐)——体素侧已有 `acquireVoxelSource`,几何侧是缺口。
- **冻结门消费者存活证据**:renderer/网格/持久化均通过 Source 边界工作且不退化(53/53 + kernel 55/55,round-trip 字节守恒)→ libXQ/Source 1.0 候选可正式冻结。

## 大规模懒驻留基座(M9b-A,`06-30-m9b-a-geometry-mmap-source`)

M9b-A 补齐 M9a 留的两个几何懒驻留缺口,全部"加不破"(Source 1.0 公共签名零改动):

- **`TriangleLease` borrow-faceId 零拷贝**(`ReadLease.h`):新增第二 keepalive 成员 `faceIdKeepalive_` + 新静态工厂 `borrow_borrowed_faceids(triKeepalive, triangles, faceIdKeepalive, faceIds_span)`。triangles 与 faceId 可来自不同后备(两个 mmap blob 或同一 handle),**各自 pin**,一个 live lease 同时 pin 两者。原 `borrow`(物化路径)一字不改;消费者读 `view().faceIds` 是连续 span,borrow-vs-own 对其透明。`XQTriangleSurfaceGeometryHandle` 加 `triangleFaceIds()` 连续访问器(faceId 本就连续存储),`ResidentSurfaceSource::acquire_triangles` 改走 borrow 消灭每次 O(N) 物化。
- **生产 `MappedGeometrySource`**(`io/source/`,实现 `IGeometrySource`):照 `MappedVoxelSource` 范式,多 blob(points/tris/faceId 或 volPoints/tets)各自 `map_file_readonly` + size/类型守门 + 逐 blob 惰性 memo 校验 + reinterpret 零拷贝 borrow(keepalive=各 blob 的 mmap 控制块)。`acquire_triangles` 走 borrow-faceId 两端零拷贝。完整性支持 FullVerify 与 SegmentedMerkle;writer 已为几何 blob 机会性写 `.merkle`,resolver 应优先 SegmentedMerkle,缺/坏 sidecar 时回退 FullVerify。部分源(只表面/只体)缺 role 返回 empty lease。
- **`GeometryResourceManager::acquireGeometrySource`**(services):与 `acquireVoxelSource` 对称。`ResidentBlock` 泛化为 `voxelSource | geometrySource` 二选一互斥,共用 pin/bytes/LRU/use_count gating;几何 blob 按 role(points/tris/faceId 或 surfPoints/surfTris/surfFaceId + volPoints/tets)解析,**预算 = 整 asset 几何 blob 字节求和**。头只见 core 抽象,io 的 `MappedGeometrySource` 在 .cpp 构造(沿用 `services→io` PRIVATE 边)。
- **仍缺(留 M9b 后续 / E)**:reader 惰性 opt-in + payload 持 assetId(E)。M9b-A 只交付"取数后端 + 工厂",不改 payload/reader;几何 sidecar 写路径与 resolver 策略已作为后续修复落地。

## payload 去 vector 化 / 惰性几何驻留(M9b-E,`06-30-m9b-e-payload-devector`)

M9b-E 接上 A 的"取数后端",让 reader 在 opt-in 下**不整块物化几何进 payload handle**,改记一个 assetId 由 services 惰性解析。全程**并存、加不破**:

- **payload 可选 `geometryAssetId`**(core,`XQSurfaceModelPayload`/`XQMeshPayload`):纯追加成员(`AssetId` + bool,默认 absent),既有构造/`model()`/`mesh()`/`domainType()` 签名零改。`clone()` 三分支——持 handle 深拷(原行为)、仅 assetId 只拷引用零几何分配、二者都有以 handle 为准深拷且带上 assetId;assetId 无条件随 clone 复制(轻量引用)。一个 mesh 的 geometryAssetId 覆盖该 asset 的 surf + vol 两组 blob。
- **reader 惰性 opt-in**(io,`XQProjectReadOptions{lazyGeometry}` + `load` 重载):旧 `load(path,out)` 转发 `{}`(eager),调用方零改。`lazyGeometry==true` 且几何 blob 在场时,surface/mesh rebuild **跳过整块 `store.get`**,改 `setGeometryAssetId(asset.id)`、几何 handle 留空;否则回退整块 load(逐字节不变)。io 不引 services/vtk;**不动文本段解析顺序**(blob 行照常进 registry,只是不物化——避免 `xq-section-header-peek` 类假 ParseError)。
- **services 惰性解析助手**(services,`resolveLazyGeometrySource(payload, manager, registry)`):取 payload 的 geometryAssetId,从 registry 的 `BufferRef.elementCount` 反推 `GeometrySourceSpec` 计数(**manager 不自推,计数是调用方必填**——实读 `GeometryResourceManager.cpp:198-206`),先以 `spec.segmented=true` 调 `acquireGeometrySource` 使用 writer 产出的 `.merkle`,失败后以 `spec.segmented=false` 回退 FullVerify。无 assetId / 未知 asset / 无几何 blob → 无效句柄,调用方回退 resident 路径。
- **分层硬约束**:`services→io` 是 PRIVATE 边,**reader(io)物理上不能调 manager(services)**。故惰性"解析"只能落 services;reader 只"标记 assetId + 不物化"。这条决定了形态:io 管"从盘解析结构 + 标记几何来源",services 管"按需驻留 bytes"。
- **验收关键(踩坑沉淀)**:① AC3"未调 store.get"不靠探针,靠**删光 blob 文件后 lazy 仍成功、eager 失败**反证(决定性);② AC4 等价对比把副作用 `acquire_*` 先取出再断言(`no-sideeffect-in-assert`);③ 组合 surf+vol 的 `MappedGeometrySource` 的 `acquire_points` 返回 **surface** 点(`hasSurface_ ? points_`),故等价对比要**按 aspect**(surface points/tris 比 eager surface handle、tets 比 eager tet handle),不能拿 surface 源的点比 tet 源的点。
- **消费者渲染接入留 follow-up**:E 在 services 层证 AC4 取源等价即收口;`XQMainWindow` 渲染 dispatch 迁"持 assetId→resolver→renderer"需 renderer 吃源公共入口(A/B renderer 侧),未就绪不阻塞 E。惰性链(payload→assetId→manager→MappedGeometrySource)已端到端打通 + 等价验证。
- **M9b 里程碑收口**:A(取数后端+工厂+borrow-faceId)/ B(LOD 砍上传量)/ C(渐进分块上传)/ E(payload 去 vector 化)全部交付;线2(reader 常驻 + 瞬时双份)在 opt-in 下打掉。host 峰值减半 + 大网格拾取(D)是显式后续项(见父 prd)。

## Scenario: Blob metadata schema gate for lazy/eager geometry

### 1. Scope / Trigger
- Trigger: any code that parses `blob` lines from `.xqproj`, reads a `BufferRef`
  through `BlobStore`, stamps `geometryAssetId` for lazy geometry, or converts
  `AssetRegistry` blob refs into `MappedGeometrySource::Inputs`.
- This is a cross-layer storage contract: the project document is the only
  schema source for headerless blob files, so eager and lazy consumers must both
  reject bad metadata before using the blob.

### 2. Signatures
- `BlobStore::get(const BufferRef&, std::vector<double|int|uint8_t>*)`
  returns `BlobStore::Status::InvalidMetadata` for invalid shape/path/type
  metadata before reading bytes.
- `XQProjectReader::load(..., XQProjectReadOptions{.lazyGeometry = true})`
  must run the same project blob schema checks as eager loading before setting
  `XQSurfaceModelPayload::geometryAssetId()` or `XQMeshPayload::geometryAssetId()`.
- `GeometryResourceManager::acquireGeometrySource(assetId, spec)` must validate
  registry `BufferRef`s against `GeometrySourceSpec` before constructing
  `MappedGeometrySource`.

### 3. Contracts
- `BufferRef.relPath`: non-empty relative path under the asset root; absolute
  paths, root names, root directories, and any `..` path component are invalid.
- `BufferRef.formatVersion == 1`, `endianness == 0`, known `elementType`,
  `components > 0`, and `byteCount == elementCount * components *
  BlobStore::byteWidth(elementType)` with checked multiplication.
- Role schema:
  `points/surfPoints/volPoints = F64 x3`;
  `tris/surfTris = I32 x3`;
  `faceId/surfFaceId = I32 x1`;
  `tets = I32 x4`;
  `voxels = U8 x1`.
- Parallel counts: `faceId.elementCount == tris.elementCount`,
  `surfFaceId.elementCount == surfTris.elementCount`, and
  `voxels.elementCount == maskDims[0] * maskDims[1] * maskDims[2]`.
- `GeometryResourceManager` treats `spec.hasSurface` and `spec.hasTet` as
  required complete groups. Missing one requested role returns an empty handle;
  it must not silently downgrade to a partial source.

### 4. Validation & Error Matrix
- Invalid `relPath`, unknown type, bad version/endian, zero components,
  byte-count overflow, or logical byte-count mismatch -> `InvalidMetadata`.
- File shorter than declared byte count -> `TruncatedBlob`.
- File larger than declared byte count -> `ByteCountMismatch`.
- Hash mismatch after size/schema validation -> `ChecksumMismatch`.
- Unknown or duplicate project blob role -> project load failure.
- Bad registry geometry metadata -> empty `GeometrySourceHandle` and no resident
  block inserted.

### 5. Good/Base/Bad Cases
- Good: writer-produced project round-trips in eager and lazy modes; manager
  maps a complete surface or complete surface+tet asset.
- Base: older projects without an assets section keep their existing behavior.
- Bad: lazy load accepts `points` with `components=2`; manager maps a tet-only
  source when `spec.hasSurface=true`; `BlobStore` follows `../outside.bin`.

### 6. Tests Required
- `test_blob_store`: path traversal, logical byte-count mismatch, overflow,
  typed overload mismatch, unsupported version/endian, truncated file, and
  oversized file.
- `test_payload_roundtrip`: corrupted role schema must fail eager and lazy load;
  `faceId`/`tris` and `voxels`/`maskDims` count mismatches must fail.
- `test_geometry_resource_manager`: wrong role components/count/path and missing
  requested surface/tet groups must return an invalid handle with `blockCount()==0`.
- Final gate: Release full `ctest`.

### 7. Wrong vs Correct
#### Wrong
```cpp
if (spec.hasSurface && points && tris && faceId) {
    fillSurface();
}
// Missing faceId can still fall through and return a tet-only valid source.
```

#### Correct
```cpp
if (spec.hasSurface) {
    if (!points || !tris || !faceId) {
        return GeometrySourceHandle();
    }
    fillSurface();
}
```

## Scenario: GeometryResourceManager handle/source lifetime

### 1. Scope / Trigger
- Trigger: any code that returns, stores, copies, moves, or dereferences
  `GeometryResourceManager::VoxelSourceHandle` or `GeometrySourceHandle`.
- The handle is a consumer-facing source owner, not merely a cache lookup token.
  It must remain safe after the manager that produced it is destroyed.

### 2. Signatures
- Public signatures stay stable:
  `valid()`, `source()`, `operator->()`, copy/move construction, copy/move
  assignment, `acquireVoxelSource(...)`, and `acquireGeometrySource(...)`.
- Private storage must use `std::shared_ptr<const IVoxelSource>` or
  `std::shared_ptr<const IGeometrySource>` for handle-owned source lifetime.
- The per-resident `std::shared_ptr<void> pin` remains a separate cache
  residency gate.

### 3. Contracts
- `valid() == true` means the handle owns a live source object. It must not mean
  only that the manager cache still contains a block.
- `source()` and `operator->()` may be called while the handle is alive even if
  the producing `GeometryResourceManager` has already been destroyed.
- Manager destruction, block erase, or budget eviction may release the cache's
  owner of the source, but must not invalidate previously returned handles.
- The eviction policy must continue to inspect only the per-block `pin`
  ownership (`pin.use_count() == 1` means no live manager handle pins residency).
  Do not switch eviction decisions to `source.use_count()`.
- Leases acquired from a live handle must keep their own mapped data controls
  alive and remain readable after manager destruction.

### 4. Validation & Error Matrix
- Handle stores a raw `IVoxelSource*` / `IGeometrySource*` -> dangling access
  after manager destruction.
- Evictor uses `source.use_count()` -> lease-only or copied-source ownership can
  distort LRU/budget behavior.
- `valid()` is tied to `pin_` only -> a handle can report valid while source is
  already destroyed.
- Manager destroys resident blocks while handles exist -> handles must still
  read source data; failure is a lifetime bug, not caller misuse.

### 5. Good/Base/Bad Cases
- Good: return a source handle from a local manager scope, destroy the manager,
  then call `handle.source().acquire_*()` and read the expected data.
- Base: while the manager is alive, existing budget/LRU/cache-hit behavior and
  `blockCount()`, `mapCount()`, `hitCount()`, `evictCount()` remain unchanged.
- Bad: a handle reports valid after manager destruction but dereferencing it
  crashes or reads freed storage.

### 6. Tests Required
- `test_geometry_resource_manager`: voxel handle outlives manager and can read
  `acquire_whole()`.
- `test_geometry_resource_manager`: voxel lease acquired before manager
  destruction remains readable after manager destruction.
- `test_geometry_resource_manager`: geometry handle outlives manager and can
  read points, triangles, and faceIds.
- Existing budget/LRU/pin/concurrency tests must keep passing to prove the pin
  residency gate was not weakened.
- Final gate: Release full `ctest`.

### 7. Wrong vs Correct
#### Wrong
```cpp
class VoxelSourceHandle {
    const IVoxelSource* source_ = nullptr;
    std::shared_ptr<void> pin_;
};
```

#### Correct
```cpp
class VoxelSourceHandle {
    std::shared_ptr<const IVoxelSource> source_;
    std::shared_ptr<void> pin_;
};
```

## Scenario: Lazy geometry GUI resolver aspects and cache keys

### 1. Scope / Trigger
- Trigger: any code that loads native projects lazily, resolves
  `geometryAssetId`, calls `resolveLazyGeometrySource`, or acquires mapped
  geometry through `GeometryResourceManager`.
- This is a cross-layer contract from app startup -> scene payload ->
  services resolver -> mapped source -> renderer. It must preserve eager
  resident rendering while making lazy surface/mesh payloads renderable.

### 2. Signatures
- `XQProjectReadOptions options; options.lazyGeometry = true;`
- `XQAppStartupState::assetRootDir` stores the derived
  `<project-parent>/<project-stem>.assets` directory for project-path startup.
- `void XQMainWindow::attachGeometryResources(const AssetRegistry* registry,
  const std::string& assetRootDir)`.
- `enum class LazyGeometrySourceMode { All, SurfaceOnly, TetOnly }`.
- `resolveLazyGeometrySource(payload, manager, registry)` resolves `All`.
- `resolveLazyGeometrySource(payload, manager, registry, mode)` resolves only
  the requested aspect.
- `GeometryResourceManager::acquireGeometrySource(assetId, spec)` caches by
  `AssetId + resident source kind/aspect + integrity mode`.

### 3. Contracts
- App startup with a native project path must load through
  `XQProjectReader::load(path, &result, XQProjectReadOptions{lazyGeometry=true})`
  and retain the matching asset root. No-argument startup leaves
  `assetRootDir` empty.
- `XQMainWindow` must render resident handles first. Lazy resolution is only
  used when the selected surface/mesh payload has no resident geometry.
- Surface lazy selection uses `LazyGeometrySourceMode::SurfaceOnly` and
  `XQSceneRenderer::addSurfaceProgressive`.
- Mesh lazy selection must try `LazyGeometrySourceMode::TetOnly` first and call
  `addVolumeMeshProgressive` when `meta().tetCount > 0`; only if that path is
  invalid or empty may it fall back to `SurfaceOnly`.
- Do not pass a combined mesh source to the volume renderer: a combined
  `MappedGeometrySource` exposes surface points through `acquire_points()`,
  while tets index the volume point array.
- `GeometryResourceManager` must not reuse a surface-only resident block for a
  later tet-only acquire of the same mesh `AssetId`, or vice versa.
- Invalid/missing asset ids, missing requested role groups, bad metadata, or
  empty specs return invalid handles and must not fabricate geometry.

### 4. Validation & Error Matrix
- Lazy payload with no `geometryAssetId` -> invalid handle; caller may use
  resident fallback.
- Unknown asset id -> invalid handle.
- `SurfaceOnly` with no complete surface role group -> invalid handle.
- `TetOnly` with no complete `volPoints/tets` role group -> invalid handle.
- `GeometrySourceSpec{hasSurface=false, hasTet=false}` -> invalid handle even
  if the same asset was previously cached.
- Same mesh asset acquired as `SurfaceOnly` then `TetOnly` -> two resident
  cache entries, `mapCount` increments twice, and repeated matching acquire
  hits the corresponding entry.
- `FullVerify` and `SegmentedMerkle` acquisitions must not share one resident
  block.

### 5. Good/Base/Bad Cases
- Good: open a saved project through `xq_app <file.xqproj>`, select a lazy
  surface node, and the central stack switches to the render widget through
  `addSurfaceProgressive`.
- Good: select a lazy mesh node with tets and the GUI renders the volume path,
  using the volume point array.
- Base: eager in-memory surface/mesh handles keep the existing `addSurface` and
  `addVolumeMesh` paths.
- Bad: selecting a lazy mesh after a surface-only resolve returns the cached
  surface source and reports `tetCount == 0`.
- Bad: `xq_app` loads a path eagerly, causing all geometry blobs to materialize
  before the window is shown.

### 6. Tests Required
- `test_app_startup`: saved project path loads lazy surface and mesh payloads,
  records `assetRootDir`, attaches the main window, and selecting lazy nodes
  switches from image label to render widget.
- `test_geometry_resource_manager`: same mesh asset acquired surface-only then
  tet-only yields distinct cache entries and correct meta for both aspects.
- `test_geometry_source_resolver`: 3-argument resolver remains source/link
  compatible and resolves the same geometry as eager mode.
- `test_main_window` and `test_scene_renderer_progressive` stay green.
- Final gate: Release full `ctest`.

### 7. Wrong vs Correct
#### Wrong
```cpp
auto lazy = resolveLazyGeometrySource(*payload, manager, registry);
renderer.addVolumeMeshProgressive(lazy.source());
```

#### Correct
```cpp
auto lazy = resolveLazyGeometrySource(*payload, manager, registry,
                                      LazyGeometrySourceMode::TetOnly);
if (lazy.valid() && lazy->meta().tetCount > 0) {
    renderer.addVolumeMeshProgressive(lazy.source());
}
```

## Scenario: Lazy geometry sidecar resolver strategy

### 1. Scope / Trigger
- Trigger: any code that writes geometry blobs/sidecars, resolves
  `geometryAssetId`, sets `GeometrySourceSpec::segmented`, or changes
  `GeometryResourceManager` cache keys for geometry sources.
- Purpose: writer-produced geometry `.merkle` sidecars must be used by lazy
  project/runtime paths without making older or partially written projects
  unreadable.

### 2. Signatures
- `resolveLazyGeometrySource(payload, manager, registry)`
- `resolveLazyGeometrySource(payload, manager, registry, mode)`
- `GeometryResourceManager::GeometrySourceSpec::segmented`
- `GeometryResourceManager::acquireGeometrySource(const AssetId&, const GeometrySourceSpec&)`
- `MappedGeometrySource::IntegrityMode::{SegmentedMerkle, FullVerify}`

### 3. Contracts
- The resolver derives geometry roles and element counts from `AssetRegistry`
  exactly as before; integrity mode selection must not change surface/tet aspect
  selection.
- For a non-empty valid geometry spec, resolver must first call
  `acquireGeometrySource` with `spec.segmented = true`.
- If the segmented acquire returns an invalid handle, resolver must retry the
  same asset/aspect with `spec.segmented = false`.
- FullVerify fallback must still pass `BufferRef.sha256` into
  `MappedGeometrySource` so blob tampering is detected without sidecars.
- FullVerify and SegmentedMerkle resident blocks must remain distinct cache
  entries; a failed segmented attempt must not insert an invalid block.
- The resolver must still return invalid for no `geometryAssetId`, unknown
  asset id, or no complete requested geometry role group.

### 4. Validation & Error Matrix
- Writer-produced sidecars present and valid -> segmented handle valid; first
  geometry acquire checks merkle segments.
- Sidecars missing/unreadable/corrupt but blob content and sha256 are valid ->
  segmented attempt invalid, FullVerify fallback valid.
- Blob tampered and main-document sha256 stale -> segmented may fail on segment
  verification; FullVerify fallback must also fail on sha256 mismatch.
- Missing requested `SurfaceOnly` roles -> invalid; no FullVerify fabrication.
- Missing requested `TetOnly` roles -> invalid; no FullVerify fabrication.

### 5. Good/Base/Bad Cases
- Good: save a native project, lazy-load it, resolve a lazy surface, acquire
  points/triangles, and observe `MappedGeometrySource::stats().segmentsChecked > 0`.
- Base: delete the writer-produced `.merkle` files, lazy-load the same project,
  resolve the surface, and observe valid FullVerify fallback with
  `segmentsChecked == 0` and `bytesHashed > 0`.
- Bad: resolver hard-codes `spec.segmented=false`; sidecars are written but the
  lazy runtime path never exercises segmented verification.
- Bad: resolver hard-codes `spec.segmented=true`; old projects with no sidecars
  become unreadable despite valid anchored blobs.

### 6. Tests Required
- `test_geometry_source_resolver`: writer-produced sidecars drive
  SegmentedMerkle and `segmentsChecked > 0`.
- `test_geometry_source_resolver`: removed sidecars fall back to FullVerify and
  `bytesHashed > 0`.
- Existing eager/lazy equivalence assertions for surface and mesh remain green.
- `test_geometry_resource_manager`: full/segmented and surface/tet cache keys
  remain distinct.
- `test_payload_roundtrip`: writer still emits geometry sidecars.
- Final gate: Release full `ctest`.

### 7. Wrong vs Correct
#### Wrong
```cpp
spec.segmented = false;
return manager.acquireGeometrySource(id, spec);
```

#### Correct
```cpp
spec.segmented = true;
auto handle = manager.acquireGeometrySource(id, spec);
if (handle.valid()) {
    return handle;
}
spec.segmented = false;
return manager.acquireGeometrySource(id, spec);
```

## Scenario: Project-scoped DICOM voxel residency and lazy reopen

### 1. Scope / Trigger
- Trigger: adding or changing DICOM import, `AssetId`-based resident voxel
  install/acquire/remove, persisted image lazy resolution, or the optional real
  DICOM acceptance gate.
- This is a cross-layer contract from reader -> import command -> Scene/Asset ->
  project save/reopen -> runtime `IVoxelSource`. Project metadata is
  authoritative; residency is a rebuildable project-instance cache.

### 2. Signatures
- `bool GeometryResourceManager::usesAssetRegistry(const AssetRegistry*) const`
- `VoxelSourceHandle installResidentVoxelSource(AssetId,
  std::shared_ptr<const IVoxelSource>)`
- `VoxelSourceHandle acquireResidentVoxelSource(AssetId)`
- `bool removeResidentVoxelSource(AssetId)`
- `bool removeResidentVoxelSourceIfMatches(AssetId,
  const VoxelSourceHandle&)`
- `ImageResourceResolveResult ImageResourceResolver::acquire(AssetId,
  const XQImageVolume&, const std::string& projectFilePath,
  GeometryResourceManager&, const AssetRegistry&, IDicomSeriesReader&)`
- CMake cache path `XQ_DICOM_TEST_DATA_ROOT`; when non-empty it registers
  `test_dicom_real_series`.

### 3. Contracts
- A `GeometryResourceManager` is permanently bound to the `AssetRegistry`
  supplied at construction. A resolver must reject a manager/registry mismatch;
  an `AssetId` from another project must never hit the cache.
- Caller-installed DICOM residency uses its own cache flavor, distinct from
  managed FullVerify/Segmented voxel blobs. Install requires an already
  registered asset and may occur only after the project batch command succeeds.
- Each resident block snapshots the content-bearing asset association:
  category/kind, relative and absolute locators, DICOM identity, fingerprint,
  and geometry. Lookup drops an entry when that association changes or the
  asset is removed/re-registered.
- A handle owns both the source and the per-install `pin`. The pin is also the
  install generation: compare-and-remove must match both source and pin. An old
  handle must not remove a later install even when the exact same source
  `shared_ptr` is reinstalled. Existing handles/leases remain readable after
  cache removal or manager destruction.
- `sourceRelPath` is resolved from the `.xqproj` parent. It may contain `..` for
  a deliberate external relative source, but it must have no absolute/root-name
  or root-directory component (`C:series` and `\series` are invalid on
  Windows). `sourceAbsPath` is used only when the relative directory is missing.
- Resolver entry snapshots the AssetRecord before reader I/O and revalidates
  both locators, fingerprint, DICOM identity, and geometry before install. It
  always passes the persisted SeriesInstanceUID and accepts data only after
  canonical LPS geometry, scalar/component, buffer size, intensity/window,
  identity, and fingerprint checks succeed.
- Reader-owned diagnostic codes/messages never cross the service boundary;
  only fixed severity-based templates are emitted. Image payloads stay
  metadata-only, and `XQProjectReader` never decodes DICOM.
- `XQ_DICOM_TEST_DATA_ROOT` defaults empty and therefore registers no test. When
  configured it must name a readable directory containing exactly one
  multi-slice series; the test explicitly reads its discovered UID and performs
  import/save/reopen/headless lazy byte recovery. Running that target against a
  synthetic fixture validates plumbing only, not the required authorized
  de-identified real-data acceptance run.
- Authorization and de-identification are an evidence gate separate from byte
  readability. A directory name containing `tcia`, `public`, or `anonymized` is
  not evidence. Keep an external, non-PHI provenance bundle beside the data with
  the official collection/download identity, license, official de-identification
  policy, archive digest, and provider-supplied per-file hashes. Do not commit the
  dataset, archive, local path, or provenance bundle to this repository.
- Before reporting the authorized gate as passed, verify the archive digest,
  every supplied per-file hash, expected file count, and DICOM preamble, then run
  the production tests. If official web pages cannot be reached during a later
  rerun, report that live revalidation was unavailable; never convert a network
  failure into a fabricated online check.
- The real-data gate carries only geometry/identity, byte count, and a SHA-256
  digest between phases. Initial decode and import residency must leave scope
  before lazy reopen; never retain or copy several full clinical volumes merely
  to compare bytes.

### 4. Validation & Error Matrix
- manager bound to another registry, unknown asset, malformed persisted
  metadata, or rooted `sourceRelPath` -> `InvalidArgument`; no reader call and no
  resident insertion.
- both locators unavailable -> `SourceNotFound`.
- reachable directory without the persisted UID -> `SeriesNotFound`; never pick
  the first discovered series.
- UID/fingerprint/geometry/scalar/intensity drift, or locator/fingerprint/
  identity/geometry mutation during reader I/O -> `SourceChanged`; no install.
- reader reports Ok without a valid buffer, or validated residency cannot be
  installed/acquired -> `ReadFailed`.
- stale compare-and-remove handle, including same-source reinstall -> `false`;
  the current generation remains resident.
- non-empty `XQ_DICOM_TEST_DATA_ROOT` that is not a directory -> configure
  failure. Zero/multiple/single-slice series or byte mismatch after reopen ->
  `test_dicom_real_series` failure, never skip-as-green.
- missing license/source/de-identification evidence -> the production read may be
  reported only as a technical real-file run, not as the authorized gate.
- archive or per-file hash mismatch, unexpected file count, or missing DICOM
  preamble -> stop before product execution and reject the evidence bundle.

### 5. Good/Base/Bad Cases
- Good: commit an ExternalSource Image asset, install its resident source,
  save/reopen metadata-only state, then resolve the same UID/fingerprint and
  recover identical bytes without any MainWindow state.
- Base: a valid resident association returns without invoking the reader;
  default builds with no real-data path keep the optional test unregistered.
- Good: an external public-series bundle has a recorded official source and
  license, all provider hashes and DICOM preambles verify, both production
  real-data tests pass, and the CMake cache is restored to an empty root.
- Bad: cache globally by `AssetId`; install before command success; accept
  `C:series` as project-relative; read the old locator then snapshot a new one;
  remove a same-pointer reinstall using only source-pointer equality; or call a
  directory "authorized" without source/license/hash evidence.

### 6. Tests Required
- `test_geometry_resource_manager`: install/acquire/remove, independent managed
  flavor, budget/LRU/pin behavior, asset-id reuse invalidation, manager lifetime,
  invalid/overflow metadata, and same-source ABA generation regression.
- `test_dicom_import_integration`: atomic project state, strict fingerprint and
  checked buffer size, safe diagnostics, and rooted-relative locator rejection.
- `test_image_resource_resolver`: resident-first, project registry mismatch,
  relative preference/absolute fallback, missing/changed source, persisted UID,
  locator/fingerprint mutation during I/O, PHI sentinel redaction, root-component
  rejection, save/reopen, and headless byte access.
- With an explicitly authorized de-identified multi-slice directory: configure
  `-DXQ_DICOM_TEST_DATA_ROOT=<series-dir>`, build `test_dicom_real_series`, and
  run that test. Record only non-sensitive pass/fail evidence.
- Independently verify the external archive digest, every provider-supplied file
  hash, file count, and DICOM preamble without printing patient/free-text tags.
  Run `test_shell_a_real_data` when registered, then restore the cache variable
  to empty and confirm the default CTest count.
- Final gate: focused DICOM/source/project/VTI tests plus Release full `ctest`.

### 7. Wrong vs Correct
#### Wrong
```cpp
if (current.voxelSource == expected.source_) {
    erase(current); // stale handle can delete a same-pointer reinstall
}

reader.read(oldLocator, persistedUid);
install(assetId, source); // asset locator may have changed during I/O
```

#### Correct
```cpp
if (current.voxelSource == expected.source_
    && current.pin == expected.pin_) {
    erase(current); // exact install generation only
}

const AssetRecord snapshot = *registry.find(assetId);
auto read = reader.read(resolvedLocator, snapshot.dicom.seriesInstanceUid);
const AssetRecord* current = registry.find(assetId);
if (!current || current->sourceRelPath != snapshot.sourceRelPath
    || current->sourceAbsPath != snapshot.sourceAbsPath
    || current->contentFingerprint != snapshot.contentFingerprint) {
    return SourceChanged;
}
install(assetId, validatedSource);
```

## Scenario: Legacy DICOM frame identity and binary gold labels

### 1. Scope / Trigger

- Trigger: reading a public classic DICOM series that has complete LPS slice
  geometry but omits `(0020,0052) FrameOfReferenceUID`, converting a DICOM
  voxel gold mask to `XQSegmentationMask`, or preparing such a series for the
  persisted XQ Image/Path/Profile workflow.
- Purpose: read real legacy bytes without silently inventing identity, while
  still providing a deterministic, auditable path into contracts that require
  a frame UID.

### 2. Signatures

```cpp
DicomSeriesReadResult GdcmItkDicomSeriesReader::read(
    const std::string& directory,
    const std::string& seriesInstanceUid);

DicomLabelReadResult GdcmItkDicomLabelReader::read(
    const std::string& directory,
    const std::string& seriesInstanceUid,
    const DicomBinaryLabelProfile& profile);
```

```text
xq_normalize_dicom_frame
  <source-dicom-directory> <new-output-directory> <public-frame-key>

xq_vascular_label_alignment_probe
  <image-dir> <label-dir> <label-name> [foreground-value]
```

Stable diagnostics:

```text
dicom.series_descriptor_missing_frame_uid
dicom.frame_uid_absent
dicom_label.unsupported_values
```

### 3. Contracts

- The production DICOM reader may return `Ok` for a series with an empty frame
  UID only when Study/Series/SOP identity and position/orientation/spacing are
  otherwise valid and consistent. It preserves the empty string and emits the
  warning; it never derives a UID inside `discover()` or `read()`.
- `isValidCanonicalDicomImageMetadata` remains strict. Project import,
  Path/Profile assembly, save/reopen residency, and any caller requiring frame
  identity continue to reject an empty frame UID.
- `xq_normalize_dicom_frame` writes a new directory only. It derives one
  `2.25` UID from version + public frame key + complete canonical LPS geometry,
  inserts only `(0020,0052)`, then re-reads through the production reader and
  requires equal Series UID, decoded buffer SHA-256, dimensions, spacing,
  origin, and direction.
- Original bytes remain immutable and separately hashed. A derived output is
  local-only when the source license forbids redistribution of adaptations.
- `GdcmItkDicomLabelReader` delegates all enumeration, metadata validation,
  sorting, decode, rescale, and fingerprint work to
  `GdcmItkDicomSeriesReader`. It owns only the explicit binary-value mapping to
  `XQSegmentationMask`; no second DICOM reader is allowed.
- Binary gold profiles require two distinct finite scalar values, a non-zero
  XQ output label, and a non-empty public label name. Any third voxel value or
  empty foreground rejects the whole mask.
- Gold-label readers and alignment probes are evaluator/data-preparation
  paths. Production segmentation receives image + versioned profile only.

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| frame UID present and valid | ordinary reader/import path; no missing-frame warning |
| frame UID absent, full consistent LPS geometry | reader `Ok` + explicit warning; canonical import still rejects raw series |
| frame UID absent and geometry incomplete/inconsistent | `UnsupportedGeometry`/`InconsistentSeries`; no normalization |
| normalization output already exists | refuse; never overwrite an audited derived series |
| source/output decoded digest or geometry differs | normalization fails; derived series is not accepted |
| CT and label normalize with different frame keys/geometries | frame mismatch; alignment fails |
| label contains value outside frozen background/foreground pair | `UnsupportedLabelValues`; no partial mask returned |
| gold path reaches production runner | architecture/acceptance failure even if metrics improve |

### 5. Good/Base/Bad Cases

- Good: raw IRCAD CT and masks read with an absent-frame warning; one documented
  case/geometry key creates the same derived frame UID for CT and both masks;
  decoded hashes remain equal; derived CT imports, saves, reopens, and lazy
  reloads through existing XQ ownership.
- Base: a conformant DICOM series already carrying FrameOfReferenceUID follows
  the unchanged reader/import path and is never rewritten.
- Bad: fill `frameOfReferenceUid` from Study UID, directory text, random UID, or
  geometry hash inside the reader; copy GDCM enumeration into the label reader;
  accept every non-zero gold value without a frozen profile.

### 6. Tests Required

- Existing conformant synthetic DICOM adapter/import/resolver checks remain
  green, proving no regression in ordinary series selection, geometry, rescale,
  persistence, or lazy recovery.
- On an authorized real missing-frame case, run the production reader and
  assert warning + decoded geometry/buffer; normalize CT and labels; assert one
  shared derived UID and equal source/output decoded SHA-256.
- Run `xq_vascular_label_alignment_probe` for each accepted label and assert
  geometry equality, frame equality, complete index-to-LPS round-trips, and the
  frozen foreground count/hash.
- Configure the optional real DICOM root to the derived CT, run
  `test_dicom_real_series` and `test_shell_a_real_data`, then restore the cache
  variable to empty.
- Final Release full CTest remains required evidence, not completion by itself.

### 7. Wrong vs Correct

#### Wrong

```cpp
if (identity.frameOfReferenceUid.empty()) {
    identity.frameOfReferenceUid = identity.studyInstanceUid;
}
```

#### Correct

```cpp
if (identity.frameOfReferenceUid.empty()) {
    addWarning("dicom.frame_uid_absent");
    // Preserve absence. A separate audited tool may write a derived copy.
}
```

```text
raw immutable series
  -> production read with explicit missing-frame warning
  -> separate deterministic frame-v1 derived directory
  -> production re-read proves same pixels + LPS geometry
  -> canonical XQ import/save/reopen
```
