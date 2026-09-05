# Implement — 资产身份 + payload 实体持久化(M8a,S1~S4)

> 接手先读:`prd.md` + `design.md`(均 v2 重型基线)+ `field-spec.md` + `CLAUDE.md` + 本机 memory。
> 构建/测试配方见 memory `xq-build-recipe`(vcvars64 + CMAKE_PREFIX_PATH + offscreen ctest)。
> **规则:S1→S2→S3→S4 串行,每步「构建 + 单元测试 + 全量 ctest + diff review」绿了才进下一步;
> 禁止四层一次性改完只在最后统一验收。** 旧版执行计划见 git 历史(已被本文件取代)。

## Step 0 — 接手核对(不写代码)
- [ ] `task.py current`;非本任务 → `task.py start payload-persistence`。
- [ ] `git status` 干净;`git log -5` 确认 master。**工作区必须从干净的 v2 基线开始**(`.claude/CLAUDE.md` 那类用户私改不算本任务,但别误提交)。
- [ ] **从当前正确基线创建独立 M8a v2 分支**(如 `feat/m8a-payload-persistence-v2`),S1→S4 在该分支串行重做。
- [ ] **记录 stash commit ID**:`git rev-parse stash@{0}` → 当前为 **`d5be339`**(全:`d5be339a0fc4a8372ebf2d328a762a24310cedd0`)。
      stash list / 索引会变,后续删除以**此 commit ID** 为准,**不假定它届时仍是 stash@{0}**。

### stash 处置纪律(硬约束,违反即返工)
- **禁止 `git stash pop` / `git stash apply`**。stash(`d5be339`)是 v1 半成品(几何写文本 + FNV sidecar),
  **只作只读历史参考,绝不作为 v2 代码基线**。旧架构不得进入新实现。
- 需参考 v1 时**只允许** `git stash show -p stash@{0}` 或对单文件看 diff(stash 触及 3 文件:
  `XQProjectReader.cpp` +1446 / `XQProjectWriter.cpp` +454 / `CMakeLists.txt` +7,**未碰 core/tests**)。
- **允许人工摘取**:与新架构无关、经确认的局部逻辑——payload 字段遍历顺序、解析错误结构、测试夹具。
- **禁止**:整体复制文件;复用 FNV;复用几何文本序列化;复用旧 sidecar 路径;
  绕过 AssetRegistry 的读写逻辑;任何把大型数组写进主档的行为。
- **保留时机**:M8a 全部步骤 + 全量测试 + diff review 完成前**保留该 stash**。

- [ ] 读 `XQProjectWriter.cpp`(149)/`XQProjectReader.cpp`(618):掌握现有逐行解析范式 + `endScene` 后 peek `provenance`(:465)的插入点。
- [ ] 读 `XQDataNode.h`(setPayload :34)、`GeometryTypes.h`(Point3=3×double)、各 payload 头(字段见 field-spec.md)。

## S1 — core 资产身份层
- [ ] 新增 `src/core/asset/`:`AssetId.h/.cpp`(uint64,serialize/deserialize 同 NodeId;**explicit、不可隐式转 NodeId/整数**)、
      `BufferRef.h`(relPath/byteCount/sha256 + formatVersion/endianness/elementType/components/elementCount)、
      `AssetRecord.h`(category/kind/external 定位/geometry/blobs[role,BufferRef]/displayName)、
      `AssetRelation.h`、`AssetRegistry.h/.cpp`(createAsset/find/visit_assets/addRelation/visit_relations;
      **禁含 load/acquire/cache/evict/budget/mmap**)。
- [ ] `XQProject` 加 `assetRegistry()`(const + 非 const)。
- [ ] `XQDataNode` 加 `setAssetId/hasAssetId/assetId`;同步**所有构造 XQDataNode 的测试/mock**(铁律:接口加字段补 stub)。
- [ ] CMake:把新 asset 源加入 `xq_core`;`add_executable(test_asset_registry ...)` + link xq_core + `add_test`。
- [ ] **验收门 S1**:`test_asset_registry`(注册 / **重复 ID 拒绝** / 身份稳定 / 多节点引用同一 asset / 血缘 /
      **NodeId↔AssetId 类型隔离**:写一个"传错 id 类型则编译失败"的佐证,或 static_assert 不可转换)→
      构建 + 全量 ctest 绿 + diff review。

