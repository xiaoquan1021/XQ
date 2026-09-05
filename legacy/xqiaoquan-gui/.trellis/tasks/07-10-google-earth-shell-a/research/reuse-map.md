# Reuse Map: 壳档 A 与后续目标

## Reuse directly in the runtime

| Asset | Reuse role | Boundary |
| --- | --- | --- |
| XQ Project/Scene/Payload/Asset/Command infrastructure | Authoritative data spine, lineage, stale, undo and persistence | Extend; do not replace |
| XQ Source/mmap/resource management | Large voxel/geometry access | Preserve source contracts |
| Qt/VTK GUI, MPR, renderer, LOD, progressive upload | Visualization host | LOD remains rendering-only |
| ITK 5.4 + GDCM/ITKIOGDCM | DICOM series IO and image metadata | Private adapter, XQ-owned outputs |
| Existing Path/Contour workflow | Navigation and measurement evidence | Feed `VesselProfileV1`; not solver authority by themselves |
| FlowSolver1D/RCR | First real consumer smoke | Fixed smoke only; no M5 claim |
| `ITetMesher` / `ILevelSetSegmenter` pattern | Model for future narrow typed scientific ports | No universal module context |

## Reuse conditionally or later

| Asset | Use | Timing |
| --- | --- | --- |
| TetGen/MMG | Optional mesh backends | Not a shell-A gate |
| SimVascular sample artifacts | Existing real workflow regression | Keep `0007_H_AO_H` gate |
| Boileau/openBF | 1D numerical validation | M5, not shell A |
| BodyParts3D/Visible Human/TotalSegmentator outputs | Whole-body coarse context | Post-shell, atlas/data only |
| Debbaut/Vidotto methods | Darcy microcirculation and 1D-3D coupling | Post-M5 |
| PhysiCell/BioFVM | Local primary/metastatic tissue model | Cell/tissue phase |

## Reuse as ideas only

- Slicer/MITK: data-node, service boundary and provenance concepts; not the application/plugin runtime.
- 3D Tiles: content identity and hierarchical refinement concepts; not Cesium/WGS84 runtime.
- Physiome/CellML: model version/description ideas for later; not a shell-A dependency.

## Do not import

- 3D Slicer/MITK/CTK as the host application.
- Cesium or a geospatial engine.
- vmtk SuperBuild.
- Embedded Python or Python-based runtime pipelines.
- Dynamic DLL plugin discovery and ABI lifecycle.

