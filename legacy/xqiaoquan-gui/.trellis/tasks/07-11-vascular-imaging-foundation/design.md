# Design: 血管影像外部依赖地基

## 1. Design Position

本任务不再扩写第二套“宿主壳”。现有 Project/Scene/Asset/Source/command/GUI 是宿主，新增工作聚焦外部成熟 kernel 构成的影像与血管几何生产链。

设计原则：

1. ITK 管三维医学影像、分割与形态学；VTK 管表面、几何与显示。
2. 外部类型止于 adapter/private implementation，XQ 自有类型穿过 services/core/Scene。
3. 自动算法与参考标注严格分路：金标只能评价，不能喂给成功路径。
4. 每一层都保留 LPS/mm/direction/provenance，禁止靠 GUI 看起来对齐来代替数值守恒。
5. vmtk、TetGen 等高风险依赖均有明确探针、开关和失败结论；fallback 必须通过同一门。

## 2. Target Data Flow

```text
DICOM directory + explicit SeriesInstanceUID
  -> GdcmItkDicomSeriesReader
  -> XQImageVolume + IVoxelSource
  -> VascularImageService
       -> ItkVascularPreprocessor (private itk::Image<float,3>)
       -> vesselness / diagnostics (XQ-owned output)
       -> ItkAutomaticVesselSegmenter
       -> XQSegmentationMask
  -> VascularGeometryService
       -> private ITK<->VTK bridge
       -> closed XQ surface
       -> ICenterlineExtractor
            A: vtkvmtk C++ adapter
            B: validated 3D skeleton + distance map adapter
       -> XQ centerline tree (points, radii, graph, provenance)
       -> ITetMesher / MMG production backend
       -> XQ surface + volume mesh
  -> atomic command bundle
       -> Scene nodes + source/derived relations + assets
  -> renderer / save / reopen
```

## 3. Layer Boundaries

### 3.1 core

Core remains free of ITK/VTK/GDCM/vmtk/TetGen/MMG. It owns only stable data contracts.

Required XQ-owned concepts:

- Existing `XQImageVolume`, `IVoxelSource`, `XQSegmentationMask`, surface/mesh handles.
- A tree-capable centerline result, containing:
  - version and coordinate system (`LPS`, `mm`)
  - stable graph node/branch/sample identifiers
  - point, tangent, arc length and finite positive radius per sample
  - parent/child connectivity and endpoint/bifurcation classification
  - input revision/fingerprint, algorithm/backend/version and parameter fingerprint
  - quality flags and diagnostics

The final name and persistence shape are implementation details, but `XQPath` control points alone are insufficient because they do not own extracted radius and tree topology.

### 3.2 adapters

- `adapters/gdcm`: keep the existing production DICOM reader.
- `adapters/itk`: own import to 3D ITK images, diffusion, Hessian vesselness, segmentation, morphology, connected components, distance maps and optional level-set refinement.
- `adapters/vtk` or visualization private implementation: own ITK-VTK bridge, surface extraction and VTK lifetime.
- `adapters/vmtk`: optional centerline backend; only minimal C++ sources and required patches.
- `adapters/tetgen`, `adapters/mmg`: existing kernels, enabled only after ABI/license gate.

No public adapter header may expose `itk::`, `vtk::`, `tetgenio`, MMG or vtkvmtk types.

### 3.3 services

Services own orchestration, validation and atomic result preparation:

- `VascularImageService`: validates image/source and parameter profile, invokes ITK backend, returns XQ-owned vesselness/mask plus structured diagnostics.
- `VascularGeometryService`: validates masks/surfaces, selects the approved centerline backend, builds centerline tree and mesh results.
- A workflow/controller layer submits a single command bundle only after every required output passes validation.

Services do not depend on Qt and do not contain copied ITK algorithms.

### 3.4 app / visualization

The app selects input/parameter profile, shows progress and diagnostics, previews vesselness/mask/tree/mesh, and submits service-produced commands. It does not perform thresholding, coordinate conversion, component selection, radius calculation or topology repair.

## 4. Image and Coordinate Contract

### 4.1 Canonical space

- Coordinate system: DICOM/ITK LPS.
- Spatial unit: mm.
- Image geometry: dimensions, spacing, origin and full 3x3 direction.
- Scalar buffer: canonical modality-rescaled values already used by the DICOM reader.

### 4.2 XQ -> ITK

- Acquire the whole or required region through `IVoxelSource` once per block.
- Construct/import `itk::Image<float,3>` with the exact XQ geometry.
- Do not reset origin/direction to identity and do not process each axial slice independently.
- Any resampling must emit the explicit transform and output geometry.

### 4.3 ITK -> XQ

- Persist final scalar/mask results as XQ-owned buffers/objects.
- Preserve source node, image fingerprint, algorithm version and parameter fingerprint.
- Intermediate ITK object graphs stay private and disposable.

### 4.4 ITK -> VTK

- Use ITKVtkGlue official bridge in an adapter/private implementation.
- Immediately establish VTK ownership/lifetime appropriate for lazy pipelines; no borrowed ITK buffer may dangle after adapter return.
- Copy/set the complete direction matrix supported by VTK 9.3 and numerically test physical points.

## 5. Three-Dimensional Traditional Segmentation

The first production baseline is a deterministic, profile-driven chain:

