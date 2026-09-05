# Implementation Plan: fix-geometry-resource-lifetime

## Steps

1. Update handle ownership
   - Change `VoxelSourceHandle` to store `std::shared_ptr<const IVoxelSource>` instead of a raw pointer.
   - Change `GeometrySourceHandle` to store `std::shared_ptr<const IGeometrySource>`.
   - Keep `pin_` in both handles for existing eviction semantics.
   - Update `valid()`, `source()`, and `operator->()` to use the owned shared source.

2. Update resident cache storage
   - Change `ResidentBlock::voxelSource` and `geometrySource` from `std::unique_ptr` to `std::shared_ptr`.
   - First-touch mapping creates `std::shared_ptr<MappedVoxelSource>` / `std::shared_ptr<MappedGeometrySource>`.
   - Cache hits and first-touch returns pass the resident shared source plus the pin.
   - Leave `evictToBudgetLocked()` using `pin.use_count() > 1`.

3. Add regressions
   - `test_geometry_resource_manager`: voxel handle survives manager destruction.
   - `test_geometry_resource_manager`: voxel lease acquired before manager destruction survives manager destruction.
   - `test_geometry_resource_manager`: geometry handle survives manager destruction.
   - Existing budget/LRU/concurrency tests must remain unchanged in meaning.

4. Validation
   - Build and run:
     `test_geometry_resource_manager`
     `test_geometry_source_resolver`
     `test_mapped_voxel_source`
     `test_mapped_geometry_source`
   - Run targeted ctest:
     `ctest --test-dir XQ/build_m9be -C Release --output-on-failure -R "geometry_resource_manager|geometry_source_resolver|mapped_voxel_source|mapped_geometry_source"`
   - Run full Release `ctest`.

## Rollback Points

- If shared source ownership changes eviction counts, inspect whether a test is accidentally holding a handle longer than intended. Do not remove the `pin` gate as part of this task.
- If a compile error appears in callers, preserve public handle method signatures and adjust only private storage/constructors.

## Files

- `XQ/src/services/resource/GeometryResourceManager.h`
- `XQ/src/services/resource/GeometryResourceManager.cpp`
- `XQ/tests/services/resource/GeometryResourceManagerTest.cpp`
- `.trellis/spec/XQ/core/source-interface.md`
