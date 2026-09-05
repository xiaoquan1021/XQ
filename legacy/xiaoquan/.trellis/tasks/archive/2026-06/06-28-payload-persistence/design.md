# Design — 资产身份 + payload 实体持久化(v2 重型基线)

> 配合 `prd.md`(v2)。旧版见 `design.old-lightweight.md`。
> 本任务范围:**身份 + 格式 + 数据落盘**;**不碰运行时内存访问**(IVoxelSource/IGeometrySource/驻留/淘汰全部 out of scope)。

## 0. 设计决策(已与用户敲定,实现照做)

| # | 决策 | 取舍理由 |
|---|---|---|
| D1 | **主档延续自定义纯文本逐行**,在 `endScene` 后新增 `assets` 段 | reader/writer 增量扩展,改动最小,向后兼容易做;不重写为 XML |
| D2 | **schema `1.1`→`1.2`;`minimumReaderVersion` 也升 `1.2`(严格)** | assets 段视为**必读**;新档被旧 reader 拒(UnsupportedVersion)。**reader 须真正校验 minReader**(现状未校验) |
| D3 | **大型数组 → 专用资产目录 `<stem>.assets/` 下内容寻址 blob**:`blobs/<前2位>/<sha256>.bin` | 天然去重;为 `original/`(Managed Original Source)、`canonical/`(Managed Canonical Volume)、便携打包预留干净结构 |
| D4 | **SHA-256 走 header-only vendoring**(许可证兼容、有测试向量,如 picosha2.h)进 `third_party/`;由 xq_io 封装**内部流式哈希接口**(支持分块) | 代码少;io 仍零 VTK / 零重依赖(header-only 不引动态库) |
| D5 | **`assetId` = 独立稳定标识**(与 blob 内容 SHA 解耦);**NodeId/AssetId 强类型、不可隐式转换** | 资产重定位 / external→managed / 内容重算时 assetId 不变(符合身份语义);blob 的 SHA 单独管完整性;类型隔离防 id 串用 |
| D6 | **blob 无头**:文件只存**规范化 little-endian 原始字节**,不附自定义文件头。`elementType/components/elementCount/byteCount/endianness/formatVersion/SHA-256` **全部由 BufferRef(主档)记录** | 避免 blob 头与主档元数据**双重真相**;无头裸字节**后续可直接 mmap**;`Point3`/`std::array` 逐字段显式 little-endian 编码,**不 dump C++ ABI** |
| D7 | **保存事务顺序**:先生成并原子发布全部 blob → 生成临时主档 → 验证全部引用 → **原子替换 `.xqproj`**;未引用旧 blob 本任务**不自动删**(GC 推后) | 失败不留半完整工程;原子发布保证主档与 blob 一致 |
| D8 | **加载失败语义(严格)**:含资产语义的工程,blob 缺失/损坏 → **整体加载失败 + 结构化错误**,**不返回无 payload 的半完整工程** | 半完整工程比明确失败更危险;静默丢实体会让上层误以为数据完整 |

## 1. core 新增资产层(最小)

文件(新增,放 `src/core/asset/`,纯值类型,零依赖):

