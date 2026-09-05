# M9b-E 设计 — payload 去 vector 化(并存 assetId 惰性驻留路径)

> 依据实读:`XQSurfaceModelPayload.h:36-46`、`XQMeshPayload.h:37-50`(clone)、`XQDataNode.h:40-51`(node assetId)、`XQProjectReader.cpp:966-1012/1313-1430/1432-1626/2038-2066/2424-2508`(整块 load + attach)、`GeometryResourceManager.{h,cpp}`(acquireVoxelSource:60-118 模板)、`CMakeLists.txt:240-242`(services→io PRIVATE)、`put_payload_blobs:774-835`(writer blob role)。
> 用户决策锁定:**并存惰性路径**,不物理删 handle 字段、不破 clone、可回退;依赖 M9b-A。

## 架构总览

惰性链路一句话:**payload 持 assetId → 消费者在 services/app 层经 manager 把 assetId 解析成 `IGeometrySource` → 按需取连续 span**。reader 只负责"记 assetId + 不物化",绝不碰 manager。

```
              ┌──────────────────────────────────────────────────────────┐
   io 层      │ XQProjectReader::load(path, out, {lazyGeometry})           │
 (无 services)│  asset 有几何 blob + 绑定节点 + lazyGeometry?               │
              │   是 → payload.setGeometryAssetId(asset.id);几何 handle 留空│
              │   否 → rebuild_triangle_geometry/tet 整块 get(现行,回退) │
              └───────────────────────────┬──────────────────────────────┘
                                          │ payload(持 assetId 或 持 handle)
                                          ▼
   core 层    ┌──────────────────────────────────────────────────────────┐
              │ XQSurfaceModelPayload / XQMeshPayload                      │
              │  + optional geometryAssetId(AssetId,core 已有)           │
              │  clone(): 有 handle → 深拷;仅 assetId → 拷 assetId 引用    │
              └───────────────────────────┬──────────────────────────────┘
                                          │ 消费者(持 manager)
                                          ▼
 services 层  ┌──────────────────────────────────────────────────────────┐
 (可见 io)    │ acquireGeometrySource(payload, GeometryResourceManager&)   │
              │  持 handle → wrap ResidentSurfaceSource/ResidentTetSource  │
              │  持 assetId → manager.acquireGeometrySource(id, spec)      │  ← A 产出
              │            → MappedGeometrySource(map+FullVerify+reinterpret)│
              └───────────────────────────┬──────────────────────────────┘
                                          ▼ const IGeometrySource&
                            renderer / 网格服务 / writer(渐进迁移)
```

两条路径**全程并存**:resident handle 路径(M9a 已迁,`ResidentSurfaceSource` 包 handle)不动一行;assetId 惰性路径是可选叠加。

---

## 决策 1:惰性"解析"落 **services**,reader 只"标记 + 不物化"

### 背景(实测,硬约束)
`CMakeLists.txt:240-242`:`xq_services` 链 `xq_io` 为 **PRIVATE**,io 永不依赖 services。`GeometryResourceManager` 在 services。**reader(io)物理上不能调 manager**——强行让 reader 惰性解析会引入 `io→services` 反向边,破环。

### 取舍
- **方案 A reader 内解析(否决)**:要么把 manager 下沉到 io(破分层、manager 还要 `AssetRegistry`+预算+LRU,全在 services),要么给 reader 注入一个 core 级 resolver 接口(payload 持函数指针/接口 → 又把"谁持有数据何时释放"的 services 职责泄漏进 core/payload,且 clone 要拷 resolver,复杂度上来)。
- **方案 B reader 只标记,services 解析(选定)**:reader 在 payload 上盖 `geometryAssetId` 戳并跳过整块 get;真正"assetId→source"由 services 层的消费者(持 manager)在用到几何时调 `acquireGeometrySource`。职责干净:io 管"从磁盘解析出结构 + 标记几何来源",services 管"按需驻留几何 bytes"。与体素侧范式一致(`GeometryResourceManager::acquireVoxelSource` 也在 services 被消费者调,reader 不碰)。

### 结论
选 **B**。reader 改动仅:① 加读选项 `lazyGeometry`;② surface/mesh 的 rebuild 在惰性档跳过几何整块 get、改盖 assetId 戳。io 零新增依赖。解析助手 `acquireGeometrySource(payload, manager)` 落 services(可见 manager + core payload)。

---

## 决策 2:payload **自带** `geometryAssetId`,不复用 node assetId

### 背景
`XQDataNode` 已有 node→asset 绑定(`XQDataNode.h:40-51`,`setAssetId/hasAssetId/assetId`),reader 在 attach 阶段已 `node->setAssetId`(:2439)。看似可直接拿 node 的 assetId 解析几何。

