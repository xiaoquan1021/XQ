# XQ CFD Pre-processing Pipeline — Architecture

This document describes the business-logic layer of XQ's CFD pre-processing
pipeline after the 2026-04 refactor. It corresponds to the five guided
stages visible in the sidebar (Import → Trace → Contour → Build → Solve).

## 1. Decoupling contract

All five stages communicate **exclusively through the MITK DataStorage**.
No stage directly calls another stage's API. Each stage reads upstream
producer nodes by looking up well-known string properties on its own
output nodes, and writes its results back as new DataStorage nodes
tagged with:

| Property key                     | Example value            | Meaning                        |
|----------------------------------|--------------------------|--------------------------------|
| `xq.pipeline.stage`              | `path`, `contour_group`, `model`, `volume_mesh`, `simulation_prep` | Which stage produced this node |
| `xq.pipeline.version`            | `1`                      | Schema version                 |
| `xq.pipeline.algorithm`          | `dijkstra`, `threshold`, `vtk`, `tetgen` | Concrete algorithm used |
| `xq.source.image`                | `<image node name>`      | Upstream Image dependency      |
| `xq.source.path`                 | `<path node name>[;...]` | Upstream Path dependency       |
| `xq.source.contour_groups`       | `g1;g2;g3`               | Upstream ContourGroup set      |
| `xq.source.model`                | `<model node name>`      | Upstream Model dependency      |
| `xq.source.mesh`                 | `<mesh node name>`       | Upstream VolumeMesh dependency |

The helpers live in `@/home/xiaoquan/XQ/Code/Source/xq4gui/Modules/Common/xq_PipelineDataUtils.h`:

- `xq::pipeline::MarkNode(node, stage)` — stamp the stage property.
- `xq::pipeline::HasStage(node, stage)` — check it.
- `xq::pipeline::FindNodeByNameAndStage(ds, name, stage)` — resolve an upstream node.
- `xq::pipeline::ResolveUpstreamNode(ds, downstream, sourceKey, stage)` — generic upstream resolver.
- `xq::pipeline::SplitSourceList / JoinSourceList` — semicolon-separated list helpers.

## 2. Strategy/Bridge interfaces

Each stage exposes an abstract interface so algorithms can be swapped without
touching the pipeline driver. A factory chooses the default implementation.

| Stage           | Interface                    | Default impl          | Alt impl (stubbed)          |
|-----------------|------------------------------|-----------------------|-----------------------------|
| Path            | `xq_PathPlanner`             | `xq_DijkstraPathPlanner` | `xq_VmtkFastMarchingPathPlanner` (TODO VMTK) |
| 2D Segmentation | `xq_SegmentationAlgorithm`   | `xq_ThresholdSegmentation` | `xq_LevelSetSegmentation` (TODO ITK)   |
| 3D Model        | `xq_SolidModeler` (Bridge)   | `xq_VtkSolidModeler`  | `xq_OccSolidModeler` (TODO OCCT)           |
| Volume Mesh     | `xq_MeshGenerator`           | `xq_TetGenMeshGenerator` | Netgen/MMG (not yet)                     |
| Simulation Prep | `xq_SvPreWriter` (exporter)  | writes .svpre/bct.dat | —                                           |

## 3. Stage data flow

### Stage 1 — Path planning
- **Input**: Image node + seed points.
- **Output**: Path node (`xq_VesselCenterline`) with `xq.source.image`.
- **Driver**: `xq_PathPipelineService::CreatePath`.
- **Algorithm**: VMTK Fast Marching (preferred) or Dijkstra on the voxel graph.
- **Smoothing**: `xq_VtkParametricSpline::GetSplineFramePoints` produces a 3D cardinal spline plus a Frenet-like frame (`tangent`, `normal`, `rotation`) used by all downstream stages.

### Stage 2 — 2D segmentation
- **Input**: Image + Path node.
- **Output**: ContourGroup node (`xq_ContourGroup`) with `xq.source.image` + `xq.source.path`.
- **Driver**: `xq_SegmentationPipelineService::ExtractContours`.
- **Resampling**: for every Nth trace vertex, `vtkImageReslice` is fed a `vtkMatrix4x4` whose column 2 is the tangent and whose columns 0,1 are the persisted in-plane axes. The 2D slice is handed to the selected algorithm.
- **Algorithm interface**: `xq_SegmentationAlgorithm::Extract(slice, params)` returns an ordered closed polygon in slice coords; the driver lifts each polygon back to world coords using the same frame.

### Stage 3 — Solid modeling
- **Input**: one or more ContourGroup nodes.
- **Output**: Model node (`xq_Model` + `xq_PolyGeometry`) with `xq.source.contour_groups`, `xq.source.path`.
- **Driver**: `xq_ModelPipelineService::CreateModel`.
- **pathFilter** on the request restricts the set of ContourGroups to those whose `xq.source.path == pathFilter`; without the filter every group is taken.
- **Bridge**: `xq_SolidModeler::Build` returns a capped surface, a `wallCellCount`, `capCount` and a **per-cell face id vector**. Capping now assigns one face id per boundary loop, fixing the previous "uniform cap band" bug.