```cpp
// AssetId.h — 独立稳定身份(非内容哈希)。强类型,不可隐式转 NodeId。
class AssetId {
    // ValueType 用 std::uint64_t(对齐 NodeId 风格);serialize/deserialize 同 NodeId
    // 生成:工程内单调递增(AssetRegistry 持 next_id_),或外部注入
    // explicit 构造 + 无到 NodeId/整数的隐式转换(D5 类型隔离,S1 须测)
};

// BufferRef.h — 主档对 sidecar blob 的引用 + blob 的全部自描述元数据(D6:blob 无头)
struct BufferRef {
    std::string relPath;          // 相对 .xqproj 目录,如 "foo.assets/blobs/ab/abcd….bin"
    std::uint64_t byteCount;      // blob 字节数(= elementCount*components*sizeof(elementType))
    std::string sha256;           // 64-hex,blob 全字节的 SHA-256(内容寻址 = 文件名)
    std::uint32_t formatVersion;  // 编码格式版本(= 1)
    std::uint8_t  endianness;     // 0=little(基准)
    std::uint8_t  elementType;    // enum{ f64=1, f32=2, u32=3, u64=4, u8=5, i32=6 }
    std::uint16_t components;     // Point3=3, Tri=3, Tet=4, faceId=1, voxel=1
    std::uint64_t elementCount;   // 元素个数(点数/三角数/tet 数/体素数)
};

// AssetRecord.h
enum class AssetCategory { ExternalSource, ManagedOriginal, ManagedCanonical, Derived };
enum class AssetKind {     // 资产承载的数据种类
    Image, SegmentationMask, Surface, Mesh, SimulationCase, FlowResult, AiAnalysis, Path, Contour
};
struct AssetRecord {
    AssetId id;
    AssetCategory category;
    AssetKind kind;
    // External 定位(category==ExternalSource 时有效):
    std::string sourceAbsPath;       // 绝对路径(可空)
    std::string sourceRelPath;       // 相对 .xqproj(可空)
    DicomSeriesIdentity dicom;       // study/series/frameOfReference UID(复用 XQImageVolume.h 已有结构)
    bool hasDicom;
    std::string contentFingerprint;  // 内容指纹(影像源的 SHA-256 或等价,登记用,不解码)
    bool hasGeometry;                // External 影像登记必要几何元数据
    ImageGeometry geometry;          // dims/spacing/origin/direction/coordSys(复用 core 已有结构)
    // Derived/Managed 实体引用:0..N 个 blob(一个资产可拆多数组,如 surface 的 points+tris+faceId)
    std::vector<std::pair<std::string /*role*/, BufferRef>> blobs; // role: "points"/"tris"/"faceId"/"voxels"/"volPoints"/"tets"/...
    std::string displayName;         // 可空
};

// AssetRelation.h — 资产级派生血缘(数据怎么算出来的)
struct AssetRelation { AssetId source; AssetId derived; }; // 影像→掩膜→表面→体网格→流场

// AssetRegistry.h — 只管身份/类型/存储描述/定位/sidecar 引用/元数据/血缘
class AssetRegistry {
public:
    AssetId createAsset(AssetCategory, AssetKind);        // 分配稳定 id
    AssetRecord* find(const AssetId&);                     // 非 const 取以填 blob/元数据
    const AssetRecord* find(const AssetId&) const;
    void visit_assets(const std::function<void(const AssetRecord&)>&) const;
    void addRelation(const AssetId& source, const AssetId& derived);
    void visit_relations(const std::function<void(const AssetId&, const AssetId&)>&) const;
    // 明确不含:load/acquire/cache/evict/budget/mmap — 那些是下一阶段
private:
    std::map<AssetId, AssetRecord> records_;
    std::vector<AssetRelation> relations_;
    AssetId::ValueType next_id_;
};
```

`AssetRegistry` 挂在 `XQProject` 上(`XQProject::assetRegistry()` get/非 const get)。
具体 setter/getter 粒度实现时定;**禁止**在 AssetRegistry 出现任何加载/缓存/内存语义的方法(AC1 审计点)。

## 2. `XQDataNode` 增加可选 assetId 引用

```cpp
// XQDataNode 增:
void setAssetId(const AssetId& id);
bool hasAssetId() const;
const AssetId& assetId() const;
// 私有: AssetId assetId_; bool hasAssetId_;
```

`NodeId` 语义不变;节点级 `link_derived`/stale **不重构**。同一 `assetId` 可被多个 node 引用(不强制唯一)。
> 给 `XQDataNode` 加成员要同步:复制/相等语义(若有)、所有构造它的测试/mock(铁律:接口加方法补全 stub)。

## 3. 主档新增段(纯文本)

版本头改:
```
XQ_NATIVE_PROJECT schemaVersion 1.2
writerVersion XQ-M9-001
minimumReaderVersion 1.2          # D2 严格:旧 reader 见此 > 自身能力 → 拒
...
```

段顺序:`scene` → `endScene` → **`assets`(新增)** → `provenance` → `diagnostics` → `end`。
**`scene` 段的 `node` 行结构不动**(D 修正:不扩展 node 行)。Node↔Asset 绑定用**独立记录**
(放 `assets` 段内,见下),旧 4-token node 行保持原样 → reader 的 node 解析逻辑零改动。

