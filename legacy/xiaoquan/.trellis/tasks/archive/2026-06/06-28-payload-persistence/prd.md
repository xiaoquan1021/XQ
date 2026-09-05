# PRD — 资产身份 + payload 实体持久化(v2 重型基线)

> 任务 id:`06-28-payload-persistence` | 优先级 P1 | package XQ | parent `06-29-xq-rebuild`
> v2 重写背景:v1 按"轻量化单机原型"(几何写主档文本、影像 buffer 落盘)规划,与真实目标
> **重型前处理(高分影像 + 千万级网格)** 对不上,已暂停重写。旧版见 `prd.old-lightweight.md`。

## 1. 目标与定位

让 XQ 存档从"只存 scene 结构"升级到"**存稳定的数据资产身份 + payload 实体数据**",
做到**实体级 round-trip**,并为重型前处理建立**可演进的资产模型骨架**。

**产品定位**:重型前处理(高分医学影像 + 千万级血管网格)+ 1D 血流求解;3D CFD 通过 `ICfdSolver`
接口 + adapter 后续接入(本任务不做)。

**本任务的边界(关键)**:只让 **"身份、格式、数据落盘"正确**;运行时"内存如何加载与释放"是**下一阶段**。
本任务**不改变** services / solver / renderer / UI 的运行时数据访问方式。

## 2. 资产模型(本任务确立的领域抽象)

四类资产,**"是否一等数据" ≠ "是否进 .xqproj"**:

| 资产类别 | 含义 | 是否进工程真相 | 本任务处理 |
|---|---|---|---|
| **External Source Asset** | 原始医学影像(DICOM/NIfTI),物理上默认外部引用 | 是(登记,不复制体素) | 登记元数据,**不复制体素、不解码** |
| **Managed Asset** | 用户显式"导入工程"的资产(Managed Original Source / Managed Canonical Volume) | 是 | **仅格式预留**,不实现导入流程 |
| **Derived Project Asset** | XQ 产生的派生数据:segMask、Surface、Tet Mesh、标注、配准等 | 是,**必须随档(sidecar)** | **完整落盘 + round-trip** |
| **Disposable Cache** | 解码体素 / GPU 资源 / 缩略图 / 重采样 | 否,可丢弃 | **不进 AssetRegistry、不写 .xqproj** |

身份与关系分两层:
- **`assetId`**:稳定的数据资产身份,独立于 `NodeId`。资产重定位 / external→managed 转换时
  **`assetId`、`NodeId`、派生关系均不变**,只改 AssetRegistry 中的存储描述。
- **`NodeId`**:scene 图中的对象实例标识(现状不变)。
- **数据血缘落在 Asset 层**(AssetRegistry 存资产级派生血缘:影像→掩膜→表面→体网格→流场);
  **scene 继续管节点拓扑与节点级关系**(`link_derived` / stale 现状**不重构**)。
- `SceneNode` 增加**可选** `assetId` 引用;同一 asset 可被多个 Node 引用。

## 3. 范围

### In scope

1. **core 新增最小资产层**:`AssetId`、`AssetRecord`、`AssetRegistry`、`AssetRelation`、`BufferRef`。
   `AssetRegistry` 只负责:稳定身份、资产类型/存储模式、存储描述、外部源定位、sidecar 引用、
   必要元数据、**资产级派生血缘**。**不负责**加载/缓存/mmap/淘汰/内存预算。
2. **`SceneNode`(`XQDataNode`)增加可选 `assetId` 引用**;`NodeId` 语义不变;节点级关系不重构。
3. **存档升级为版本化资产格式**:新增独立 **`assets` 段** + **`scene` 段**。
   这是**第一版稳定的版本化资产格式**(不宣称永不升级)。旧 `1.1` 档**必须仍可读**;新保存统一写新版本。
4. **大型数组 → 规范化二进制 sidecar**:segMask 体素、Surface points/triangles/attributes、
   Tet Mesh points/tets/boundary faces 等写入 sidecar;主档只存 `BufferRef`。
   磁盘格式**显式记录** `formatVersion / endianness / elementType / components / elementCount /
   byteCount / SHA-256`,**不得直接序列化 C++ `Point3` / `std::array` 等 ABI 内存布局**。
5. **原始影像 = External Source Asset**:登记绝对路径 + 相对路径 + DICOM UID + 内容指纹 + 必要几何元数据;
   **不复制体素、不实现 DICOM/NIfTI 解码缓存 / mmap / 导入 / 便携打包**。
6. **运行时 payload 与全量 vector 保持不变**:save 时 writer 把现有 payload 数组写入 sidecar 并在
   `AssetRecord` 记 `BufferRef`;load 时 reader 读取 + 校验 sidecar,再恢复现有 payload vector。
   **本任务只改身份与持久化方式,不改运行时访问方式**。