## S2 — 内容寻址 BlobStore(无头规范化)
- [x] vendor header-only SHA-256 到 `third_party/`(许可证兼容、有测试向量,如 picosha2.h);CMake include 路径。
- [x] `src/io/blob/`:SHA-256 流式封装(`update/final→hex`,一次性 vs 分块结果一致)+ `BlobStore`:
      `put(elementType,components,elementCount,bytes源)`→ 规范化 LE 编码 + 流式 SHA + 临时写 + fsync +
      原子重命名 `blobs/<sha[0:2]>/<sha>.bin`(已存在则跳过=去重)→ 返回 BufferRef;
      `get+verify(BufferRef)`→ 读 + 校验大小/SHA + 解码回 vector;**blob 无头**。
- [x] 四类结构化错误:MissingBlob / TruncatedBlob / ByteCountMismatch / ChecksumMismatch。
- [x] **验收门 S2**:`test_blob_store`(标准 SHA-256 向量:空串/"abc"/长串 + 一次性=分块 + 规范化编码字节级稳定 +
      写读一致 + 同内容去重 + 四类错误各一例)→ 构建 + 全量 ctest 绿 + diff review。
      **(已实测:Release 47/47 全绿;假绿抽查 漏校验 SHA → FAIL + exe 时间戳真变 → 恢复 PASS;io 零 VTK 审计通过;
      picosha2 MIT vendoring 进 XQ/third_party/picosha2)**

## S3 — schema 1.2 + 资产元数据 round-trip(不接入实体 payload)
- [x] writer:版本 1.2 / minimumReaderVersion 1.2;`endScene` 后写 `assets` 段(AssetRecord + blob 引用行带全
      BufferRef 字段 + `nodeAssets`/`nodeAsset` 绑定 + `assetRel` 血缘 + endAssets)。**node 行结构不动。**
      空工程也写 `assets 0`(空段,不触发诊断)。
- [x] reader:补 **minimumReaderVersion 比对**(`kReaderMajor=1`/`kReaderMinor=2`,档要求 > 能力 → UnsupportedVersion);
      `endScene` 后 peek `assets` → **两段式**:先全解析 assets/nodeAsset/assetRel 到内存(不依赖字段顺序)→
      统一验证引用完整性(端点存在 / role 不重 / node 不双绑) → 填 AssetRegistry(registerAsset)+ setAssetId 绑定;
      非 `assets`(旧档)→ `PROJECT_NO_ASSET_DATA`(Info)+ load Ok。
- [x] CMake:`test_asset_metadata_roundtrip`(+ add_test)。
- [x] **改现有** `test_project_versioned_save.cpp`:`schemaVersion 1.1`→`1.2`;新档 round-trip 仍 `diagnostics.empty()`;
      **两个手写旧档测试**(`test_same_major_higher_minor_loads` / `test_v1_legacy_without_provenance_migrates`)
      原 `empty()` 断言**已改为**"恰好一条 Info `PROJECT_NO_ASSET_DATA`"(矛盾消解见下)。
- [x] **验收门 S3 已过**:全新 build_s3 全量 ctest **48/48**(原 47 + 新增 asset_metadata);
      逐字段元数据 round-trip(External dicom/geometry/fingerprint/path + Derived blob 引用 + 血缘 + node 绑定)、
      旧档 Info 诊断、D2 严格 minReader 1.3 拒、引用完整性失败整体 ParseError(ASSET_REFERENCE_INVALID);
      **假绿抽查**(漏读 blob `components` / 禁用 minReader 校验 → asset_metadata Release FAIL → 恢复 PASS,exe 时间戳真变);
      io 零 VTK 审计干净。**未接入实体 payload(blob 仅作元数据 round-trip,不读真实 blob)。**

