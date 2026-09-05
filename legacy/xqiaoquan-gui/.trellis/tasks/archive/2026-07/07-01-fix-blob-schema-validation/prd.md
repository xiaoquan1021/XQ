# 修复项目 blob 元数据校验缺失

## Goal

项目加载、blob 读取和 lazy geometry 解析必须拒绝与角色不匹配或越界的 `BufferRef` 元数据，防止损坏项目被静默恢复为错误几何/体素，或通过 `relPath` 逃逸资产根目录。

## Confirmed Evidence

- `XQ/src/io/project/XQProjectReader.cpp:588` 的 `parse_blob_line()` 只解析 `role/relPath/byteCount/sha256/formatVersion/endianness/elementType/components/elementCount` 的数值范围，未按 role 校验 `elementType/components/count/byteCount`。
- `XQ/src/io/project/XQProjectReader.cpp:966` 的 `rebuild_triangle_geometry()` 直接 `store.get()` 三个 blob，再用 `points.size()/3`、`tris.size()/3` 截断重建，`faceIds` 不足时默认 0。
- `XQ/src/io/project/XQProjectReader.cpp:1266` 的 `rebuild_seg_mask()` 未验证 `voxels.elementCount` 等于 mask dims 的 voxel 数。
- `XQ/src/io/project/XQProjectReader.cpp:1385` 和 `XQ/src/io/project/XQProjectReader.cpp:1527` 的 lazy path 只检查 blob 是否存在，未读取 blob 文件，因此更依赖 main-document metadata 先被严格验证。
- `XQ/src/io/blob/BlobStore.cpp:225` 直接把 `rootDir_ / ref.relPath` 拼为文件路径，未拒绝绝对路径或 `..` 路径。
- `XQ/src/io/blob/BlobStore.cpp:253`、`XQ/src/io/blob/BlobStore.cpp:275`、`XQ/src/io/blob/BlobStore.cpp:300`、`XQ/src/io/blob/BlobStore.cpp:322` 使用乘法计算逻辑长度，未做溢出检查，也未强制 `byteCount == elementCount * components * byteWidth(elementType)`。
- `XQ/src/services/resource/GeometryResourceManager.cpp:183` 用 registry `BufferRef` 构造 `MappedGeometrySource` 输入，但未再次验证 role schema；绕过 reader 构造 registry 时仍可能进入 lazy source。

## Requirements

### R1. BlobStore 必须拒绝无效 BufferRef 元数据

- `relPath` 必须是相对路径，且不得包含 `..`，不得为空，不得为绝对路径。
- `formatVersion` 必须为 1，`endianness` 必须为 0。
- `elementType` 必须是已知枚举；`components` 必须大于 0。
- `byteCount` 必须精确等于 `elementCount * components * byteWidth(elementType)`，乘法溢出必须失败。
- `get(double*)` 只能读取 F64/F32，`get(int*)` 只能读取 I32/U32/U64，`get(uint8_t*)` 只能读取 U8。

### R2. ProjectReader 必须按 blob role 校验 schema

- `points`、`surfPoints`、`volPoints`: `F64 x 3`。
- `tris`、`surfTris`: `I32 x 3`。
- `faceId`、`surfFaceId`: `I32 x 1`，且 elementCount 必须分别等于 `tris`/`surfTris` 的 elementCount。
- `tets`: `I32 x 4`。
- `voxels`: `U8 x 1`，并在 segMask rebuild 时验证 elementCount 等于 mask dims 的 voxel 数。
- 同一 asset 内重复 role 必须失败，避免 `find_blob()` 静默选中第一项。
- 任何 unknown blob role 在当前 schema 版本下必须失败，避免未来/损坏字段被当成有效项目保留。

### R3. Eager 和 lazy 加载必须共享同一元数据校验

- eager path 读取 blob 前必须已经通过 role schema 校验。
- lazy path 即使不读取 blob 文件，也必须拒绝错误 role metadata 和错误 count。
- reader 必须保持 “blob 错误导致整项目加载失败，不返回部分项目” 的现有契约。

### R4. GeometryResourceManager 必须防御坏 registry

`GeometryResourceManager::acquireGeometrySource()` 在把 registry `BufferRef` 转成 `MappedGeometrySource::Inputs` 前，必须验证 expected role、type、components、elementCount、byteCount 和预算求和溢出。无效 registry 返回空 handle，不得 map。

## Acceptance Criteria

- [ ] 错误 `elementType/components` 的 `points/tris/faceId/surfPoints/surfTris/surfFaceId/volPoints/tets/voxels` blob 会导致项目加载失败。
- [ ] `faceId` 与 `tris` 数量不一致、`surfFaceId` 与 `surfTris` 数量不一致会导致项目加载失败。
- [ ] `voxels.elementCount` 与 mask dims 乘积不一致会导致项目加载失败。
- [ ] `byteCount` 与逻辑长度不一致、逻辑长度乘法溢出、未知 elementType/formatVersion/endianness 会被拒绝。
- [ ] `relPath` 为绝对路径或包含 `..` 时，`BlobStore::get()` 返回失败状态，且不读取资产根外文件。
- [ ] lazyGeometry=true 时，损坏 metadata 即使 blob 文件存在/不读取，也不能成功 stamp `geometryAssetId`。
- [ ] `GeometryResourceManager::acquireGeometrySource()` 对坏 registry 返回空 handle。
- [ ] 现有合法项目 round-trip、lazy geometry、blob store 测试仍通过。

## Out of Scope

- 不改变 project schema version。
- 不实现几何 connectivity 索引合法性校验；该问题属于 `07-01-fix-renderer-connectivity-validation`。
- 不改变 writer 的 blob 输出格式。
- 不修复 geometry sidecar resolver 策略；该问题属于 `07-01-fix-geometry-sidecar-resolver`。

## Verification

- Targeted: `test_blob_store`、`test_payload_roundtrip`、`test_project_lazy_geometry`、`test_geometry_resource_manager`、`test_geometry_source_resolver`、`test_mapped_geometry_source`。
- Final child check: Release 全量 `ctest`。
