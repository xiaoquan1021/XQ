# Wire Lazy Geometry Into GUI Rendering

## Goal

Make the production GUI render surface and mesh nodes whose geometry was loaded lazily through `geometryAssetId`. The current reader and services resolver can defer geometry materialization, and `XQSceneRenderer` can consume `IGeometrySource`, but `XQMainWindow` only renders resident handles. A lazily loaded surface or mesh can therefore appear in the scene tree and still render as blank when selected.

## Confirmed Facts

- `XQProjectReader::load(..., XQProjectReadOptions{.lazyGeometry = true})` stamps `XQSurfaceModelPayload` and `XQMeshPayload` with `geometryAssetId` while leaving resident geometry handles empty.
- `resolveLazyGeometrySource(const XQPayload&, GeometryResourceManager&, const AssetRegistry&)` resolves those payloads to `IGeometrySource` through `GeometryResourceManager`.
- `XQSceneRenderer` already exposes `addSurfaceProgressive(const IGeometrySource&)` and `addVolumeMeshProgressive(const IGeometrySource&)`.
- `XQMainWindow::onSceneSelectionChanged()` currently renders only `model.hasTriangleGeometry()`, `mesh.hasVolumeTets()`, and `mesh.hasSurfaceTriangles()`. It never checks `geometryAssetId`.
- `GeometryResourceManager` requires both the loaded project's `AssetRegistry` and the `<project-stem>.assets` root path.
- `XQAppStartup` currently loads native projects with the default eager reader path and does not retain the asset root path for the window.

## Requirements

### R1. Keep eager resident rendering unchanged

If a surface or mesh payload carries resident handles, `XQMainWindow` must keep using the existing `addSurface()` / `addVolumeMesh()` paths.

### R2. Resolve lazy surface geometry on selection

If a selected `SurfaceModel` payload has no resident triangle geometry but has a `geometryAssetId`, the window must resolve it through `GeometrySourceResolver` and render it via `XQSceneRenderer::addSurfaceProgressive`.

### R3. Resolve lazy mesh geometry on selection

If a selected `Mesh` payload has no resident volume or surface handles but has a `geometryAssetId`, the window must resolve it and render via:

- `addVolumeMeshProgressive` when the resolved source has tetrahedra;
- otherwise `addSurfaceProgressive` when the source has triangles.

### R4. Wire real app startup to the lazy path

When a project path is supplied to `xq_app`, startup must load it with `lazyGeometry=true`, record the matching asset root directory, and attach the window to the project's lazy geometry resources. No-argument startup remains an empty project with no asset root.

### R5. Fail closed and fall back correctly

An unresolved lazy asset must not crash and must not fabricate geometry. The existing resident-handle fallback remains the only fallback. If neither resident nor lazy geometry is available, selection leaves the renderer empty.

### R6. Preserve layer boundaries

`XQMainWindow` may depend on services-level lazy geometry resolution, but it must not parse project files or blob files directly. `xq_app_shell` must not gain a direct `xq_io` dependency solely for this UI dispatch; IO remains behind `GeometryResourceManager` and existing service/library edges.

## Acceptance Criteria

- [ ] AC1: A GUI startup/window test fails against the current code because a lazily loaded surface or mesh selection stays blank.
- [ ] AC2: Selecting a lazy surface node switches the central stack from the image label back to the render widget, proving render stats were `ok`.
- [ ] AC3: Selecting a lazy mesh node renders through the volume-source path when tetrahedra exist.
- [ ] AC4: `initializeAppStartup()` for a native project path loads surface/mesh geometry lazily, stores the derived asset root path, and exposes enough state for the window to resolve geometry.
- [ ] AC5: Existing eager `test_main_window`, `test_app_startup`, renderer progressive tests, resolver tests, and full Release `ctest` pass.

## Out of Scope

- Adding menu/file-dialog project open.
- Changing project file format or asset registry schema.
- Implementing sidecar Merkle resolver policy; that is covered by `07-01-fix-geometry-sidecar-resolver`.
- Reducing host peak memory beyond existing progressive upload behavior.
- Adding async/background chunk upload or progress UI.

## Open Questions

None blocking. Repository evidence points to using existing services resolver plus renderer progressive source APIs.