### Stage 4 — Volume meshing
- **Input**: Model node (verified via `xq.pipeline.stage == model`).
- **Output**: VolumeMesh node (`xq_MitkGrid` wrapping `xq_TetGenGrid`) with `xq.source.model`.
- **Driver**: `xq_MeshPipelineService::CreateVolumeMesh`.
- **Parameters** now carried correctly through the interface: global edge size, quality ratio, boundary layer count, **first height** (previously dropped), growth rate, local face sizes.

### Stage 5 — Simulation pre-processing
- **Input**: Model + VolumeMesh nodes (both stage-validated).
- **Output**: SimulationPrep node (`xq_MitkSolverJob`) with `xq.source.model` + `xq.source.mesh`.
- **Driver**: `xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep`.
- **Face-role policy (fixed)**: if the upstream Model carries `xq.source.path`, the driver resolves that Path and labels the cap whose centroid is closest to the **first trace vertex** as `inflow`; every other cap becomes `outflow`. When no path is resolvable, the legacy "first cap = inflow" heuristic is used and a warning diagnostic is emitted. User overrides in `faceRoleOverrides` always win.
- **Export**: `xq_SimulationPrepPipelineService::ExportForSolver` resolves Model + Mesh via `ResolveUpstreamNode` and calls `xq_SvPreWriter::Export`, which writes:
  - `<jobName>.svpre` — SimVascular preprocessor script
  - `mesh-complete/mesh-complete.mesh.vtu` — binary VTU of the volumetric grid
  - `mesh-complete/<face>.vtp` — per-face surface polydata
  - `bct.dat` — inlet waveform table (placeholder constant-flow if caller supplies no waveform)

## 4. Bug fixes in this refactor

| Bug site                                  | Symptom                                            | Fix |
|-------------------------------------------|----------------------------------------------------|-----|
| `xq_ModelPipeline.cxx:288-333`            | Ignores `xq.source.path`; mixes contours from unrelated paths | `pathFilter` in request + acceptPath lambda |
| `xq_ModelPipeline.cxx:209`                | `faceIds->SetValue(cellId, 1 + capCellOffset/1)` — `/1` is a placeholder | Delegated to `xq_SolidModeler::Build` which returns a real per-cell face id vector |
| `xq_ModelPipeline.cxx:243-255`            | `cellsPerCap = total / capCount` — caps with different loop sizes collapse | Each boundary loop gets its own face id directly from the modeler |
| `xq_MeshPipeline.cxx` (request)           | `boundaryLayerFirstHeight` accepted but never passed to TetGen | Added `SetBoundaryLayerFirstHeight` + propagation via `xq_MeshGenerator::Params` |
| `xq_SimulationPrepPipeline.cxx:26-34`     | "first cap = inflow" hard-coded                     | Geometric inflow detection via centerline start vertex |
| `xq_SegmentationPipeline.cxx:20-70`       | `CreateContourGroup` just made an empty container  | New `ExtractContours` drives `vtkImageReslice` + `xq_SegmentationAlgorithm` |

## 5. Known deferred work (TODO markers in code)

- **VMTK Fast Marching**: `xq_VmtkFastMarchingPathPlanner` falls back to Dijkstra until the VMTK package is wired through CMake.
- **LevelSet segmentation**: `xq_LevelSetSegmentation` falls back to Threshold until ITK GAC filter is integrated.
- **OCCT solid modeling**: `xq_OccSolidModeler` delegates to the VTK modeler until `xq_OCCTGeometry` + `BRepAlgoAPI_Fuse/Fillet` is wired through the Bridge.
- **Inflow BC**: `bct.dat` currently writes a placeholder constant-flow row when no waveform is supplied — Womersley 3D-inlet support needs a separate profile generator.
- **Outflow BC**: `.svpre` emits `pressure_vtp … 0.0` for outflow caps with a `TODO` comment; RCR/impedance models live in `xq_SolverJob` properties but are not yet serialized.

## 6. Extending the pipeline

Adding a new algorithm is a two-step process and does **not** require any
changes to `*PipelineService` code:

1. Derive a class from the stage interface (e.g. `class MyAlgo : public xq_SegmentationAlgorithm`) and implement `Name()` + the pure-virtual method.
2. Register it in the factory (`CreateSegmentationAlgorithm`, `CreateSolidModeler`, `CreateMeshGenerator`, ...) and advertise its name from the UI.

The `*Service::*` drivers will pick it up through the existing `algorithm` field
of the request struct and persist the chosen name in `xq.pipeline.algorithm`.
