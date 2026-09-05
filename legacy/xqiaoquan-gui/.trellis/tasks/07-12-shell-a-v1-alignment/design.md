# Design: 壳档 A v1 对齐

## 1. Design Boundary

本任务补齐壳 v1 的最小机制，不重写已有 Scene/Project/Source/GUI/Flow，也不把血管影像 v2 算法塞进壳门闩。

```text
Existing DICOM / Project / Scene / Profile / ScaleSlot
                      |
                      v
         immutable VesselPathV1 snapshot
                      |
          +-----------+-----------+
          |                       |
 Path-only static modules    geometry-only smoke
          |
  Noop / PathValidate

XQ-owned binary mask
  -> private ITK 3D thinning adapter
  -> physical distance radius
  -> graph/prune/main path
  -> XQPath + VesselProfileV1
  -> VesselPathV1 snapshot
```

## 2. Reuse and Authority

- `XQPath` remains navigation/resampling/frame geometry; it is not silently mutated into a second physical-radius authority.
- `VesselProfileV1` remains the persisted physical geometry authority and preserves area/evidence/lineage.
- New `VesselPathV1` is an immutable module-input snapshot/view with A5 semantics. It is deterministically derived from validated `VesselProfileV1` or centerline-B output and may be dumped for evidence, but is not independently edited in Scene.
- Radius derived from Profile area is `sqrt(areaMm2 / pi)`; no alternate radius storage is allowed in the same snapshot.
- Existing Flow geometry smoke remains a separate backend L0 consumer and is not reused to fake `smoke_geometry_only`; it is not a Shell-A GUI entry. The shell's fifth page is Path/Modules in both Flow ON and OFF builds.

## 3. VesselPathV1 Contract

Provisional XQ-owned contract shape:

```cpp
enum class VesselPathSource {
    AutomaticCenterlineB,
    SemiAutomatic,
    GoldFile,
};

struct VesselPathStationV1 {
    Point3 positionLpsMm;
    double radiusMm;
    double arcLengthMm;
};

struct VesselPathV1 {
    static constexpr unsigned ContractVersion = 1;
    unsigned contractVersion;
    std::string frameOfReferenceId;
    VesselPathSource source;
    DerivationStamp derivationStamp;
    std::vector<VesselPathStationV1> stations;
};
```

The child may refine names, but it may not weaken these invariants:

- contract version exact;
- LPS/mm and non-empty frame;
- at least two stations;
- finite positions/radii/arclengths;
- every radius positive;
- arclength starts at a documented origin and is non-decreasing;
- source is explicit and maps to actual evidence;
- construction is deterministic for the same input stamp.

`VesselPathAdapter::fromProfile` validates Profile first, derives radius and source, and returns either a complete snapshot or typed issues. It never returns a partial path.

## 4. Path-only Module Boundary

The module layer belongs in a pure C++ service namespace, not core and not Qt app code.

```cpp
class IShellPathModule {
public:
    virtual ~IShellPathModule() = default;
    virtual ShellModuleDescriptor descriptor() const = 0;
    virtual ShellModuleResult run(const VesselPathV1&) const = 0;
};
```

- `ShellPathModuleRegistry` owns a deterministic static set by module ID.
- `NoopShellPathModule` proves registration and invocation without changing project state.
- `PathValidateShellModule` runs the shared validator and returns a stable diagnostic report.
- Registry/module APIs may not accept `XQScene`, `XQContourGroup`, Qt, ITK, VTK or Flow DTOs.
- App/session wiring may expose module availability, but the registry is not conflated with `WorkflowCapabilities`.
- Flow OFF excludes Flow execution only; shell modules remain available.

## 5. Geometry-only Smoke

`ShellGeometrySmokeService` consumes only a validated `VesselPathV1` and produces a deterministic, non-physical report such as station/segment count, total length, radius range and stable input stamp.

- It performs no solver call, no unit conversion to CGS and no Scene commit by default.
- Bad path returns typed diagnostics and no report.
- Repeated execution over the same snapshot yields identical output.

## 6. Centerline B

### Input boundary

- Input is an XQ-owned 3D binary mask/voxel source with full geometry.
- Gold mask or semi-automatic mask is allowed as a shell fixture; its source must be disclosed.
- The adapter privately constructs `itk::Image<unsigned char,3>` and preserves spacing/origin/direction.

### Kernel path

1. Use the audited ITKThickness3D v5.3.0 source lock or another explicitly approved true 3D ITK thinning implementation.
2. Execute binary 3D thinning with vmtk disabled.
3. Execute spacing-aware SignedMaurer/equivalent physical distance map.
4. Build a 26-neighbour skeleton graph in physical space.
5. Identify endpoints/junctions, remove short spurs using an mm threshold, and select one deterministic main path for档 A.
6. Sample positive distance radius, transform indices to LPS physical points, compute non-decreasing arclength.
7. Publish XQ-owned `XQPath` plus segmentation-derived `VesselProfileV1`; derive module snapshot and validate.

### Failure behavior

- Empty mask, no skeleton, disconnected unusable graph, non-positive radius, invalid geometry/direction or ambiguous main-path selection returns stable diagnostics.
- Failure commits no Path/Profile/Scene/Asset partial state.

### Acceptance depth

- Synthetic straight/curved/branched masks test math and edge cases.
- At least one disclosed non-PHI mask fixture proves the real adapter path.
- This child does not claim automatic segmentation quality, anatomical tree correctness or radius gold accuracy.

## 7. Canonical Build and Runtime Entry

- The verified dependency-remediation roots become one shared canonical configuration source consumed by Flow ON, Flow OFF and GUI run wrappers.
- Exact package dirs remain explicit; the broad `install/windows-x64` root is not used as a discovery catch-all.
- Qt points to isolated `windows-x64-vascular/qt-6.7.0`.
- Locked Python 3.11 is configure-only because installed ITK/VTK metadata requires it; host Anaconda is forbidden.
- Fresh canonical build directories are used rather than mutating evidence trees in place.
- Full CTest runs are sequential. Build graph and recursive PE checks are mandatory for both required product configurations.

## 8. Persistence and Compatibility

- Prefer no project schema change: persist existing `XQPath` and `VesselProfileV1`; reconstruct the module snapshot after load.
- If a schema change becomes unavoidable, the owning child must stop and update its PRD/design before implementation.
- Historical FlowResult/Path/Profile objects remain readable with Flow OFF.
- Existing command/lineage/stale/undo rules apply to centerline-B publication.

## 9. Acceptance Evidence

The final child produces one A1–A15 matrix with exact commands and artifacts:

- dependency/configure/build/CTest logs;
- real DICOM sample ID and metadata summary;
- Path dump and source label;
- geometry smoke report;
- Noop/PathValidate registry/run log;
- vmtk OFF thinning execution proof;
- GUI checklist, screenshot and user physical-machine record;
- limitations/non-claims page.

No individual child may declare the parent complete.

## 10. Rollback

- New module/path/thinning code remains additive behind narrow interfaces.
- Canonical scripts can point back to the prior build trees without deleting them.
- ITKThickness3D source integration is isolated to its adapter and can be removed without changing core public types.
- Preserve all existing untracked files and build evidence; no broad clean/reset operations.
