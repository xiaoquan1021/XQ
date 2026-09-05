# M8b-2 设计(design.md)

> 依据:4 路并行勘察(payload 爆破面 / lease 升并发影响 / io 加载路径 / manager 分层)+ scale-probe 实测。
> 范围决策:**全栈打通** —— manager + lease 并发 + 生产 MappedVoxelSource + 分段 Merkle,并把**最大且当前游离的 image 体素链**真正接到 manager 懒映射端到端验证;surface/mesh/seg 几何 payload 去 vector 化 + 其消费者迁移留 M9a。

## 0. 一句话定调

把"谁持数据、何时加载/释放"这层补齐:**AssetRegistry(core,身份层)不变,新增 GeometryResourceManager(services,数据层)按 assetId 懒驻留 + 预算 + refcount 驱逐;生产 MappedVoxelSource(io)走 map→verify(分段 Merkle)→reinterpret 零拷贝;ReadLease 并发语义定稿为协作式 use_count gating(签名不变,契约强化)。** 首个真实接入选 image 体素链(最大数组、游离在 scene 外、消费者明确),不与 M9a 几何消费者迁移撞车。

## 1. 分层与依赖(ROADMAP 锁死,勘察实证)

```
services/  GeometryResourceManager  ← 新增,持 IVoxelSource*/IGeometrySource* 抽象 + 预算/LRU/gating
   │  新增 link 边 xq_services → xq_io(方向合法、无环;勘察:io 不依赖 services)
io/        MappedVoxelSource(生产)  ← 实现 core 的 IVoxelSource;复用 BlobStore 路径/Sha256;零 VTK
   │
core/      IVoxelSource/IGeometrySource/ReadLease(M8b-1,签名冻结) + AssetRegistry(身份层,不碰 load/evict)
```

- manager 头文件**只暴露 core 的 Source/Lease 类型**,把 `MappedVoxelSource` 构造藏在 .cpp → `xq_services` 对 `xq_io` 用 **PRIVATE** link(不污染 services 下游)。证据:CMakeLists.txt:191-234,xq_io PUBLIC xq_core、xq_services 现仅 PUBLIC xq_core。
- AssetRegistry(`AssetRegistry.h:14-19` 硬声明无 load/evict/mmap)与 manager 互补:manager 持/查 Registry 拿 `AssetRecord.blobs`(role→BufferRef),不反向。

## 2. 三个核心决策(自主拍板 + 理由)

### 决策 A — 分段 Merkle 段表存储:`.merkle` 边车 + 主档锚 root(schema 1.3,向后兼容)
- **选**:每个体素 blob 旁存一个 `.merkle` 边车(段大小 + 逐段 SHA-256 列表),其 **Merkle root 记入主档的 blob 记录(schema 1.2→1.3)**;root 是信任锚。
- **读路径**:首次 map 时读 `.merkle`,用逐段 SHA 重算 root 比对主档锚 root(一次性、轻量,O(段数)而非 O(blob));之后 acquire 触碰某段 → 只对该段算 SHA 比对段表项(惰性,bytesHashed 随触碰量)。
- **理由**:① 探针实测分段 Merkle 必选(128× 惰性差距);② 段表自身需信任锚,否则段表被篡改无从发现——root 入主档(已签名文档)是干净锚点;③ **向后兼容**:老档(1.2,无 root / 无 `.merkle`)→ manager 回退 `FullVerify`(全量 SHA,即 M8a 现状),不破坏旧工程;④ 不改 `BufferRef` 结构体(它是 blob 自描述),root 作为主档 blob 记录的新字段,走 reader/writer schema 升级——这是 io 本职。
- **不选**扩 `BufferRef` 加段表字段:BufferRef 是无头 blob 的自描述,塞段表破坏其"单 blob 一条 ref"的简洁;边车独立、内容寻址、可单独校验更干净(探针已证边车可行)。