### 取舍
- **复用 node assetId(否决)**:`clone()` 是 **payload 方法**,克隆出的 payload 脱离 node 上下文(edit-style 命令拷 payload 后另挂节点,见 payload.h 注释)。若几何来源只存在 node 上,克隆的 payload 丢失"我的几何在哪个 asset",惰性解析断链。且一个 node 概念上一个 asset,但 payload 自描述更稳。
- **payload 自带 geometryAssetId(选定)**:payload 自描述"我的几何走哪个 asset 惰性取",随 clone 一起走、与 node 解耦。`AssetId` 已是 core 值类型(`AssetId.h`),payload 加一个 `std::optional`-风格成员(`AssetId asset_; bool has_=false;`,与 `XQDataNode` 同款)即可,零新依赖。

### 结论
选**自带**。reader 盖戳时用 `asset.id`(rebuild 循环里 `it->id` 可得,见 :2473-2481 与 node_by_asset:2461-2466),与 node assetId 同值但独立存储,clone 存活。

---

## 决策 3:spec 来源 — 对接 A 的 `GeometrySourceSpec`

### 背景
`manager.acquireGeometrySource(id, spec)`(A 产出,对称于 `acquireVoxelSource(id, VoxelSourceSpec)`:`GeometryResourceManager.cpp:60-118`)需 spec 描述几何(点数/三角数/tet 数 + 哪些 blob role + 完整性档)。voxel 侧 spec 是 dims/type/components;几何侧 A 定义 `GeometrySourceSpec`。E 是消费方,spec 内容**以 A 的最终定义为准**,开工前对齐。

### 取舍(spec 谁来填)
- **payload 携带完整 spec(否决/最小化)**:让 payload 存点/三角计数等 → payload 字段膨胀、与 writer 写出的 blob 元数据冗余。
- **解析时从 registry 重建 spec(选定)**:`AssetRegistry`(core)里 `AssetRecord.blobs` 已有各 role 的 `BufferRef`(含 `byteCount`)。**实读确认(`GeometryResourceManager.cpp:198-206`)**:`acquireGeometrySource` 的 `GeometrySourceSpec` 计数(`pointCount`/`triCount`/`volPointCount`/`tetCount` + `hasSurface`/`hasTet`)是**调用方必填**,manager **不**从 registry 自推(它只用 `byteCount` 累加预算)。故 **E 的 services 助手**负责从该 asset 的 BufferRef 反推计数:`pointCount = points.byteCount/(3·sizeof(double))`、`triCount = tris.byteCount/(3·sizeof(int))`、`volPointCount = volPoints.byteCount/(3·sizeof(double))`、`tetCount = tets.byteCount/(4·sizeof(int))`;role 命中 `points`/`surfPoints` 决定 surface 命名,`volPoints`/`tets` 决定 tet。`segmented=false`(FullVerify)。payload **不冗余几何计数**。

### 结论
spec 由 **E 的 services 助手**从 registry blob `byteCount` 推导(manager 不自推,已实读核实);E 助手入参 = `(const XQPayload&, GeometryResourceManager&, const AssetRegistry&)`——需 registry 才能反推计数。surface 用 `points/tris/faceId` 或 `surfPoints/surfTris/surfFaceId`,vol 用 `volPoints/tets`。**A 已合入,spec 字段已锁定(`GeometryResourceManager.h:84-92`),开工前置已清。**

---

## 决策 4:clone() 分支(不引入 COW)

```cpp
// XQSurfaceModelPayload::clone()(示意,沿用现有结构 :36-46)
std::shared_ptr<XQPayload> clone() const override {
    XQSurfaceModel copy = model_;                 // 值拷:faces/source + 浅拷几何 ptr
    if (model_.hasTriangleGeometry()) {           // 路径①:持 handle → 深拷(现行为)
        auto g = std::make_shared<XQTriangleSurfaceGeometryHandle>(*model_.triangleGeometry());
        copy.setTriangleGeometry(g);
    }
    auto out = std::make_shared<XQSurfaceModelPayload>(std::move(copy));
    if (has_geometry_asset_) out->setGeometryAssetId(geometry_asset_); // 路径②:assetId 引用拷贝
    return out;
}
```
- 持 handle:深拷 handle(原/克隆不共享可变几何,AC2 守 payload.h 既有契约)。
- 仅 assetId、无 handle:`hasTriangleGeometry()==false` → 不进深拷分支 → 只拷 assetId 引用,**零几何分配**(惰性收益不被 clone 破坏)。
- 二者都有(理论态):handle 深拷 + assetId 引用一并带上,无歧义。
- **无 COW**:只读"有无 handle"分支,不引共享可变态/写时复制。`XQMeshPayload`(:37-50,surf + vol 两 handle)同法,assetId 覆盖整个 payload 的几何来源。

