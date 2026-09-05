# Design：领域契约与持久化

## Architecture boundary

本任务只建立 `core` 值类型、payload、scene/asset 分类和 `io/project` 持久化。所有公开类型保持 C++17、零 Qt/VTK/ITK/GDCM 依赖。

```text
XQDataNode(optional scaleSlot, contentRevision)
  ├─ XQImageVolumePayload(metadata only)
  └─ XQVesselProfilePayload(VesselProfileV1)
          ↓
XQProjectWriter / XQProjectReader
          ↓
schema 1.3, while reader remains compatible with 1.2
```

## ScaleSlot ownership

- `XQDataNode` 是 ScaleSlot 的唯一权威拥有者，以 optional 表达“未指定”，没有隐式默认 `Organ`。
- `XQDataNode` clone/copy、scene command、project writer/reader 均保留该值。
- 1.2 node 没有 scale 字段，reader 保持 absent，并产生至多一次信息诊断。
- ScaleSlot 不进入 render type、LOD selector 或相机逻辑。

## Image metadata payload

新增 `XQImageVolumePayload`，内部保存 `XQImageVolume` 的值语义元数据。它不得持有 `XQMemoryImageBufferHandle`、`IVoxelSource` 或外部图像对象；`XQImageVolume` 中现有 counts-only `ImageBufferHandle` 在序列化时不作为像素来源。

像素数据边界保持：

```text
node.payload -> geometry/type/DICOM metadata
node.assetId -> AssetRecord -> ImageSourceResolver
                              ├─ ExternalSource DICOM -> reread by UID
                              └─ managed BufferRef("voxels") -> mapped source (future/legacy)
```

这样 GUI 与 headless consumer 都能由同一 node/asset 重建影像视图，窗口私有缓存只负责短期驻留。

## VesselProfileV1

建议核心形态：

```text
VesselProfileV1
  contractVersion = 1
  coordinateFrame = LPSPatient
  frameOfReferenceUid
  lengthUnit = Millimeter
  areaUnit = SquareMillimeter
  sourcePathNode
  sourceEvidenceNodes[]
  optional externalEvidenceId / externalEvidenceFingerprint
  algorithmId / algorithmVersion / parameterSummary
  samples[]

VesselProfileSample
  stableSampleId
  arcLengthMm
  positionMm
  tangent
  areaMm2
  sourceKind / quality
```

半径只通过 `sqrt(area/pi)` 计算，不持久化为并列权威字段。BC、材料和求解结果不进入该类型。

## Validation

提供纯 core 或 pure-service validator，返回稳定状态码和问题列表，不抛异常穿过 UI：

- unsupported contract version；
- unknown/wrong coordinate frame or unit；
- missing/invalid source ids/fields；MeasuredContour 必须声明 Contour evidence ids，ImportedGold 必须有 external evidence id/fingerprint；pure validator 不查询 Scene，source 实际存在/类型/binding 由 controller 与 project command 在 prepare/commit 两次验证；
- fewer than three samples；
- non-finite position/tangent/arc/area；
- duplicate sample id；
- non-increasing arc length；
- non-unit or zero-length tangent；
- non-positive area。

validator 不修改传入 profile。所有 importer/assembler/solver consumer 在提交 scene 前调用同一 validator。

## Domain and asset integration

- 在 `XQDomainType` 末尾追加 `VesselProfile`，避免改变既有枚举值。
- 在 `AssetKind` 末尾追加 `VesselProfile`。
- `groupForDomain(VesselProfile)` 映射到现有 `Paths` 分组；domain 仍保持独立，不伪装为 Path。
- 新 payload clone 为深值复制。

## Persistence and schema

- writer 写 schema 1.3；reader 接受 1.2 与 1.3，拒绝未知未来版本。
- node 记录 optional scale token 与 content revision；读取 1.2 时 scale absent、revision 使用兼容初值。
- image payload 使用显式 `imageVolume` block 保存 metadata。
- profile metadata/samples 使用现有项目格式可严格校验的 typed encoding；如采用 blob，必须先确认 BlobStore 支持 sample-id 的无损 element type，不能臆造不受支持的 `U64` role。
- reader 校验所有 sample 字段计数与类型一致后才重建 payload。
- 任一 payload/blob 失败导致项目整体加载失败，不返回部分 profile。

## Compatibility and migration

- 现有 1.2 fixture 不重写即可读取。
- 保存已加载的 1.2 项目时升级为 1.3；节点 id、asset id、relations、stale 和现有 payload 不变。
- 不为旧项目猜测 DICOM UID、profile 来源或微观尺度。

## Revision, derivation and multi-source mutation

- `contentRevision` 是 node 的 semantic content 版本；普通 reader/materialize 不改变它。
- `DerivationStamp` 保存算法版本、参数摘要、可选 seed 和输入 node revision/asset fingerprint。
- 新 `ProjectNodeBatchCommand`（最终命名可按现有风格）持有 `XQProject*` 和一组 prepared additions：node、optional caller-supplied AssetRecord、binding、source ids。它预验证全部 ids/sources，随后依次注册 assets、插入 nodes、建立 Scene relations 和 asset relations；任一步失败按逆序撤销全部步骤，undo 同样完整撤销。
- `AssetRegistry` 增加受测试的 `unregisterAsset`/relation removal；unregister 不降低 `next_id_`，从而不复用历史 id。运行时 resident cache 是可重建派生状态，不作为 Project 原子性的替代真源。
- 新 semantic replace command 在 execute 前保存 payload/revision/完整 stale snapshot，替换后递增 revision 并调用现有 transitive stale；undo 恢复三者。
- ContourGroup typed payload 与 AssetKind 在同一 schema gate 修复，否则完整派生链重开会断裂。

## Failure atomicity

reader 在临时 project 中完成解析、blob 校验和 payload 重建，全部成功后才返回。project-level command 在 validator 成功后才构造；asset/node/binding/relations 任一步失败都回滚，失败不得进入 undo stack。
