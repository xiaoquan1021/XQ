# Implementation Plan: fix-blob-schema-validation

## Steps

1. Harden `BlobStore`
   - Add `InvalidMetadata` status.
   - Add checked logical-byte-count helper.
   - Reject absolute/empty/parent-traversal `relPath`.
   - Enforce version/endian/type/components/byteCount and typed overload compatibility.
   - Add `test_blob_store` regressions for metadata mismatch, overflow, typed overload mismatch, and path traversal.

2. Harden `XQProjectReader`
   - Map `InvalidMetadata` to `ASSET_BLOB_INVALID_METADATA`.
   - Validate each asset's blob roles after parsing.
   - Reject duplicate and unknown roles.
   - Enforce role schema table and parallel faceId counts.
   - Check segMask `voxels.elementCount == dims[0] * dims[1] * dims[2]` with overflow protection.
   - Add `test_payload_roundtrip` or dedicated reader regressions for wrong type/components/count and lazy metadata failure.

3. Harden lazy geometry resource path
   - Validate registry `BufferRef` roles inside `GeometryResourceManager::acquireGeometrySource()`.
   - Reject count/byteCount mismatch and budget overflow before constructing `MappedGeometrySource`.
   - Add `test_geometry_resource_manager` regression for bad registry returning empty handle.

4. Validation
   - Build targets: `test_blob_store`, `test_payload_roundtrip`, `test_project_lazy_geometry`, `test_geometry_resource_manager`, `test_geometry_source_resolver`, `test_mapped_geometry_source`.
   - Run targeted ctest:
     `ctest --test-dir XQ/build_m9be -C Release --output-on-failure -R "blob_store|payload_roundtrip|project_lazy_geometry|geometry_resource_manager|geometry_source_resolver|mapped_geometry_source"`.
   - Run full Release `ctest`.

## Rollback Points

- If strict unknown-role rejection breaks an existing committed fixture, inspect whether the fixture encodes a real supported role. Do not silently whitelist unknown roles without a consumer contract.
- If adding `InvalidMetadata` fans out too broadly, keep the enum addition but map existing callers through `BlobErrorCode::of()` so public reader behavior remains structured.

## Files

- `XQ/src/io/blob/BlobStore.h`
- `XQ/src/io/blob/BlobStore.cpp`
- `XQ/src/io/project/XQProjectReader.cpp`
- `XQ/src/services/resource/GeometryResourceManager.cpp`
- `XQ/tests/io/test_blob_store.cpp`
- `XQ/tests/io/test_payload_roundtrip.cpp`
- `XQ/tests/services/resource/test_geometry_resource_manager.cpp`
