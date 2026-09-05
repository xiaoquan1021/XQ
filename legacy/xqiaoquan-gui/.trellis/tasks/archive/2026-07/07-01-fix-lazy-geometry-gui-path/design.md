# Design: Lazy Geometry GUI Rendering Path

## Scope

This task connects three existing pieces:

- `XQAppStartup` loads native project geometry lazily and retains the asset root path.
- `XQMainWindow` owns or attaches a `GeometryResourceManager` for the displayed project.
- Selection dispatch resolves `geometryAssetId` and calls `XQSceneRenderer` source-based progressive render APIs.

It does not move blob parsing into UI and does not change renderer internals.

## Data Flow

### Native project startup

`runXQApp(argc, argv)` -> parse path -> `XQProjectReader::load(path, &result, {.lazyGeometry = true})` -> `XQAppStartupState.project = result.project` -> `XQAppStartupState.assetRootDir = <dir>/<stem>.assets` -> window attaches workflow and geometry resources.

### Surface selection

Scene tree selection -> `XQSurfaceModelPayload` -> if resident handle exists, call `addSurface()` -> otherwise resolve `geometryAssetId` with `GeometryResourceManager` -> call `addSurfaceProgressive(source)`.

### Mesh selection

Scene tree selection -> `XQMeshPayload` -> if resident volume handle exists, call `addVolumeMesh()` -> else if resident surface handle exists, call `addSurface()` -> otherwise resolve lazy source -> inspect `source.meta()` -> call `addVolumeMeshProgressive(source)` when `tetCount > 0`, else `addSurfaceProgressive(source)` when `triCount > 0`.

## Ownership

`XQMainWindow` should expose a small attachment method such as:

```cpp
void attachGeometryResources(const AssetRegistry* registry,
                             const std::string& assetRootDir);
```

The window can construct and own a `GeometryResourceManager` because the manager is a services-layer object and the app shell already depends on services through controllers. The caller owns the project and registry; the window only borrows the registry pointer, matching the existing `attachWorkflow` lifetime model.

`XQAppStartupState` should retain the asset root directory for loaded projects:

```cpp
struct XQAppStartupState {
    XQProject project;
    XQCommandStack commandStack;
    std::string assetRootDir;
};
```

No-argument startup leaves `assetRootDir` empty and does not attach geometry resources.

## Error Handling

- Missing registry, empty asset root, absent asset id, unknown asset, or invalid blob metadata resolves to an invalid handle.
- Invalid lazy handles are ignored; no window crash, no fake geometry, no fallback to unrelated assets.
- The renderer's existing connectivity and source validation remains the final upload gate.

## Compatibility

- `XQMainWindow::setScene()` and `attachWorkflow()` remain source-compatible.
- Eager project loads and service-produced resident handles keep the existing render path.
- `xq_app_shell` remains free of direct native project reader calls.