`assets` 段文法:
```
assets <N>
asset <assetId> category <Cat> kind <Kind> name <present 0|1> <encoded?>
  external <0|1> [absPath <present><enc?> relPath <present><enc?> dicom <0|1> [study <enc> series <enc> frame <enc>] fingerprint <present><enc?>]
  geometry <0|1> [dims 3i spacing 3d origin 3d direction 9d coordSys LPS|RAS]
  blobs <B>
  blob <encodedRole> <encodedRelPath> <byteCount> <sha256> <formatVersion> <endianness> <elementType> <components> <elementCount>   (×B)
endAsset
... (×N)
nodeAssets <M>
nodeAsset <nodeId> <assetId>      (×M)   # Node↔Asset 绑定:独立记录,不改 node 行
relations <R>                     # 资产级血缘(与 scene 段的 node relations 区分)
assetRel <sourceAssetId> <derivedAssetId>   (×R)
endAssets
```

> **blob 引用行携带 BufferRef 全字段**(D6:blob 无头,元数据全在主档):
> `blob <role> <relPath> <byteCount> <sha256> <formatVersion> <endianness> <elementType> <components> <elementCount>`。

可空串编码沿用 field-spec §3 决定:`<present 0|1> <encoded>`(避免 `-` sentinel 与合法值冲突);
double 用 `setprecision(17)`;NodeId/AssetId 用各自 serialize。

> **大型数组不出现在主档**(AC4 铁律):surface 的 points/tris/faceId、mesh 的 surf/vol points/tets/
> boundary faces、segMask voxels 全部进 blob,主档只有 `blob <role> <relPath> <byteCount> <sha256>` 引用行。
> simCase/flowResult/aiAnalysis/path 的小标量(参数/时序/指标)留主档文本还是下沉 sidecar:
> **按数组规模**——flowResult 的 q/p/a 矩阵([segment][time])规模大 → sidecar;simCase 参数、path 控制点
> 量小 → 可留主档文本(沿用 field-spec 取舍)。具体每字段归属见 §5 表。

## 4. BlobStore:内容寻址无头规范化二进制(每个 blob 文件)

**blob 文件 = 纯规范化 little-endian 原始字节,无任何文件头**(D6)。所有自描述元数据
(formatVersion/endianness/elementType/components/elementCount/byteCount/SHA-256)由**主档 BufferRef** 记录,
不在 blob 内重复(避免双重真相;无头裸字节后续可直接 mmap)。

- **规范化编码**:`Point3` 写 3×f64 little-endian;`Tri` 写 3×i32;`Tet` 写 4×i32;`faceId` 写 i32;
  voxel 写 u8。**逐字段显式写,绝不 memcpy struct / std::array**(跨 ABI 稳定)。
- **写(BlobStore::put)**:从现有 payload handle 整块取(`points()`/`tets()`/`voxels()` 等)→ 按 elementType
  逐元素逐分量 little-endian 编码到内存/临时文件,**同时流式 SHA-256** → 临时文件 fsync →
  **原子重命名**到 `blobs/<sha[0:2]>/<sha>.bin`;若目标已存在(同内容)→ 跳过写(去重),返回同一 BufferRef。
- **内容寻址**:文件名 = 全字节 SHA-256。BufferRef 记 relPath/byteCount/sha256 + 编码元数据。
- **读(BlobStore::get + verify)**:按 relPath 读全字节 → 校验 `文件大小 == byteCount`、流式 SHA-256 == BufferRef.sha256
  → 按 elementType/components/elementCount **解码**回 vector。
  四类结构化错误(S2 须各有测试):
  - `MissingBlob`(文件不存在)
  - `TruncatedBlob`(实际字节 < byteCount,或不足以解出 elementCount×components×sizeof)
  - `ByteCountMismatch`(文件大小 ≠ byteCount)
  - `ChecksumMismatch`(SHA-256 不符)
- **SHA-256**:vendoring header-only(D4)→ xq_io 封装**内部流式哈希接口**(`update(bytes)` / `final()→hex`),
  支持一次性与分块,二者结果须一致(S2 测)。标准测试向量须过(空串 / "abc" / 长串)。
- **elementType↔C++**:f64=double、i32=int(int 固定按 32 位编码,读回 `static_cast<int>`)、u8=uint8_t。

## 5. 各 payload 数组归属(主档文本 vs sidecar blob)

