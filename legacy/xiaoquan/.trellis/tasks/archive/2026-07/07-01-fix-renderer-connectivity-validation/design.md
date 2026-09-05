# Design: renderer connectivity validation gate

## Data Flow

`XQTriangleSurfaceGeometryHandle` / `XQTetVolumeMeshHandle` or an
`IGeometrySource` exposes points plus cell connectivity. `XQSceneRenderer`
copies that data into VTK arrays:

- one-shot surface: `build_surface()`
- one-shot volume: `build_volume()`
- progressive surface: `addSurfaceProgressive()` -> `build_cell_chunk()`
- progressive volume: `addVolumeMeshProgressive()` -> `build_cell_chunk()`

The renderer is the last boundary before VTK and before `build_cell_chunk()` may
index into the point span, so it must not trust connectivity from either
resident handles or mapped/lazy sources.

## Validation Model

Add a small anonymous-namespace helper in `XQSceneRenderer.cpp`:

```cpp
bool connectivity_in_bounds(const int* flatConn,
                            std::size_t cellCount,
                            int vertsPerCell,
                            std::size_t pointCount)
```

The helper returns false if:

- `flatConn == nullptr` while `cellCount > 0`;
- `vertsPerCell <= 0`;
- any index is negative;
- any index is `>= pointCount`.

It performs a linear scan over `cellCount * vertsPerCell`, matching the copy
cost already paid by upload. It does not enforce degeneracy or manifold rules;
those belong to modeling/meshing validation.

## Failure Semantics

- One-shot invalid surface/volume returns `ok=false` and does not add an actor.
- Progressive invalid surface/volume returns `ok=false`, reports no chunks
  completed, and does not call `onChunk`.
- No partial chunk creation happens before validation completes.
- Valid paths keep their existing `RenderStats` semantics.

## Compatibility

No public header changes. VTK remains private to `visualization`.

## Risks

- Scanning connectivity adds O(number of cell vertices) CPU work before upload.
  This is acceptable because upload already copies the same connectivity and the
  alternative is memory-unsafe access.
- Rejecting the whole add instead of skipping bad cells is intentionally strict:
  it exposes corrupt upstream data instead of hiding it behind a partial render.