### S3 关键坑/决策(沉淀)
- **段头 peek 用「行首 token」不是「整行单 token」**:`assets <N>` 行有 2 token,
  误用 `line_is_single_token` 会判 false → 跳过 assets 解析 → 后续 peek provenance 错位 → 假 ParseError。
  新增 `line_starts_with_token` 专门 peek 带计数的段头。**(已踩过并修复)**
- **诊断矛盾消解(用户已拍板)**:prd §7"旧档发 Info"vs design §9 早先"旧档测试断言不变"冲突。
  取 prd §7 单一真相:旧档(schema<1.2 且无 assets 段)load 成功并发一条 Info `PROJECT_NO_ASSET_DATA`;
  新 1.2 档总写 assets 段(空也写)→ round-trip 不发该诊断。design §9 已同步改写。

## S4 — 实体 payload sidecar 全链路

> 本段是**自带事实基线的执行清单**(已通读 reader/writer/BlobStore/全部 payload 头,新对话照此干,不必重读代码)。
> 串行原则不变:实现完 → 全新 build + 全量 ctest + 假绿抽查 + diff review 绿了才算完。

### S4.0 核心决策(已与用户拍板,不再开放)
- **payload→blob 转换由 writer 自动派生**(非调用方预填)。理由:现有 service 创建 AssetRecord、节点持 payload,
  但二者无自动绑定;主线 service 尚未产 asset,若靠调用方预填则实体落盘无从触发。故:
  **writer 遍历 `project.scene().visit_nodes`,对每个带"大数组 payload"的 node:
  在 AssetRegistry 里 createAsset(Derived, 对应 kind) → BlobStore::put 落 blob → BufferRef 入该 AssetRecord
  → node.setAssetId 绑定。** 小标量字段(见 design §5 表)进 assets 文本,不进 blob。
- **写时机**:writer 收到的 `project` 是 `const&`。要改 registry/绑定 → 在 writer 内**先深拷贝一份 `XQProject` 工作副本**
  (`XQProject` 拷贝语义已验证可带 scene+registry,见 S3 reader `*project = parsed`),在副本上 createAsset/put/setAssetId,
  再序列化副本。**绝不改调用方传入的 const project。**

### S4.1 BlobStore 根目录与 relPath 约定(已定)
- 资产目录 = `<projectFileStem>.assets/`,与 .xqproj 同目录。stem 取法:去掉 `.xqproj` 扩展名。
  例:`/tmp/foo.xqproj` → 资产目录 `/tmp/foo.assets/`。用 `std::filesystem::path` 推导(io 已用 `<filesystem>`)。
- `BlobStore(assetRootDir)` 的 `assetRootDir` 传**绝对路径** `<dir>/<stem>.assets`。
- `put` 返回的 `BufferRef.relPath` 形如 `blobs/<前2位>/<sha>.bin`(**相对资产目录根**,BlobStore 已这么给)。
  **主档 BufferRef 直接存这个 relPath 不动**;reader 用**同样的 `<stem>.assets` 根**构造 BlobStore 即可 get。
  (即:relPath 基准 = 资产目录根,不是 .xqproj 目录。design §3 示例写的 `foo.assets/blobs/...` 是另一种基准——
  **本任务统一采用"相对资产目录根"**,writer/reader 两端一致即可,已自洽。)

### S4.2 payload 类型判别与字段事实基线(从头文件实摘,直接用)
- **判别子类**:`XQPayload` 只有 `domainType()`(返回 `XQDomainType` 枚举),**无 RTTI tag**。
  用 `payload->domainType()` switch + `std::dynamic_pointer_cast<具体Payload>(node.payload())` 取具体类型。