| payload | 留主档文本(小标量) | 下沉 sidecar blob(大数组,role) |
|---|---|---|
| source(XQSourcePayload) | domainToken + sourcePath(其实归入 External Asset) | — |
| path | id/interp/sampleSpacing/sourceImage + 控制点(量小) | —(控制点通常 < 数百,留文本) |
| segMask | dims/geometry/labels/sourceImage | `voxels`(uint8) |
| surface | id/source/sourceContour/preserved/faces 元数据 | `points`(f64×3)/`tris`(i32×3)/`faceId`(i32×1) |
| mesh | id/sourceModel/preserved/quality/regions/boundaryFaces 元数据 | `surfPoints`/`surfTris`/`surfFaceId`/`volPoints`/`tets`(i32×4) |
| simCase | 全部(RCR/波形/Rom/Fluid/Solver,量小) | —(波形采样若极大可下沉,默认留文本) |
| flowResult | sourceCase/converged/maxCfl/times/segments 元数据 | `flowQ`/`pressureP`/`areaA`(f64,[seg×time] 展平,大) |
| aiAnalysis | 全部(metrics/annotations,量小) | — |

字段级 getter/setter 清单见 `field-spec.md`(实读 core 头的事实基线);本任务把其中**大数组那几行从
"主档逐行" 改为 "blob 引用"**,其余文本布局沿用。

## 6. 错误模型(结构化,AC5)

`XQProjectReadResult.diagnostics` push 带 code 的 Diagnostic;`XQProjectReader::Status` 区分:
- `PROJECT_NO_ASSET_DATA`(Info):旧 1.1 档无 assets 段 → load 成功、资产空(AC7)。
- blob 损坏(对应 BlobStore 四态)→ **整体加载失败**(D8 严格):
  `ASSET_BLOB_MISSING` / `ASSET_BLOB_TRUNCATED` / `ASSET_BLOB_BYTECOUNT_MISMATCH` /
  `ASSET_BLOB_CHECKSUM_MISMATCH`(Error)→ Status::ParseError(或专用失败 Status,实现时定)+ 诊断。
  **绝不返回"无 payload 的半完整工程"**:含资产语义的工程,任一引用 blob 坏 = load 失败。
- 引用完整性(reader 两段式后统一校验):`nodeAsset` 指向不存在的 assetId、`assetRel` 端点缺失、
  blob role 重复等 → `ASSET_REFERENCE_INVALID`(Error)→ 整体失败。
- `PROJECT_UNSUPPORTED_SCHEMA_VERSION`(Error):major > 支持 **或** 档案 minimumReaderVersion > reader 能力
  (D2 严格)→ UnsupportedVersion。

## 7. reader/writer 改造要点(基于现有 618/149 行)

**writer**(`XQProjectWriter.cpp`)—— 保存事务顺序(D7):
1. 遍历 scene 节点取 `payload()` → 按 kind 把大数组经 **BlobStore::put** 编码+发布 blob(原子重命名),
   收集每个资产的 BufferRef(全字段);小标量留主档 assets 文本。
2. **全部 blob 原子发布完成后**,生成**临时主档**:版本 1.2 / writerVersion / minimumReaderVersion 1.2;
   scene 段(node 行结构不动)→ assets 段(AssetRecord + blob 引用行 + `nodeAssets` 绑定 + `assetRel` 血缘)
   → provenance → diagnostics → end。
3. **验证临时主档引用的全部 blob 均已存在**(自检)→ **原子替换** `.xqproj`。
4. 未引用的旧 blob **本任务不自动删**(GC 推后)。
- 现状 writer 把 projectId/record 写死占位 → 维持现状占位(不在本任务扩范围)。

**reader**(`XQProjectReader.cpp`)—— 两段式(D 修正:先全解析,再统一验证):
- `parse_project` 版本校验补 **minimumReaderVersion 比对**(D2);定义 reader 能力版本常量(如 `kReaderVersion={1,2}`)。
- **node 行解析逻辑零改动**(绑定走独立 `nodeAsset` 记录,不在 node 行)。
- `endScene` 后 peek:`assets` → **第一段:纯解析**整个 assets 段到内存结构(AssetRecord 列表 / nodeAsset 列表 /
  assetRel 列表 / blob 引用),**不依赖字段顺序**;非 `assets` → 旧路径(1.1 档发 `PROJECT_NO_ASSET_DATA`)。