### 决策 B — lease 并发:协作式 use_count gating(签名冻结,契约强化)
- **选** REPORT §1③ 选项 a:`ReadLease`/`IVoxelSource`/`IGeometrySource` **公共 C++ 签名全部冻结不变**;并发机制全落 manager 侧。
- **机制**:manager 为每个可驱逐驻留块持一个 `shared_ptr<const void> block`(= mmap keepalive,deleter 做 unmap);acquire 时把这个 block 注入 `VoxelLease::borrow(block, view)`,lease 拷一份 → use_count+1。evictor 只在目标块 `use_count()==1`(无 live lease)时回收。**acquire 与 evict 决策共享一把 manager 锁**,杜绝 TOCTOU(判定 use_count==1 后又被新 acquire 抬到 2)。
- **不变量**:keepalive 控制块所有权**收敛到 manager 独占基线**——除 manager 持的那一份 + 活跃 lease 拷贝外,禁止第三方长持该 shared_ptr(否则 use_count 永 >1 无法驱逐)。
- **borrow vs own 二分**:`borrow` lease(连续:acquire_whole/slab、几何 points/tris/tets)= 被 pin 不可驱逐;`own` lease(region 跨行物化、faceId 物化)自持 buffer、keepalive 为 null、免疫驱逐。design 把这条写进生命周期。
- **契约强化**(进冻结门的唯一 core 改动,是文本非签名):spec `source-interface.md` 把 keepalive 从"延寿"升级为"**驱逐 gating pin:任何可驱逐存储的 IVoxelSource 实现者,其 evictor 必须在 use_count==1 时才回收底层**"。Resident*Source(常驻 baseline、永不驱逐)不改。
- **并发≠可拷贝**:多线程各自 acquire 各得独立 lease(各拷 keepalive,shared_ptr 计数原子),move-only(AC9)保持冻结。
- **理由**:探针实测协作式 SAFE;此刻生产无任何 Source 消费者(只有测试),改契约迁移面为零——这是定稿 lease 语义的最佳窗口,晚于 M9a 改就有面。

### 决策 C — 首个真实接入:image 体素链(全栈打通,不碰几何 payload)

> **2026-06-30 实现期作废**:本决策的前提(image 源体素是可经 manager 懒映射的 M8a blob)查实不成立——源 image 体素由 `VtkImageAdapter::loadVtiWithBuffer` 直接解码 `.vti`(`VtkImageAdapter.cpp:156`),从未存为 M8a content-addressed blob,且仅测试调用、不走 project reader。reader 唯一在 load 期物化的 M8a 体素 blob 是分割 mask 体素(`XQProjectReader.cpp:1271-1279`),但拆 `XQSegmentationMask.voxels` 值成员属动 core payload = M9a 性质(消费者迁移)。
> **决定(用户确认)**:M8b-2 机制收口,任何体素链全栈接入整体移交 M9a。冻结门只需"真 mmap + 并发 evict 喂过接口且存活"——已由 manager + 生产 MappedVoxelSource + 并发测试达成。下面原文保留为历史。

#### (作废原文)首个真实接入:image 体素链
- **现状**(勘察):image 节点只挂 source-path payload,体素 `XQMemoryImageBufferHandle` 由 `VtkImageAdapter::decode` 一次性整卷 make_shared,引用传给 SegmentationService/AiService/renderer。**最大数组,且游离在 scene payload 外**。
- **改造**:体素卷作为 asset(已有 assetId↔blob 元数据)经 manager 按 assetId 懒映射(`MappedVoxelSource`);消费侧从"收 `const XQMemoryImageBufferHandle&`"改为"经 manager 取 `IVoxelSource&` / 持 `VoxelLease`"。端到端:加载工程 → 体素不在 load 期物化(AC7)→ 首次分割/渲染时 manager 映射 → 经 `IVoxelSource` 取数。
- **理由**:① 体素链是最大内存项,先收编它最能验证 manager 真实价值;② 它游离、消费者边界清晰(seg/ai/render 的 image 路径),改造**不与 M9a 的几何 payload 消费者(renderer build_surface/writer flatten)迁移撞车**;③ 几何 payload(surface/mesh/seg-mask)的去 vector 化牵动 renderer/writer 大面积迁移,那是 M9a,本里程碑不动(范围纪律)。
- **seg-mask voxels(值成员,最难拆)**:本里程碑**不拆**(留 M9a 随几何 payload 一起)——它是分割产物 mask 不是源 image 体素,与"image 体素链"是两条链,别混。

## 3. 组件设计

### 3.1 GeometryResourceManager(services,`src/services/resource/`)
- 状态:`AssetRegistry*`(查 BufferRef)+ `assetId → ResidentBlock` 映射 + LRU 队列 + 预算上限 + 一把 mutex。
- `ResidentBlock`:持 `shared_ptr<const void> keepalive`(mmap 或 resident vector 的控制块)+ 字节数 + 该 block 当前 `IVoxelSource`/`IGeometrySource` 实例。
- API(只吐 core 抽象):`acquireVoxelSource(assetId) -> IVoxelSource&`(或返回带生命周期的句柄);`acquireGeometrySource(assetId)`;`setBudgetBytes(n)`;`residentBytes()`/计数器(供 AC1/AC2/AC7 验证)。
- 加载:首次 acquire → 查 Registry 拿 role→BufferRef → 经 io 工厂构造 `MappedVoxelSource`(或 Resident 后备)→ 入映射 + LRU + 累加预算。二次命中缓存。
- 驱逐:`residentBytes > budget` → LRU 从尾扫,**仅 `keepalive.use_count()==1` 的块**回收(全程持 manager 锁);跳过活跃。
- 后备选择:面向 `IVoxelSource`/`IGeometrySource` 抽象,mmap(大/磁盘后备)与 resident(小/内存产物)对 manager 无差别。