- **大数组 payload(进 blob)**:仅以下三类带大数组:
  - `XQDomainType::SegmentationMask` → `XQSegmentationMaskPayload`(`.mask()` → `XQSegmentationMask`)
  - `XQDomainType::SurfaceModel` → `XQSurfaceModelPayload`(`.model()` → `XQSurfaceModel`)
  - `XQDomainType::Mesh` → `XQMeshPayload`(`.mesh()` → `XQMesh`)
- **小标量 payload(全留 assets 文本,不进 blob,但仍要 round-trip)**:
  source / path / simCase / flowResult(注:flowResult 的 q/p/a 矩阵 design §5 说"大→sidecar",
  但**实测其为 `vector<vector<double>>`,落盘按 design §5 仍可 sidecar;S4 为简先留文本逐行**,
  规模真大是后续事;**若你判断要 sidecar 也可,但务必 reader 对称**)/ aiAnalysis。
  → **决策:S4 先把 flowResult q/p/a 也留主档文本逐行(series 文法见 field-spec §1 flowResult)**,
    避免引入"矩阵展平进 blob"的额外复杂度;真大矩阵下沉留后续。其余小标量同。

#### blob role / 编码类型对照(BlobElementType 见 BufferRef.h:F64=1/F32=2/U32=3/U64=4/U8=5/I32=6)
- segMask:`voxels`(U8, components=1, elementCount=voxelCount)。
  - 取:`mask.voxels()` 是 `const vector<uint8_t>&`(`XQSegmentationMask::voxels()`)。
  - 重建:`XQSegmentationMask m(dims);` 然后**逐 voxel** `m.setLabelAt(i, v)`(无批量 setVoxels)。
    dims 取 `mask.dimensionX/Y/Z()`;`voxelCount()` = x*y*z。
- surface(`XQSurfaceModel`,几何在 `triangleGeometry()` → `XQTriangleSurfaceGeometryHandle`):
  - `points`(F64, components=3, elementCount=pointCount):把每个 `point(i)`(Point3 x/y/z)按 x,y,z 展平进 `vector<double>`。
  - `tris`(I32, components=3, elementCount=triangleCount):每个 `triangle(i)`(array<int,3>)a,b,c 展平。
  - `faceId`(I32, components=1, elementCount=triangleCount):每三角 `triangleFaceId(i)`。
  - 重建:`auto h=make_shared<XQTriangleSurfaceGeometryHandle>();` 逐点 `h->addPoint({x,y,z})`;
    逐三角 `h->addTriangle(a,b,c,faceId)`(带 faceId 重载);`model.setTriangleGeometry(h)`。
  - **仅当 `model.hasTriangleGeometry()` 才落这三个 blob**;否则该资产 blobs 空(只剩文本元数据)。
- mesh(`XQMesh`):
  - surf 三角(`hasSurfaceTriangles()` → `surfaceTriangles()` handle):role `surfPoints`/`surfTris`/`surfFaceId`,同 surface 编码。
  - vol tet(`hasVolumeTets()` → `volumeTets()` → `XQTetVolumeMeshHandle`):
    `volPoints`(F64×3, pointCount);`tets`(I32×4, tetCount):每 `tet(i)`(array<int,4>)a,b,c,d 展平。
  - 重建:surf handle 同上 setSurfaceTriangles;vol handle 逐点 addPoint、逐 tet `addTet(a,b,c,d)`、setVolumeTets。

#### 小标量文本字段(assets 段内,逐字段;**全部要 round-trip**,field-spec §1 是布局基线)
- segMask 文本:dims(已在 geometry?——**不**,mask 自带 dims,要单独写)、hasGeometry+geometry、
  sourceImageNode(可选)、labels(value+name)。geometry 复用 AssetRecord.geometry?——
  **不复用**:AssetRecord.geometry 是 External 影像几何;segMask 的 mask geometry 要落在 blob role 之外的文本字段里。
  → **决策:segMask 的 dims/maskGeometry/sourceImage/labels 作为该 Derived 资产的"扩展文本行"写在 blobs 段后、endAsset 前**,
    新增文法行(见 S4.3)。