---

## 数据流

**写出(不变)**:payload(持 handle)→ `put_payload_blobs`(:774-835)经 `ResidentSurfaceSource`/`ResidentTetSource` 取 span → blob role 写盘 + registry 记 BufferRef。E 不改 writer。

**惰性载入**:
1. reader 解析 assets 段 → registry 记 `AssetRecord`(含 blob role→BufferRef,:2417-2421)。
2. rebuild 阶段(:2473-2508),`lazyGeometry==true` 且 asset 有几何 blob:构造 payload(几何 handle 留空)+ `setGeometryAssetId(it->id)`,**跳过 `rebuild_triangle_geometry`/tet 整块 get**;attach 到节点。
3. 消费者用到几何时(渲染/导出),services 助手 `acquireGeometrySource(payload, manager)`:payload 持 assetId → `manager.acquireGeometrySource(id, spec)` → `MappedGeometrySource`(A:map+FullVerify+reinterpret 零拷贝)→ `const IGeometrySource&`。
4. 取数 `acquire_points()/acquire_triangles()/acquire_tetrahedra()` 连续 span,与整块 load 的 handle 逐元素等价(AC4)。

**回退载入**(默认 / 老工程):`lazyGeometry==false` 或无 assetId → 走 `rebuild_triangle_geometry`/tet 整块 get(:980-1011/1517-1539,现行逐字节),payload 持 handle,消费者经 `ResidentSurfaceSource` 取(M9a 路径)。

---

## 分层与依赖
- payload `geometryAssetId`:core(`AssetId` 已 core),无新依赖。
- reader opt-in:io,**不引 services、不引 vtk**;`XQProjectReadOptions` 在 io 头。
- 解析助手:services(可见 `GeometryResourceManager` + core payload + io 的 `IGeometrySource`,符合现有 `services→io` PRIVATE 边)。
- 消费者迁移:app/services(持 manager),不改 renderer 公共签名(renderer 吃源入口是 A/B)。
- Source 1.0 公共签名零改动(static_assert 成立);新增 link 边方向合法无环。

## 验证策略
- **AC3 不物化**:惰性载入后断言 payload `hasGeometryAssetId()==true` 且 `hasTriangleGeometry()/hasVolumeTets()==false`;用 `BlobStore`/`get` 计数或注入探针证明几何 blob 未被整块解码。
- **AC4 等价**:同一工程载两次——eager(默认)出 handle、lazy 出源——逐元素对比 points/tris/faceId/tets 全等(副作用 `acquire_*`/`get` 取出后再 assert,**不进 assert**)。
- **AC2 clone**:持 handle clone 后改原 handle、验克隆不受影响(沿用现有 payload 测试);持 assetId clone 验 assetId 带过去且无几何分配。
- **AC5/AC6 不退化 + 兼容**:`lazyGeometry==false` round-trip 字节一致;老归档(无 assets 段,reader:2516-2524 Info 路径)、无 assetId asset 走整块 load 无变化。
- **AC7 分层**:grep io/core 无新增 `vtk`/`services` include;Source 头 static_assert 编译通过;link 边核对无环。
- 铁律:Release + 全量 ctest 全绿 + 假绿抽查。

## 风险
- **R1 A 的 spec 形态未定**:`GeometrySourceSpec` 字段(计数从 registry 推导 vs payload 携带)直接决定 E 助手入参。**缓解**:E 开工前置 = 对齐 A 的 design;A 未合入不开工(父 prd 依赖序)。
- **R2 reader 改动面 + ripple**:`load` 加重载,所有调用方需默认参数保兼容;surface/mesh 两条 rebuild 路径都要插惰性分支,易漏 tet 分支(:1517-1539)。**缓解**:分阶段、每阶段编译驱动 + round-trip 回归。
- **R3 reader 段解析坑**(memory `xq-section-header-peek-needs-first-token`):若惰性档改变了几何 blob 的 peek/计数,易触发假 ParseError。**缓解**:惰性只跳过 `store.get`,**不改文本段解析顺序**(blob 行仍解析进 registry,只是不物化);debug 这类问题写 probe 塞 CMake 跑,别裸 cl 手链。
- **R4 假绿**(memory `no-sideeffect-in-assert`):AC4 等价对比把 `acquire_*`/`get` 结果先取出再断言;`/DNDEBUG` 抽查确认断言真删不掉副作用。
- **R5 完整性默认 FullVerify**:几何无 sidecar,惰性源 FullVerify 首触发全量 hash 一次(A 行为);大几何首次 acquire 有 hash 成本(非本子任务优化目标,记录即可)。