### 3.2 MappedVoxelSource(io,`src/io/source/`)生产级
- `MmapBlob`(从探针提升):`CreateFileW`/`CreateFileMapping(PAGE_READONLY)`/`MapViewOfFile(FILE_MAP_READ)` + RAII(view→map→file 顺序拆)+ `shared_ptr<const void>` keepalive(deleter unmap)。0 字节/页对齐边界已验证。
- 实现 `IVoxelSource`:`acquire_whole` 零拷贝 borrow(view.bytes=ReadSpan 指 mmap 基址,keepalive=MmapBlob 控制块);`acquire_region/slab` own 物化(同 Resident);越界 invalid lease。
- 完整性:决策 A 的分段 Merkle(读 `.merkle` 边车 + 锚 root 校验 + 按触碰段惰性校验);校验失败返回 invalid lease。
- 读路径**绕开 BlobStore::get**(它两遍全量:readVerified 整读+整 SHA、get 逐元素 readLE)。生产新增"map+verify-span+reinterpret"路径。
- **从探针弃码替换为生产**:段表改"读持久化 `.merkle`"(非现算)、整文件单次映射(超大 blob 可后续分块,本里程碑整映射够用)。

### 3.3 MerkleSidecar(io)
- 写:生成体素 blob 时,旁路产 `.merkle`(段大小 + 逐段 SHA + root),root 入主档。复用 `Sha256::update`/picosha2 对各段算哈希(无新依赖)。
- 读:首次 map 读 `.merkle`,重算 root 比对主档锚;之后按段惰性校验。
- 段大小:起点 1 MiB(探针默认),可配。

### 3.4 reader/writer schema 1.3 + open-time 不物化体素(io)
- writer:体素 asset 写 blob 时旁产 `.merkle` + 主档记 root(schema 1.3)。
- reader:体素体素链**不在 load 期 `store.get` 物化**(勘察:现 `XQProjectReader.cpp:2471-2508` rebuild 全量物化)——改为只注册 assetId+BufferRef(+merkle root)到 Registry,实际数据留给 manager 首次 acquire 时映射(AC7)。几何 payload 的 rebuild 本里程碑**不动**(留 M9a)。
- 向后兼容:老档无 root/`.merkle` → manager 该 asset 走 FullVerify(全量 SHA);老档几何 rebuild 路径不变。

## 4. 消费者改造(仅 image 体素链)
- `VtkImageAdapter::decode`:仍产体素 blob(或经 writer 落 blob),但**不再把整卷 handle 直接喂下游**;改为注册 asset,下游经 manager 取。
- `SegmentationService`/`AiService` 的 image 体素入参:从 `const XQMemoryImageBufferHandle&` 改为经 manager 取 `IVoxelSource&` / 持 lease 取 span(seg 的 region-grow 随机访问走 acquire_whole 的 view + inline scalarAt,渲染走整卷/slab)。
- `XQSceneRenderer::addImageSlice`:image 体素从 manager 取 source。**注意**:这是 image 体素这一条;renderer 的 surface/volume/mask 三处(几何)**不动**(M9a)。

## 5. 严格不做
- 不动 surface/mesh/seg-mask 几何 payload 的成员结构、不迁移 renderer build_surface/build_volume、writer flatten、meshing 取 handle(全 M9a)。
- 不改 M8b-1 lease/接口公共 C++ 签名(只强化 spec 契约文本)。
- 不做 VTK 上传核重写、LOD/分块/渐进上传、faceId 借用 span、region strided-view。
- 不复用 `XQ/probe/` 弃码(生产重写在 io/services)。

## 6. 验证策略
- 新增 `xq_io` 测试:MappedVoxelSource 正确性(AC3)、分段 Merkle 惰性+防篡改(AC4)、schema 1.3 round-trip + 老档兼容。
- 新增 `xq_services` 测试:manager 按需驻留/缓存命中(AC1)、预算 LRU + refcount gating 并发(AC2)、lease 并发契约(AC5)。
- 集成测试:image 体素链全栈(AC6)+ open-time 不物化(AC7)。
- AC8 分层:core 无新 load/evict/mmap;io 无 VTK;OFF/默认全量 ctest 绿。
- 假绿抽查:篡改某段字节 → 触碰段 acquire FAIL(AC4);evictor 无视 use_count → 暴露(AC2/AC5 用协作式应安全,篡改成强制驱逐应触发悬空检测)。副作用不进 assert。
- 构建配方见 memory `xq-build-recipe`;并发测试注意 memory `evict-harness-pin-before-evict-race`(pin-before-evict 同步)。

## 7. 完成 = 冻结门
本里程碑验证存活后,libXQ/Source 1.0 候选接口冻结(早于 M9a 消费者迁移):可冻签名 + 强化后的 lease 驱逐 gating 契约。
