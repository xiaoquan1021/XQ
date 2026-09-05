# Fix renderer connectivity validation

## Goal

Prevent `XQSceneRenderer` from reading outside the point span or handing invalid
cell connectivity to VTK when a surface or tet source contains vertex indices
outside `[0, pointCount)`.

## Confirmed Evidence

- `XQ/src/visualization/XQSceneRenderer.cpp:247-288`:
  `build_cell_chunk()` reads each global index `g` and then calls
  `allPoints[static_cast<std::size_t>(g)]` when `local < 0`. Negative `g` or
  `g >= allPoints.size()` reaches this expression and can read outside the
  source point span.
- `XQ/src/visualization/XQSceneRenderer.cpp:637` and `:751` call
  `build_cell_chunk()` for progressive surface triangles and progressive tets
  without validating the connectivity first.
- `XQ/src/visualization/XQSceneRenderer.cpp:204-205` and `:302-304` bulk-copy
  one-shot surface/tet connectivity into VTK without validating index bounds.
- `XQ/src/core/XQTriangleSurfaceGeometryHandle.cpp:13-21` and
  `XQ/src/core/XQTetVolumeMeshHandle.cpp:13-16` append connectivity without
  validating indices; `is_valid()` checks exist but renderer does not call them.
- Existing progressive renderer tests cover normal chunking, faceId offsets,
  copy-on-upload, and rendering, but do not inject bad connectivity.

## Requirements

### R1. Renderer must reject out-of-range surface connectivity before upload

For both one-shot and progressive surface paths, any triangle vertex index `< 0`
or `>= pointCount` must make the add call return `RenderStats{ok=false}` without
adding actors.

### R2. Renderer must reject out-of-range tet connectivity before upload

For both one-shot and progressive volume paths, any tet vertex index `< 0` or
`>= pointCount` must make the add call return `RenderStats{ok=false}` without
adding actors.

### R3. Failure must be atomic from the caller's perspective

Connectivity validation must happen before chunk actor creation. On failure,
`actorCount` remains the pre-call count, `chunkCount == 0`,
`completedChunkCount == 0`, and `onChunk` is not called.

### R4. Valid renderer behavior must not regress

Existing LOD/progressive copy-on-upload behavior, faceId segment offset behavior,
source point count reporting, and offscreen rendering must remain unchanged for
valid geometry.

## Acceptance Criteria

- [ ] Add a regression where a progressive surface source contains a triangle
  index outside the point span; `addSurfaceProgressive()` returns `ok=false`,
  adds no actor, and does not call `onChunk`.
- [ ] Add a regression where a progressive tet source contains a tet index
  outside the point span; `addVolumeMeshProgressive()` returns `ok=false`,
  adds no actor, and does not call `onChunk`.
- [ ] Add one-shot surface and volume regressions for invalid concrete handles;
  `addSurface()` / `addVolumeMesh()` return `ok=false` and add no actor.
- [ ] Existing `test_scene_renderer`, `test_scene_renderer_progressive`,
  `test_surface_lod`, and `test_chunk_plan` pass.
- [ ] Final child check: Release full `ctest` passes.

## Out of Scope

- Do not change `IGeometrySource`, `RenderStats`, `ChunkUploadSpec`, or public
  renderer signatures.
- Do not attempt to repair, clamp, or skip invalid cells; the renderer must
  reject the whole add call to avoid silently changing geometry.
- Do not add reader-side index scanning in this child task; lazy/project reader
  schema work is handled by separate audit tasks.