- surface 文本:model.id()、source()(ModelSource 枚举)、sourceContourGroupNode(可选)、preservedArrays(4 bool)、
  faces(ModelFace: faceId/name/kind(FaceKind)/capId(optional<int>)/boundaryLoopIds)。
- mesh 文本:id、sourceModelNode(可选)、preservedArrays、quality(min/max/mean/elementCount)、
  regions(MeshRegion: regionId/name)、boundaryFaces(MeshBoundaryFace: faceId/name/kind/capId/cellIds/localFaces)。
- source/path/simCase/flowResult/aiAnalysis:**全字段文本**,布局严格照 field-spec §1 各段。

### S4.3 主档文法扩展(在现有 assets 段基础上加,不破坏 S3 已绿的)
现有 `asset` 记录(writer `write_asset_record` / reader `parse_asset`)已有:id/category/kind/name、external 行、geometry 行、
blobs 段、endAsset。**S4 在 `blobs <B>` 之后、`endAsset` 之前新增"payload 文本块"**,按 kind 决定块内容:
```
asset <id> category <Cat> kind <Kind> name <...>
  external ...
  geometry ...
  blobs <B>
  blob ... (×B)               # 大数组引用(segMask/surface/mesh)
  payload <kindToken>         # 新增:实体小标量文本块(每类一套行,见 field-spec §1)
    ... (kind-specific lines)
  endPayload                  # 新增
endAsset
```
- `payload`/`endPayload` 块**可选**:无小标量的纯外部影像资产可不写(或写空块);**writer/reader 两端一致**即可。
- 块内 kind-specific 行**逐字照 field-spec §1**(maskDims/maskGeometry/labels;surfaceId/source/preserved/faces;
  meshId/.../regions/meshFaces;source 行;pathId/controlPoints;caseId/solver/fluid/rom/bcs;
  flowSourceCase/times/segments/series;aiKind/metrics/annotations)。
- **段头 peek 铁律**:判 `payload`/`endPayload`/`blob` 等用 `line_starts_with_token`(行首 token),
  **别用 `line_is_single_token`**(踩过坑,见 memory `xq-section-header-peek-needs-first-token`)。
- **node↔asset 绑定**:writer 自动派生时为每个有大数组/实体的 node `createAsset` 并 `setAssetId`,
  绑定走现有 `nodeAssets`/`nodeAsset` 独立记录(S3 已有),node 行结构仍不动。

### S4.4 writer 改造(`XQProjectWriter.cpp`,save 内)— 事务顺序 D7
1. **深拷贝** `XQProject work = project;`(在 work 上派生资产)。
2. 推导 `<stem>.assets` 绝对路径;`BlobStore store(assetsDir);`。
3. 遍历 `work.scene().visit_nodes`:对带实体 payload 的 node(dynamic_pointer_cast 命中 mask/surface/mesh/...):
   - 若该 node 尚无 assetId → `id = work.assetRegistry().createAsset(Derived, kind)`;`node.setAssetId(id)`。
     (注:visit_nodes 给 const ref,改 node 要用 `scene().find(id)` 取非 const,或先收集再改。)
   - 大数组 → `store.put(...)` → 把返回 BufferRef `record->blobs.push_back({role, ref})`。
     **put 失败(返回 false)→ 整个 save 返回 WriteError**(不留半档)。
   - 小标量 → 填进 AssetRecord 的"payload 文本块"所需数据(可直接在序列化时从 node.payload() 现取,免存中间态)。
4. **全部 blob put 完成后**才写主档**临时文件** `<path>.tmp`(序列化 work):版本/段顺序不变,assets 段含新 payload 块。
5. **自检**:临时主档引用的每个 blob 文件都 `fs::exists`(D7 验证引用)→ 否则失败、删 tmp、不替换。
6. **原子替换**:`fs::rename(tmp, path)`(覆盖旧 .xqproj)。
- **副作用不进 assert**(memory `feedback-no-sideeffect-in-assert`):put/rename/exists 返回值先取变量再判。

