# M9b-A 设计 — 几何 mmap 源 + acquireGeometrySource + borrow-faceId

> 依据本会话实读 `MappedVoxelSource`/`GeometryResourceManager`/`ReadLease`/`MmapBlob`/`MerkleSidecar`/`probe MappedGeometrySource`/`XQTriangleSurfaceGeometryHandle`/`XQProjectWriter` 核实的坐标。三块改动彼此咬合,但都是对 Source 1.0 的纯增量「加不破」。

## 架构总览

```
                       ┌───────────────────────────────────────────────┐
  app / E(后续)──────►│ GeometryResourceManager (services)             │
  acquireGeometrySource│   assetId + GeometrySourceSpec → GeometrySourceHandle
                       │   ResidentBlock 泛化:voxel | geometry 二选一  │
                       │   pin/use_count gating + LRU + 整 asset 多 blob 预算
                       └──────────────────┬────────────────────────────┘
                                          │ 构造(.cpp,PRIVATE 链 io)
                                          ▼
                       ┌───────────────────────────────────────────────┐
                       │ MappedGeometrySource (io)  : IGeometrySource    │
                       │   map_file_readonly × {points,tris,faceId,tets} │
                       │   verify_range(FullVerify 默认, memo)           │
                       │   reinterpret 零拷贝 → borrow(keepalive=mmap 块)│
                       └──────────────────┬────────────────────────────┘
                                          │ 借出
                                          ▼
                       ┌───────────────────────────────────────────────┐
                       │ TriangleLease (core, ReadLease.h)               │
                       │   既有 borrow(物化 faceId) ── 不动              │
                       │   新增 borrow-faceId(第二 keepalive,零拷贝)   │
                       └───────────────────────────────────────────────┘
                                          ▲
                       ResidentSurfaceSource 也改走 borrow-faceId(消灭 O(N) 物化)
```

三块落点:`ReadLease.h`(core)→ `MappedGeometrySource.{h,cpp}`(io)→ `GeometryResourceManager.{h,cpp}`(services)+ `XQTriangleSurfaceGeometryHandle` 加访问器 / `ResidentSurfaceSource.cpp` 接线(core)。

---

## 决策 1:borrow-faceId = TriangleLease 加第二 keepalive 成员 + 新静态工厂(不破原 borrow)

### 背景(实读)
`ReadLease.h:124-158` `TriangleLease` 现状:单 `keepalive_`(:155)、`ownedFaceIds_`(:156)、`view_`(:157);`borrow(keepalive, triangles, faceIds)`(:136-147)把 triangles 借出、把 `faceIds` 这个 owned `vector<int>` 搬进 `ownedFaceIds_` 并令 `view_.faceIds` 指向它。这正是 `ResidentSurfaceSource.cpp:44-51` 每次 acquire O(N) 物化、以及 probe `MappedGeometrySource.cpp:74-77` 每次拷 ~80MB 的根因。

### 取舍
- **方案 A 改 `borrow` 让 faceIds 收 span**:破坏既有签名与所有调用方(writer/renderer/tetgen 已迁的 M9a 代码),违反「加不破」。✗
- **方案 B(选定)新增独立静态工厂 + 第二 keepalive 成员**:
  ```cpp
  // 新增,既有 borrow / empty / 成员一字不改
  static TriangleLease borrow_borrowed_faceids(
      std::shared_ptr<const void> triKeepalive,
      ReadSpan<SourceTriangle> triangles,
      std::shared_ptr<const void> faceIdKeepalive,
      ReadSpan<int> faceIds)
  {
      TriangleLease lease;
      lease.keepalive_ = std::move(triKeepalive);
      lease.faceIdKeepalive_ = std::move(faceIdKeepalive); // 新成员
      lease.view_.triangles = triangles;
      lease.view_.faceIds = faceIds;   // 直接借出,ownedFaceIds_ 保持空
      return lease;
  }
  // 新私有成员:
  std::shared_ptr<const void> faceIdKeepalive_;
  ```

### 论证「不破」
- 既有 `borrow`(物化路径)走 `ownedFaceIds_`、`faceIdKeepalive_` 为 null;新工厂走借出、`ownedFaceIds_` 空。两条路互不干扰。
- `TriangleView`(`SourceViews.h:100-103`)不变——`faceIds` 本就是 `ReadSpan<int>`,借出与物化对消费者**完全透明**(都是连续 span)。消费者代码零改动。
- move-only + defaulted move 仍成立:新增 `shared_ptr` 成员的 move 是平凡的;`std::vector` move 保持 data 指针(原注释 ReadLease.h:20-22 的不变量对 `ownedFaceIds_` 空时同样成立,因为 borrow 路径不依赖它)。
- `SourceViews.h` 的 `static_assert` 与 `IGeometrySource` 纯虚集均不涉及 `TriangleLease` 成员布局 → 不受影响。
- keepalive 双成员的 eviction-gating 语义:triangles 与 faceId 可能来自**不同 blob 的不同 mmap 控制块**(几何源场景),故需两个独立 keepalive 各自 pin,任一存活都禁止对应 blob 被回收。Resident 场景两者都=handle 的 shared_ptr(同一对象,持两份引用无害)。

