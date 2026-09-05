# Design: Geometry sidecar resolver strategy

## Problem

`XQProjectWriter` writes `.merkle` sidecars for geometry blobs, but `resolveLazyGeometrySource()` still constructs a `GeometrySourceSpec` with `segmented=false`. This makes lazy GUI/project paths always use `FullVerify` and leaves the `SegmentedMerkle` geometry path unused outside direct manager tests.

## Approach

1. Keep resolver responsible for deriving the requested geometry aspect and element counts from `AssetRegistry`.
2. Try `manager.acquireGeometrySource(id, spec)` first with `spec.segmented = true`.
3. If that returns an invalid handle, retry with `spec.segmented = false`.
4. Do not stat sidecar files in the resolver. The manager and `MappedGeometrySource` already own asset-root path resolution, sidecar parsing, and integrity validation.
5. Update comments in resolver/manager/mapped source docs to say writer emits geometry sidecars opportunistically, while `FullVerify` remains the compatibility fallback.

## Edge Cases

- No geometry asset id -> invalid handle, no manager call.
- Unknown asset id -> invalid handle.
- No complete requested geometry group -> invalid handle.
- Sidecar missing/corrupt but blob valid -> segmented acquire invalid, FullVerify fallback valid.
- Blob tampered and `BufferRef.sha256` stale -> segmented may fail on segment verification; FullVerify fallback must also fail on anchor mismatch.
- Repeated segmented and full acquisitions of the same asset/aspect must not share one cache entry.

## Test Strategy

- Extend `test_geometry_source_resolver` with a writer-produced lazy project and assert the resolved `MappedGeometrySource` reports `segmentsChecked > 0` after acquisition.
- Delete all `.merkle` files for a writer-produced project, then assert lazy resolution still succeeds and the source reports `segmentsChecked == 0` with `bytesHashed > 0` after acquisition.
- Keep equivalence tests comparing eager and lazy geometry.