1. Convert canonical scalar volume to float 3D ITK image.
2. Curvature or gradient anisotropic diffusion using physical spacing.
3. Multi-scale Hessian objectness/vesselness; sigma range is specified in mm.
4. Combine vesselness with modality intensity constraints.
5. Generate high-confidence foreground automatically, expand with hysteresis/connected rules, and optionally refine with a level set seeded by that automatic mask.
6. Apply ITK morphology and component labeling/relabeling.
7. Score/retain components using explicit, versioned rules; do not blindly keep only the largest component.
8. Convert to `XQSegmentationMask`, validate geometry and submit only a complete result.

The interface remains vascular-site neutral. The first validated site uses a versioned parameter profile chosen by the data-gate task. Per-case manual seed or case-specific tuning is not part of the accepted production path.

## 6. Centerline Strategy

### 6.1 Shared output contract

Both backends produce the same XQ centerline tree and are checked by the same validator and real-data metrics.

### 6.2 Backend A: vtkvmtk

Probe sequence:

1. Freeze the exact source revision and license.
2. Build only required vtkvmtk C++ modules against VTK 9.3.0 and the same MSVC/C++ runtime.
3. Run a minimal synthetic regression and then one real closed vessel surface.
4. Apply/document the VTK 9.3 centerline compatibility fix where needed.
5. Reject the backend if reproducible crashes, unresolved ABI mismatch, non-acceptable license or unstable real output remains after the bounded probe.

Success is not “compiled”; it is a real centerline with maximum-inscribed-sphere radius and valid topology.

### 6.3 Backend B: 3D skeleton + distance map

The installed ITK currently has 2D `BinaryThinningImageFilter`, not the planned `BinaryThinningImageFilter3D`. The fallback therefore requires one of:

- enabling and pinning an auditable ITK remote module that provides true 3D thinning; or
- integrating another pure C++ 3D skeletonization implementation behind the same adapter boundary.

Radius comes from a spacing-aware signed distance map. Skeleton voxels are converted to a graph, cycles/spurs are treated with documented rules, and branches are resampled in LPS mm. The fallback does not pass unless it meets the same continuity/radius/topology gates as vtkvmtk.

## 7. Surface and Volume Meshing

### 7.1 Surface

- Generate isosurface from the accepted mask using VTK.
- Clean duplicate points/cells, orient normals, close only defects allowed by a documented rule and preserve source labels/markers.
- Record topology before/after smoothing and simplification.

### 7.2 Volume

- Existing `ITetMesher` is retained as the service boundary.
- TetGen fills the closed PLC; MMG improves the generated mesh.
- Existing programmatic curved-tube tests remain fast regressions.
- Final gate runs the selected backend on a real CTA-derived vessel surface and measures actual cell quality and containment.
- If the TetGen license gate fails, the parent task stays incomplete until a compliant fill backend is selected. MMG alone is not misrepresented as a surface-to-volume generator.

## 8. Data Gate and Metric Freeze

The validation bundle is external to Git and contains:

```text
dataset/
  SOURCE.md
  LICENSE
  manifest.sha256
  images/<case>/dicom/...
  labels/<case>/...
  geometry-map.json or equivalent verified mapping
  validation-baseline.md
```

`validation-baseline.md` is written before final algorithm runs and freezes:

- train/tune/validation case split, if the dataset size permits
- allowed parameter profiles
- Dice and boundary metric
- centerline overlap/continuity metric
- radius error metric
- topology metric
- mesh validity and quality thresholds

No threshold may be weakened after seeing final validation results without reopening planning and recording the reason.

## 9. Atomicity, Provenance and Persistence

- Each derived object records source node/revision, algorithm/backend/version, parameter fingerprint, data fingerprint and timestamp/build identity without PHI.
- Image -> mask -> tree -> surface/mesh relations are explicit.
- Failure before final validation produces no Scene mutation.
- A successful workflow returns a command bundle so undo/redo removes/restores the complete derived group.
- Reopen does not rerun the algorithm silently; it loads persisted products and marks them stale if source revision/fingerprint changed.

## 10. Build Profiles

At minimum, implementation must maintain these real configurations:

1. Default XQ regression build.
2. Vascular foundation build with required ITK/GDCM/VTK modules.
3. Approved centerline build (`vtkvmtk` or fallback module) with its real test target.
4. Volume-mesh build with the approved fill backend and MMG enabled.

All required build trees are tested sequentially because current tests share stable temporary paths. Optional OFF builds may prove isolation, but an OFF build cannot satisfy an ON capability gate.

## 11. Failure and Rollback Rules

| Failure | Required response |
| --- | --- |
| CTA dataset unavailable/unclear license | Reject it and select another dataset; do not substitute LIDC |
| vtkvmtk ABI/crash/license fails | Record probe evidence and activate the real 3D skeleton fallback |
| 3D thinning dependency unavailable | Task remains blocked at centerline backend; do not use 2D thinning or manual Path |
| TetGen license incompatible | Keep volume-mesh acceptance open and select a compliant fill backend |
| Automatic segmentation misses gate | Report metrics/failures, improve the traditional chain or revisit scope with user; do not feed gold mask |
| GUI looks correct but coordinate tests fail | Coordinate gate wins; visual compensation is rejected |
| Full CTest green but real gate fails | Task is not complete |

## 12. Deliberate Non-Design

This task does not add a plugin marketplace, generic operation registry, MITK/Slicer host, Python interpreter, DL runtime, Flow schema, Darcy/CTC contracts or semantic scale scheduler.
