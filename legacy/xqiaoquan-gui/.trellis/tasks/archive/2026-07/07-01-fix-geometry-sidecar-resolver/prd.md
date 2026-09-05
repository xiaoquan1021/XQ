# Fix geometry sidecar resolver strategy

## Goal

Project writer now emits `.merkle` sidecars next to geometry blobs, but lazy geometry resolution still forces `GeometrySourceSpec::segmented = false` and several comments still claim geometry sidecars are not emitted. Lazy project rendering therefore never exercises the segmented geometry integrity path.

## Requirements

- `resolveLazyGeometrySource(...)` must prefer `SegmentedMerkle` for lazy geometry assets when geometry sidecars are available.
- If segmented acquire fails because sidecars are absent, corrupt, or unreadable, resolver must fall back to `FullVerify` instead of making old or partially written projects unreadable.
- Fallback must still use `BufferRef.sha256` anchors so tampered blobs are rejected by `FullVerify`.
- Surface-only, tet-only, and combined geometry aspect behavior must remain unchanged.
- Full/segmented cache keys must remain distinct.
- Update stale comments/spec text that says the writer does not emit geometry sidecars.

## Acceptance Criteria

- A writer-produced lazy project resolves through `SegmentedMerkle`; tests prove the mapped source checks merkle segments.
- Removing the geometry `.merkle` files from the same project still allows lazy resolution via `FullVerify`.
- Existing eager/lazy equivalence tests keep passing.
- `test_geometry_source_resolver`, `test_geometry_resource_manager`, `test_payload_roundtrip`, and full Release `ctest` pass.

## Non-Goals

- Do not make writer sidecar failures fatal in this task.
- Do not change `.xqproj` schema.
- Do not change `MappedGeometrySource` public API unless tests prove resolver cannot be verified otherwise.