### S4.5 reader 改造(`XQProjectReader.cpp`,第二段重建)— D8 严格
- 解析阶段:`parse_asset` 在 blobs 段后**新增解析 `payload`/`endPayload` 块**到 `ParsedAsset`(扩字段:按 kind 存小标量)。
- 重建阶段(已有 pass-two registry 重建之后,或合并进去):对每个 ParsedAsset:
  - 用**同一 `<stem>.assets` 根**构造 `BlobStore store(assetsDir)`(reader 从 projectFilePath 推导 dir+stem)。
  - 对每个 blob 引用 `store.get(ref, &vec)`:**任一返回非 Ok(Missing/Truncated/ByteCount/Checksum)→ 整体失败**,
    诊断 code 映射 `ASSET_BLOB_MISSING/TRUNCATED/BYTECOUNT_MISMATCH/CHECKSUM_MISMATCH`(design §6),Status::ParseError,
    **不返回半完整工程(D8)**。返回值先取变量再判,**不进 assert**。
  - 解码 + 小标量 → 重建对应 payload(API 见 S4.2)→ 找到绑定的 node(nodeAsset 反查)→ `node->setPayload(domain, payload)`。
- **影像 External Asset**:kind==Image 的资产**不读 blob**(本就无),元数据照 S3 已有路径恢复(absPath/relPath/dicom/fingerprint/geometry)。

### S4.6 测试 `tests/io/test_payload_roundtrip.cpp`(link xq_io,无需 Qt;CMake 仿 asset_metadata 那 3 行加)
覆盖(AC3/4/5/6/9,逐字段非仅数量):
- 构造含各 payload 的小工程:segMask(4×4×2 几个前景体素+labels+geometry)、surface(4 点 4 三角带 faceId+faces+preserved)、
  mesh(几个 tet+surf 三角+regions+boundaryFaces+quality)、simCase(RCR+波形+rom)、flowResult(小 series 矩阵)、
  aiAnalysis(metrics+annotations)、path(控制点+spacing)、source。建 node 挂 payload。
- save → load → **逐字段断言**:体素值/点坐标/三角连接/tet 四索引/各属性/影像几何/assetId/node↔asset 绑定/资产血缘一致。
- **AC4**:读主档文本,断言**不含**逐点/逐 tet 行(grep 主档无 `^  pt `/`^  tet ` 等大数组行——实际大数组只在 blob);
  断言 `blob ` 引用行在、对应 `<stem>.assets/blobs/...` 文件存在。
- **AC5**:分别 (a)删 blob (b)截断1字节 (c)改主档 byteCount (d)翻1字节破 SHA → load **整体失败**对应 `ASSET_BLOB_*`,不崩。
- **AC6**:登记 image External Asset(absPath/relPath/dicom/fingerprint/geometry)→ round-trip 元数据一致;断言无 image 体素 blob。
- 铁律:`load`/`store.get` 等**先取变量再判,绝不进 assert**(Release /DNDEBUG)。

### S4.7 验收门 S4(全绿才算完)
- **全新目录 build**(memory `cmake-bad-cache-ninja-loop`;Write 的 .bat 转 CRLF,`lf-bat-fake-ninja-not-found`)
  + Release + `set QT_QPA_PLATFORM=offscreen` + **全量 ctest**(原 48 + test_payload_roundtrip)。
- **假绿抽查**(memory `ninja-target-incremental-fakegreen-trap`:用完整 `cmake --build <dir>` 重编,核对 exe 时间戳真变):
  reader 故意①漏读 tet 第4索引 ②漏校验 SHA → test_payload_roundtrip Release **FAIL** → 恢复 → PASS。
- Step 5 反向依赖审计:`rg "#include.*vtk" src/io` **空**;xq_io link 仍只 xq_core + tinyxml2(+header-only picosha2)。
- diff review:大数组绝不进主档;blob 无头;强类型 id 不串;旧 1.1 档仍 load 成功(Info 诊断)。

