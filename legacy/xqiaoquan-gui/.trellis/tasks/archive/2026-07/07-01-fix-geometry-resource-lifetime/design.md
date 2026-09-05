# Design: GeometryResourceManager handle lifetime

## Data Flow

`AssetRegistry` -> `GeometryResourceManager::acquire*Source()` -> resident block cache -> returned handle -> consumer calls `source().acquire_*()` -> source returns a lease that pins the underlying mmap/control block.

The existing cache correctly protects resident blocks from eviction while the manager is alive, but the returned handle does not own the source object. Manager teardown bypasses the eviction gate because `blocks_` is destroyed directly.

## Ownership Model

Use two independent ownership concepts:

- Source lifetime ownership: `std::shared_ptr<IVoxelSource>` / `std::shared_ptr<IGeometrySource>`.
- Cache eviction gate: existing per-block `std::shared_ptr<void> pin`.

`ResidentBlock` stores the source as `shared_ptr`. `VoxelSourceHandle` and `GeometrySourceHandle` store a `shared_ptr<const ...Source>` plus the existing `pin`. This keeps these behaviors separate:

- A handle keeps the source object alive even after manager destruction or block erase.
- The existing `pin.use_count() > 1` check continues to mean “a live manager handle exists, do not evict this resident block yet.”

## Contracts

- `valid()` returns true only when the handle owns a source.
- `source()` and `operator->()` dereference the owned source pointer.
- Cache hits return a handle sharing the resident block source and pin.
- First acquire inserts the shared source into the cache, then returns a handle that shares that source.
- Manager destruction releases the cache owner but does not invalidate already-returned handles.
- Leases remain governed by source-level keepalive semantics; keeping the source object alive preserves access to its mmap members, and leases keep their own mmap controls alive.

## Compatibility

No public method signatures change. The concrete source classes remain private to `GeometryResourceManager.cpp`. The handle classes remain copyable and default-constructible.

## Risks

- If the evictor were switched to `source.use_count()` only, lease-only references could accidentally change cache policy. This task intentionally keeps the existing `pin` gate to avoid policy drift.
- Holding handles after manager destruction can keep mapped files open longer. That is required for safe reads and matches handle ownership semantics.