- **第二段:统一验证 + 重建**:校验所有引用完整性(nodeAsset/assetRel 端点存在、role 不重复)→
  对每个 blob 引用 **BlobStore::get+verify**(任一坏 → 整体失败,D8)→ 解码重建 payload 经 `setPayload` 挂回 node、
  `setAssetId` 绑定、AssetRegistry 填 records/relations。
- 复用 stash@{0} 的各 payload 解析块**思路**(重建逻辑可参考),但:几何从主档文本改为**从 blob 读**、
  checksum 从 FNV 改 **SHA-256**、外面套 AssetRecord、两段式。**是参考重写,不是直接 pop**。

## 8. 测试设计(`tests/io/test_payload_roundtrip.cpp`,link xq_io,无需 Qt)

- **AC3 实体级 round-trip**:构造含各 payload 的小工程(segMask 4×4×2、surface 4 点 4 三角、mesh 几个 tet、
  simCase 带 RCR/波形、flowResult 小矩阵、aiAnalysis、path),建 AssetRegistry + node↔asset 绑定 + 资产血缘
  → save → load → 逐字段断言:payload 数据/拓扑/几何元数据 + assetId + node↔asset 绑定 + 资产血缘一致。
- **AC4 大数组不进主档**:save 后读主档文本,断言**不含**逐点/逐 tet 行(grep `^pt `/`^tet ` 应为 0);
  断言 `blob ` 引用行存在、对应 `<stem>.assets/blobs/...` 文件存在。
- **AC5 blob 损坏 → 整体失败**(D8):分别(a)删 blob(b)截断 1 字节(c)改 byteCount(d)翻 1 字节破 SHA →
  load **整体失败**(对应 `ASSET_BLOB_*` code),**不返回半完整工程**,不崩。
- **AC6 影像 External Asset**:登记 image 资产(absPath/relPath/DICOM UID/fingerprint/geometry)、**不写体素 blob**
  → round-trip 后元数据一致;断言无 image 体素 blob 文件。
- **AC7 向后兼容**:手工 1.1 档(无 assets 段)→ load Status::Ok + 资产空 + `PROJECT_NO_ASSET_DATA` 诊断。
- **D2 严格**:手工档 `minimumReaderVersion 1.3`(> reader 能力)→ UnsupportedVersion + 诊断。
- 铁律:`reader::load` 等副作用调用**先取变量再判断,绝不进 assert**(Release /DNDEBUG)。
- **假绿点**(AC9):reader 故意漏读 tet 第 4 索引 / 漏校验 SHA → round-trip 测试 Release FAIL。

CMake:`add_executable(test_payload_roundtrip tests/io/test_payload_roundtrip.cpp)` +
`target_link_libraries(... PRIVATE xq_io)` + `add_test`;vendoring 的 picosha2.h 路径加进 include。

## 9. 必改的现有测试(AC10,§prd 7)

`test_project_versioned_save.cpp`:
- `test_current_v11_roundtrip_preserves_node_ids`:`schemaVersion 1.1`→`1.2`(:299);
  `result.diagnostics.empty()`(:315)——新档**有** assets 段(哪怕空),不发 `PROJECT_NO_ASSET_DATA`,保持 empty;
  确认 writer 对空工程也写 `assets 0`(空段)。`\nprovenance\n`(:303)若新格式保留 provenance 则不动。
- `test_same_major_higher_minor_loads`(档声明 minReader 1.0):升 reader minReader 校验后仍应 load(1.0 ≤ 能力),
  确认 D2 校验不误伤。**但该档无 assets 段(旧档)→ 按 prd §7 发一条 Info `PROJECT_NO_ASSET_DATA`**;
  原 `diagnostics.empty()` 断言**已改为**"恰好一条 Info、code==PROJECT_NO_ASSET_DATA"(S3 实测落地)。
- `test_v1_legacy_without_provenance_migrates`(1.0 档,无 assets 段):同上,
  `diagnostics.empty()` **已改为** "恰好一条 Info `PROJECT_NO_ASSET_DATA`"。
  > **矛盾消解(S3 决策,用户已拍板)**:prd §7"旧档发 Info"与本节早先"旧档测试断言不变"冲突。
  > 取 prd §7 为单一真相——旧档(schema<1.2 且无 assets 段)load 成功并发一条 Info;故上面两个手写旧档测试断言改为查这条 Info。
  > 新 1.2 档总有 assets 段(空也写 `assets 0`),round-trip **不**发该诊断,`empty()` 保持。