### S4.8 接手注意(踩过的坑,务必照做)
- `XQProject` 拷贝带 scene+registry,S3 已验证;writer 深拷贝副本派生资产,**不改 const 入参**。
- visit_nodes 是 const 访问;要改 node(setAssetId/setPayload)→ 先收集 id 再 `scene().find(id)` 取非 const。
- 段头 peek 用 `line_starts_with_token`(行首 token),别用 `line_is_single_token`。
- 副作用调用先取变量再判,**不进 assert**;验收必 Release + 全量 ctest。
- 现有 `test_project_versioned_save` 的节点**无 payload**(纯 scene)→ S4 writer 对无 payload 节点必须**零行为变化**
  (不建资产、assets 段仍写 `assets 0`),否则该测试回归。**先确认这条再动手。**
- v1 stash `d5be339` 只读参考、禁 pop;commit 精确列文件,别 `git add -A`(memory `commit-check-gitignore-present`)。

## Step 5 — 反向依赖审计(铁律 1)
- [ ] `rg "#include.*vtk" src/io` **必须空**;`xq_io` link 仍只 xq_core + tinyxml2(+ header-only SHA,不引动态库)。

## Step 6 — 收尾
- [ ] AC1~AC10 逐条对勾(见 prd §5)。
- [ ] 更新父任务 `ROADMAP.md`:M8a 状态。
- [ ] `task.py archive --no-commit` → **精确列文件**(绝不 `git add -A`/`.`,见 memory `commit-check-gitignore-present`)→
      中文 commit:`feat: 存档 — 资产身份 + payload 实体持久化(schema 1.2 + 内容寻址 BlobStore + 向后兼容)`。
- [ ] **删除 v1 stash(仅在确认 v2 已完整覆盖其中仍有价值的逻辑后)**:
      M8a 全步骤 + 全量测试 + diff review 全绿后,逐条核对 v1 stash 中有价值的逻辑(payload 遍历 / 解析错误结构 / 夹具)
      是否已在 v2 落地;确认覆盖后,**按记录的 commit ID `d5be339` 删除**(先 `git rev-parse stash@{0}` 比对,
      不假定它仍是 stash@{0};必要时用 `git stash drop <对应 stash@{N}>`,先核对其 hash == `d5be339`)。**未确认覆盖前不得删。**

## 关键坑(memory 沉淀,务必照做)
- **副作用不进 assert**(`feedback-no-sideeffect-in-assert`):`reader::load`/`BlobStore::get` 等返回值**先取变量再判断**,
  绝不写进 assert(Release /DNDEBUG 删 assert = 假绿 segfault)。验收必须 **Release + 全量 ctest**。
- **假绿抽查别被增量骗**(`ninja-target-incremental-fakegreen-trap`):篡改后用**完整** `cmake --build <dir>` 重编,
  核对 exe 时间戳真变,别用单 target 增量(可能静默跳过链接跑旧 exe 假 Passed)。
- **commit 前核查 .gitignore**(`commit-check-gitignore-present`):精确列文件路径;status 见大数据目录/子仓库变 `??` 立刻停。
- **blob 无头**:元数据全在主档 BufferRef,blob 只裸字节(为 mmap + 防双重真相);别在 blob 写自定义头。
- **强类型 id**:AssetId/NodeId 不可隐式互转,S1 就锁死,否则后面 id 串用难查。
- **向后兼容硬验收**:旧 1.1 档必须读成功(资产空 + Info 诊断),别判 ParseError。
- **加载失败语义**:含资产语义工程的 blob 坏 = 整体失败,**不返回半完整工程**(与旧"buffer 空不崩"相反)。
- **构建坑**:全新 build 用全新目录(坏 cache 假报找不到 Ninja,见 `cmake-bad-cache-ninja-loop`);
  Write 新建 .bat 是 LF 会假报找不到 Ninja(`lf-bat-fake-ninja-not-found`),先转 CRLF;ctest 前 `set QT_QPA_PLATFORM=offscreen`。