7. **测试**:实体级 round-trip + 向后兼容 + sidecar 损坏诊断 + 零-VTK 审计。

### Out of scope(明确排除,留后续阶段)

`IVoxelSource` / `IGeometrySource`、Memory/Mapped/Bricked VoxelSource、`GeometryResourceManager`、
按需驻留、内存预算、`SceneNode` 去 vector 化、`MutableGeometryBuilder`、不可变资产版本化、
`MappedGeometrySource`、LOD、规范化影像解码缓存、Managed Source 实际导入、便携工程打包、
`.svproj` 回写、跨大版本迁移工具、压缩/增量保存。

## 4. 铁律约束(违反即 BLOCKER)

1. **io 零 VTK**:`xq_io` 只 link `xq_core` + tinyxml2。sidecar 是 XQ 自写裸字节;SHA-256 自实现或
   header-only vendoring(不引 VTK/重依赖)。审计:`rg "#include.*vtk" src/io` 必须空。
2. **大型数组不进主档文本**:Surface/Mesh/segMask 的点/单元/体素**只进 sidecar**;主档只有 `BufferRef`。
   审计:round-trip 后主档 `.xqproj` 文本中不得出现逐点/逐 tet 行。
3. **向后兼容**:旧 `1.1` 档(无 assets 段)必须仍 load 成功,不得 ParseError。
4. **规范化磁盘格式**:sidecar 显式声明 endian/类型/布局,**不依赖 C++ ABI**;little-endian 基准。
5. **结构化错误**:sidecar 缺失 / 截断 / `byteCount` 不符 / SHA-256 不符 → 返回**结构化错误/诊断**
   (带 code),不静默、不崩。
6. **provenance/stale/relation 不回归**:现有 scene 结构序列化逻辑不破坏。

## 5. 验收标准(逐条可证伪)

- [x] **AC1 资产层落地**:core 有 `AssetId`/`AssetRecord`/`AssetRegistry`/`AssetRelation`/`BufferRef`;
      `AssetRegistry` 不含任何加载/缓存/内存预算 API。
- [x] **AC2 node↔asset 绑定**:`XQDataNode` 可选引用 `assetId`;save→load 后绑定一致。
- [x] **AC3 实体级 round-trip**:含各类 payload(segMask/surface/mesh/simCase/flowResult/aiAnalysis/path/source)
      的工程 save→load 后,**payload 数据、拓扑、几何元数据、`assetId`、node↔asset 绑定、资产血缘逐字段一致**。
- [x] **AC4 大数组走 sidecar**:Surface/Mesh/segMask 大数组在 sidecar;主档文本无逐点/逐 tet 行(审计断言)。
- [x] **AC5 sidecar 自描述 + 校验**:sidecar 头含 formatVersion/endian/elementType/components/elementCount/
      byteCount/SHA-256;损坏(缺失/截断/byteCount 错/SHA 错)→ 结构化错误,不崩。
- [x] **AC6 影像 External Asset**:影像登记绝对+相对路径/DICOM UID/内容指纹/几何元数据;体素**未**被复制;
      round-trip 后这些元数据一致。
- [x] **AC7 向后兼容**:旧 `1.1` 档(无 assets 段)load 成功,资产为空 + 发诊断,不 ParseError。
- [x] **AC8 io 零 VTK**:`rg "#include.*vtk" src/io` 空;`xq_io` link 列表无 VTK。
- [x] **AC9 全量绿 + 假绿抽查**:全新构建 Release + 全量 ctest 全绿(原有 + 新增持久化测试);
      假绿抽查:篡改 reader 漏读某字段(如 tet 第 4 索引)/ 篡改 sidecar 字节 → round-trip Release FAIL → 恢复 → PASS。
- [x] **AC10 主线不回归**:`test_project_roundtrip` / `test_new_domain_roundtrip` / `test_project_versioned_save`
      全绿(后者断言需按新格式同步修改,见 §7)。

## 6. 已知现状(实测基线)

- **分层(干净,保留)**:`xq_core`(零依赖)← `xq_io`(+tinyxml2,**零 VTK 已坐实** CMakeLists:198)、
  `xq_services`(仅 core)、`xq_controllers`(core+services,headless)。重依赖隔离:VTK 在
  `xq_adapter_vtk`/`xq_visualization`/`xq_render_widget`/`xq_ui`(Qt),TetGen/MMG/ONNX 在可选 adapters(默认 OFF)。
- **writer**(`XQProjectWriter.cpp`,149 行):纯文本逐行,schema `1.1`,**完全无 payload 实体**;
  projectId/record 当前写死占位(`synthetic-l0-project` / record 1)。