## 10. 不做 / 后续(对齐 prd Out of scope)

IVoxelSource/IGeometrySource、各 VoxelSource/GeometrySource 实现、GeometryResourceManager、按需驻留、
内存预算、SceneNode 去 vector 化、MutableGeometryBuilder、不可变资产版本化、LOD、规范化影像解码缓存、
Managed 导入流程、便携工程打包、.svproj 回写、跨大版本迁移、压缩/增量。

## 11. 实现步骤(S1~S4 串行,各带独立验收门)

> 规则:**每步必须完成构建 + 对应单元测试 + 全量 ctest + diff review,绿了才进下一步**。
> 允许同一 worker 连续执行,**禁止四层一次性改完只在最后统一验收**。

### S1 — core 资产身份层(不碰 reader/writer/sidecar)
实现 `AssetId` / `BufferRef` / `AssetRecord` / `AssetRelation` / `AssetRegistry` / `XQProject::assetRegistry()`;
`XQDataNode` 加 optional `assetId`。**NodeId 与 AssetId 强类型、不可隐式转换**。同一资产可被多 Node 引用;
非资产节点可无 assetId。
- **验收门**:新增 `test_asset_registry` —— 资产注册、**重复 ID 拒绝**、身份稳定、多节点引用、资产血缘、
  **NodeId/AssetId 类型隔离**(编译期不可互换,用 static_assert 或编译失败用例佐证);原有 core 测试不回归;全量 ctest 绿。

### S2 — 内容寻址 BlobStore(无头规范化,纯 io,无 reader/writer 接入)
vendor header-only SHA-256(许可证兼容、有测试向量)→ xq_io 封装**内部流式哈希接口**。实现:规范化
little-endian 编码、临时写、SHA-256 内容寻址、分片目录、原子发布、**重复内容复用**、读取+校验。
**blob 无头**(元数据全在 BufferRef)。
- **验收门**:新增 `test_blob_store` —— **标准 SHA-256 测试向量**、一次性 vs 分块哈希一致、规范化编码固定
  (同输入字节级稳定)、写读一致、相同内容去重、**四类结构化错误**(MissingBlob/TruncatedBlob/
  ByteCountMismatch/ChecksumMismatch);全量 ctest 绿。

### S3 — schema 1.2 + 资产元数据 round-trip(不接入实体 payload)
主档版本化行式纯文本,schemaVersion / minimumReaderVersion 均 1.2;新增 `assets` 段(AssetRecord +
AssetRelation + BufferRef);Node↔Asset 用独立 `nodeAsset <nodeId> <assetId>` 记录。reader **两段式**
(全解析 Scene+Assets+nodeAsset 后**统一验证引用,不依赖字段顺序**)。新 reader 兼容旧 1.1;新 writer 只写 1.2;
< 1.2 的 reader 拒含资产语义的新档。
- **验收门**:`test_asset_metadata_roundtrip`(AssetRegistry/BufferRef/血缘/Node 绑定主档 round-trip)+
  旧 1.1 兼容(`PROJECT_NO_ASSET_DATA`)+ `test_project_versioned_save` 改完绿 + D2 严格 minReader 拒绝;
  全量 ctest 绿。**此步不接入实体 payload。**

### S4 — 实体 payload sidecar 全链路
现有 segMask / Surface / Tet Mesh payload 接入 BlobStore:writer 把 vector 编码成规范化 blob + BufferRef 入
AssetRecord;reader 校验解码 blob 恢复**现有运行时 payload vector**(renderer/solver/services 接口不变)。
影像仍只 External Asset(路径/UID/指纹/几何,不写体素)。**保存事务顺序**(D7);blob 缺失/损坏 → **整体失败**(D8)。
- **验收门**:`test_payload_roundtrip` —— **逐字段**实体 round-trip(体素值/点坐标/三角/四面体连接/属性数组/
  影像几何/AssetId/Node 绑定/资产血缘,**不止比数量**)+ AC4 大数组不进主档审计 + AC5 损坏整体失败 +
  假绿抽查(漏读 tet 第4索引/漏校验 SHA → Release FAIL);全新 build + 全量 ctest 绿。