### Resident 侧接线
`XQTriangleSurfaceGeometryHandle.h:59` 已把 faceId 存成连续 `triangleFaceIds_`,只缺访问器。加:
```cpp
const std::vector<int>& triangleFaceIds() const; // 加不破,返回内部连续缓冲
```
`ResidentSurfaceSource.cpp:37-52` 改为:
```cpp
const std::vector<int>& fids = handle_->triangleFaceIds();
return TriangleLease::borrow_borrowed_faceids(
    handle_, ReadSpan<SourceTriangle>(tris.data(), tris.size()),
    handle_, ReadSpan<int>(fids.data(), fids.size()));
```
消灭 `:44-51` 的物化循环。两个 keepalive 都传 `handle_`(同一 shared_ptr)。

---

## 决策 2:MappedGeometrySource 照 MappedVoxelSource 范式,多 blob 各自 map + 各自 keepalive

### 形态
一个 `MappedGeometrySource` 对应一个几何 asset,内部按 role 各持一个 `MappedFile`:
```cpp
struct BlobSpec { std::string path; std::size_t elementCount; int components;
                  BlobElementType elem; };  // 缺省 path 空 = 该 role 缺失
// 构造:surface 三件(points/tris/faceId)+ tet 两件(volPoints/tets),按需 map
```
- 构造期对每个存在的 role `map_file_readonly`;校验 `mapped_.size == elementCount * components * sizeof(elem)`(points: count·3·8;tris: count·3·4;faceId: count·1·4;volPoints: count·3·8;tets: count·4·4)。任一不符 → `valid_=false`。
- 元素类型守门:points/volPoints 必须 F64x3、tris I32x3、faceId I32x1、tets I32x4(与 writer `put_triangle_geometry` 落盘格式一致),否则 invalid。

### 读路径(零拷贝 reinterpret + borrow)
- `acquire_points`:`verify_range(points blob 全范围)` → `reinterpret_cast<const Point3*>(points_.base)`(基址页对齐满足 8B,`static_assert(sizeof(Point3)==24)` 在 cast 点)→ `GeometryLease<Point3>::borrow(points_.keepalive, span)`。
- `acquire_triangles`:verify tris + faceId 两 blob → tris reinterpret `const SourceTriangle*`、faceId reinterpret `const int*` → **决策 1 的 `borrow_borrowed_faceids`**,两个 keepalive 分别是 `tris_.keepalive` 与 `faceId_.keepalive`。零拷贝两端。
- `acquire_tetrahedra`:verify tets → `reinterpret_cast<const SourceTet*>` → `borrow(tets_.keepalive, span)`。
- 缺失 role → 对应 acquire 返回 empty lease(部分源,`IGeometrySource` 契约)。

### 完整性(默认 FullVerify,惰性 memo,逐 blob)
照 `MappedVoxelSource::verify_range`(`.cpp:91-149`)逐 blob 复制一份:每个 blob 有独立 `fullVerified_` 标志与 `IntegrityStats`;FullVerify 首次触碰整块 `Sha256::hashHex` 一次置位,再触不重算。`SegmentedMerkle` 读路径并存(blob 旁有 `.merkle` 时走 `MerkleSidecar::verify_segment`),但**写路径 out-of-scope**,缺 sidecar 不报错、回落 FullVerify。

> 不复用 `BlobStore::get`(逐元素 readLE 解码 + 整块重 hash);map 一次 + reinterpret(scale-probe 缺陷②结论)。

---

## 决策 3:ResidentBlock 泛化 = 两个互斥 unique_ptr,共用 pin/bytes/LRU

### 背景
`GeometryResourceManager.h:106-111` `ResidentBlock` 现持 `unique_ptr<IVoxelSource> source`。`IVoxelSource` 与 `IGeometrySource` **无公共基类**(各自独立契约),不宜强造基类(会污染 core)。

### 取舍
- **方案 A 造公共 `ISource` 基类**:侵入 core 两个已冻接口,违反「加不破」。✗
- **方案 B(选定)ResidentBlock 持两个互斥 unique_ptr**:
  ```cpp
  struct ResidentBlock {
      std::unique_ptr<IVoxelSource> voxelSource;       // 二者
      std::unique_ptr<IGeometrySource> geometrySource; // 互斥
      std::shared_ptr<void> pin;
      std::size_t bytes = 0;
      std::list<AssetId>::iterator lruIt;
  };
  ```
  pin / bytes / lruIt / LRU 链 / `evictToBudgetLocked`(`.cpp:126-147` `use_count()>1` 跳过)**完全复用**——驱逐逻辑只看 pin 与 bytes,与源类型无关。`blocks_`/`lru_`/预算 同一套。