- **reader**(`XQProjectReader.cpp`,618 行):对称解析;`endScene` 后 peek `provenance`(:465)——
  **正是插 `assets` 段的位置**;节点重建后可经 `XQDataNode::setPayload` 挂 payload。
- **payload 类型**(11 种 `make_shared<...Payload>`,全库无 image payload):source/path/segMask/
  surface/mesh/simCase/flowResult/aiAnalysis;字段清单见 `field-spec.md`(实读 core 头的事实基线)。
- **节点挂 payload**:`XQDataNode::setPayload(XQDomainType, shared_ptr<XQPayload>)`(XQDataNode.h:34);
  取 `payload()`(:33)。reader 重建路径无硬阻塞(已核 Tet/segMask handle 的 add/set API 齐全)。
- **影像**:`XQImageVolume`(core,完整影像类)+ `XQMemoryImageBufferHandle`(整块 `vector<uint8_t>` 常驻);
  **从不被任何 payload 包装、从不进 scene/存档**——image 节点现挂 `XQSourcePayload`(只 sourcePath)。
- **几何**:`Point3`=3×double(GeometryTypes.h:8);Tet=`array<int,4>`、Tri=`array<int,3>`;
  整块 `vector` 常驻在 payload handle 上。
- **stash@{0}**:v1 半成品(几何写文本 + segMask sidecar 框架,checksum 用 **FNV**)。可**参考**
  reader 各 payload 解析块,但大几何要改 sidecar、checksum 改 SHA-256、需套 AssetRegistry——是返工不是直接 pop。

## 7. 已决策的卡点(实现必须照做)

- **旧档诊断行为**:reader 见旧 `1.1` 档(无 assets 段)→ load 成功 + 发一条 **Info 诊断**
  `PROJECT_NO_ASSET_DATA`,资产为空。只有新版本档**缺 assets 段**才视为异常。
- **`test_project_versioned_save` 必改**:
  - `test_current_v11_roundtrip_preserves_node_ids` 第 299 行断言 writer 输出 `schemaVersion 1.1` →
    改为断言新版本号(随 §8 design 定);
  - 第 303 行断言 `\nprovenance\n` 存在:新格式仍保留 provenance 段则不动,否则同步;
  - 第 315 行断言 `result.diagnostics.empty()`:新档自身 round-trip 不应发"无资产"诊断(因为新档**有** assets 段),
    保持 empty;但需确认新 writer 对"空工程"是否写空 assets 段(应写,空段不触发诊断)。
- **schema 版本号 / minimumReaderVersion(已与 design D2 对齐 / 已敲定)**:schema 升 `1.1`→`1.2`,
  **`minimumReaderVersion` 也严格升 `1.2`**——assets 段视为**必读**,新档被旧 reader 拒(`UnsupportedVersion`),
  **不走"旧 reader 跳过 assets 段仍能读"的软兼容**(避免旧 reader 静默丢实体、读到半截工程)。
  reader 须**真正校验** `minimumReaderVersion`(现状未校验,本任务补上)。
  注:这是兼容方向的**单一真相**——旧 `1.1` 档**向后可读**(见铁律 §4.3),但新 `1.2` 档**不向前兼容**旧 reader。

## 8. 实现细节(已在 design.md 定的回填此处,剩余待 design/实现细化)

**已在 design.md 敲定(不再开放)**:
- **SHA-256 来源**:header-only vendoring(如 picosha2.h)进 `third_party/`,由 xq_io 封装内部流式哈希接口(design D4)。
- **存档版本号 / 兼容方向**:schema `1.1`→`1.2`,`minimumReaderVersion` 严格升 `1.2`(design D2,见本 prd §7)。
- **sidecar 目录与命名**:专用资产目录 `<stem>.assets/`,内容寻址 blob `blobs/<前2位>/<sha256>.bin`(design D3)。
- **blob 磁盘布局**:**blob 无自定义头**,只存规范化 little-endian 原始字节;`elementType/components/elementCount/
  byteCount/endianness/formatVersion/SHA-256` 全部由主档 `BufferRef` 记录(design D6,避免双重真相)。
- **保存事务顺序 / 加载失败语义**:原子发布 blob→临时主档→验证引用→原子替换(D7);含资产语义工程 blob 缺失/损坏 → 整体加载失败 + 结构化错误,不返回半完整工程(D8)。

**仍需 design/实现细化**:
- `assets`/`scene` 段的精确逐行文法(段头/字段顺序/转义)。
- `AssetRecord` 的精确字段、资产类型/存储模式枚举、资产血缘的存档文法。
- 各 payload 哪些数组进 sidecar、哪些小标量留主档文本(沿用 field-spec.md 的取舍 + 大数组下沉 sidecar)。
