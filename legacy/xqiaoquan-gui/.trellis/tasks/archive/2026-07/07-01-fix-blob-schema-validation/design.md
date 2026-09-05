# Design: Blob metadata schema validation

## Data Flow

Project file `blob` line -> `ParsedBlob.ref` -> `AssetRegistry` -> eager payload rebuild or lazy `geometryAssetId` -> `GeometrySourceResolver`/`GeometryResourceManager` -> `MappedGeometrySource`.

The main document is the only place that describes headerless blob shape. Therefore validation must happen before either eager decode or lazy stamp.

## Validation Layers

### BlobStore structural guard

`BlobStore::readVerified()` becomes the common low-level guard for:

- path confinement under `rootDir_`;
- known metadata version and endian;
- known element type and non-zero components;
- checked logical byte count;
- typed overload compatibility.

Add `BlobStore::Status::InvalidMetadata` and map it to `ASSET_BLOB_INVALID_METADATA` in the reader. This keeps caller behavior explicit without pretending path traversal or overflow is a missing file.

### ProjectReader role schema guard

Add reader-local helpers:

- `expected_blob_schema(role) -> {elementType, components}`
- `checked_ref_byte_count(ref, expectedBytes*)`
- `validate_asset_blob_schemas(asset)`
- `validate_parallel_counts(asset)`

Run this after parsing each asset and before appending it to `parsed_assets`. This makes eager and lazy paths share the same metadata validation. `rebuild_seg_mask()` still needs a payload-aware check because voxel count comes from `maskDims`.

### GeometryResourceManager registry guard

Add manager-local expected-role validation before filling `MappedGeometrySource::Inputs`.

The manager cannot assume registry entries came from `XQProjectReader`. If a test or future caller constructs `AssetRegistry` directly, invalid type/components/count/byteCount must return an empty source handle.

## Role Contract

| Role | Element type | Components | Count contract |
| --- | --- | --- | --- |
| `points` | `F64` | 3 | surface point count |
| `tris` | `I32` | 3 | surface triangle count |
| `faceId` | `I32` | 1 | equals `tris.elementCount` |
| `surfPoints` | `F64` | 3 | mesh surface point count |
| `surfTris` | `I32` | 3 | mesh surface triangle count |
| `surfFaceId` | `I32` | 1 | equals `surfTris.elementCount` |
| `volPoints` | `F64` | 3 | tet point count |
| `tets` | `I32` | 4 | tet count |
| `voxels` | `U8` | 1 | equals segmentation mask voxel count |

## Compatibility

- Legal projects written by current `XQProjectWriter` must continue loading.
- Older projects without an assets section keep existing behavior.
- Unknown blob roles are rejected for schema 1.2 because the reader has no safe consumer contract for them.

## Failure Mapping

- Invalid metadata in `BlobStore` -> `BlobStore::Status::InvalidMetadata` -> `ASSET_BLOB_INVALID_METADATA`.
- Missing role required by a payload -> existing `ASSET_BLOB_MISSING`.
- File size mismatch -> existing `ASSET_BLOB_BYTECOUNT_MISMATCH`.
- Short file/short logical decode -> existing `ASSET_BLOB_TRUNCATED`.
- Hash mismatch -> existing `ASSET_BLOB_CHECKSUM_MISMATCH`.

## Risks

- Strict unknown-role rejection can break hand-edited projects that add experimental blob roles. This is acceptable for schema 1.2 because the reader cannot preserve unknown binary contract safely.
- `byteCount == logical length` may turn previously accepted malformed metadata into load failures. That is the intended safety change.