### 工厂
```cpp
struct GeometrySourceSpec {
    // 每 role 的解码元数据(从 AssetRecord.blobs 的 BufferRef 取,或调用方给)
    // surface: pointCount/triCount;tet: volPointCount/tetCount;缺省 = 该 role 缺失
    std::size_t pointCount=0, triCount=0, volPointCount=0, tetCount=0;
    bool hasSurface=false, hasTet=false;
    bool segmented=false; // 默认 FullVerify
};
class GeometrySourceHandle { /* 与 VoxelSourceHandle 对称,copy 即加 pin */
    const IGeometrySource& source() const; const IGeometrySource* operator->() const; };
GeometrySourceHandle acquireGeometrySource(const AssetId&, const GeometrySourceSpec&);
```
- **命中**:`blocks_.find(id)` 命中且该块是 geometry → bump LRU + 发新 pin,`++hitCount_`,不重映射(AC3)。
- **未命中**:经新 helper(仿 `voxelsRelPath`)从 `registry_->find(id)->blobs` 按 role(`points`/`tris`/`faceId` 或 `surfPoints`/`surfTris`/`surfFaceId` + `volPoints`/`tets`)解析各 `BufferRef.relPath` → 拼 `assetRootDir_ + "/" + relPath` → 构造 `MappedGeometrySource` → `valid()` 否则返回 invalid handle → resident。
- **预算字节**:`bytes = Σ blob.byteCount`(该 asset 全部几何 blob,points+tris+faceId[+volPoints+tets]),非单 blob(AC4)。
- `evictToBudgetLocked` 在 acquire 末尾跑(同 voxel),刚 acquire 的块被 pin 保护(`use_count==2`)存活。

---

## 数据流(acquire_triangles,mmap 路径)

```
acquireGeometrySource(assetId, spec)
  └─ miss → resolve roles(registry.blobs)→ new MappedGeometrySource(blobSpecs, FullVerify)
              └─ map_file_readonly(points/tris/faceId) → 校验 size == count*comp*sizeof
  └─ resident(bytes=Σ byteCount)+ LRU + pin → GeometrySourceHandle(copy pin)

handle->acquire_triangles()
  └─ verify_range(tris blob)   [FullVerify: 首次 hash 一次 memo]
  └─ verify_range(faceId blob) [同]
  └─ tris   = reinterpret_cast<const SourceTriangle*>(tris_.base)   零拷贝
  └─ faceId = reinterpret_cast<const int*>(faceId_.base)            零拷贝
  └─ TriangleLease::borrow_borrowed_faceids(tris_.keepalive, triSpan,
                                            faceId_.keepalive, faceIdSpan)
       view().triangles / view().faceIds 直指 mmap 区间,两 keepalive 各 pin 各 blob
```
lease 存活 → 两 blob 的 mmap 控制块 `use_count>1` → manager 的 per-block pin 也 `use_count>1` → 双层都禁驱逐;lease 析构后才可回收。

---

## 风险
- **R1 mmap 对齐 / 端序**:`Point3` 需 8B 对齐,`MapViewOfFile` 基址页对齐满足(probe 实证);非小端主机 reinterpret 不成立——加 `endianness==0` 守门(`BufferRef.h:29`),非小端 → invalid(不在本机出现,但显式拒绝胜过静默错)。
- **R2 pin-before-evict 竞态**(memory `evict-harness-pin-before-evict-race`):AC4 测试里 evictor 检 `use_count==1` 前前台必须先 pin、用并发信号门控,否则提前回收崩进程。沿用 voxel 侧「acquire/evict 共 `mutex_`」即可,测试编排注意。
- **R3 假绿**(memory `no-sideeffect-in-assert`):`verify_range`/`acquire_*` 副作用调用绝不进 `assert`;FullVerify 校验失败要走可被 Release 断言检出的返回值路径,不能靠 `assert`。
- **R4 faceId 物化点遗漏**:`ResidentSurfaceSource` 改 borrow 后,确认 writer/renderer 等 M9a 消费者仍透明(它们只读 `view().faceIds` span);grep 确认无其它代码依赖「faceId 是 owned 拷贝」语义。
- **R5 ResidentBlock 泛化的析构顺序**:两 unique_ptr 互斥,erase 块时 pin 已 `use_count==1`,源析构触发 mmap 解映射(view→map→file 序,`MmapBlob` 内 deleter 保证);确认无 lease 仍引用(gating 保证)。
