# Automatic vessel segmentation contract

## Scenario: Reuse-first ROI/topology portal-vein segmentation v4

### 1. Scope / Trigger

- Trigger: changing the automatic vascular segmentation profile, ITK adapter,
  offline ROI import, component/topology policy, MinimalPath or local level-set
  integration, artifact format, evaluator boundary, CLI predictor, or desktop
  automatic-segmentation action.
- Purpose: implement the method and ownership boundary selected in `D:/XQ`:

```text
ITK anisotropic diffusion
-> ITK Frangi/Sato multiscale vesselness
-> ITK region growing / thresholding
-> ITK ConnectedComponent / RelabelComponent
-> XQ portal topology and peripheral-branch policy
-> optional locked MinimalPath or official ITK local level-set refinement
```

- `D:/XQ` explicitly assigns pipeline assembly, parameter tuning, portal versus
  hepatic/splenic topology separation, peripheral-branch recovery and optional
  reconnection to project work. These operations are required XQ strategy, not
  forbidden local invention.
- ITK and locked upstream modules own image/path kernels. XQ must not rewrite
  diffusion, Hessian/objectness, region growing, morphology, reconstruction,
  connected-component traversal, distance transform, thinning, fast marching,
  path optimization or level-set evolution.
- Schema 1-3 and artifacts v1-v6 remain readable legacy evidence. They are
  never fallback, union input or alternate production routes.

### 2. Signatures

The schema-4 implementation introduces explicit v4 types instead of changing
the meaning of the historical `AutomaticVesselSegmentationProfileV2` type:

```cpp
AutomaticVesselSegmentationResultV4 IAutomaticVesselSegmenter::run(
    const XQImageVolume& image,
    const IVoxelSource& source,
    const XQVesselnessVolume& vesselness,
    const XQVascularRoiPriorV1& roiPrior,
    const AutomaticVesselSegmentationProfileV4& profile,
    const AutomaticVesselSegmentationCancellation* cancellation = nullptr) const;
```

The profile owns, at minimum, these explicit fields:

```cpp
struct AutomaticVesselSegmentationProfileV4 {
    std::string profileId;
    std::uint32_t schemaVersion; // exactly 4

    double coarseDomainDilationMm;
    double rootVesselnessMinimum;
    double rootIntensityMinimumHu;
    double rootIntensityMaximumHu;
    double candidateVesselnessMinimum;
    double candidateIntensityMinimumHu;
    double candidateIntensityMaximumHu;

    std::size_t additionMinimumVoxels;
    std::size_t additionMaximumVoxels;
    double additionMaximumRootContactFraction;
    double additionMaximumEndpointDistanceMm;
    double additionMaximumSecondaryPriorFraction;

    bool reconnectEnabled;
    std::size_t reconnectMinimumComponentVoxels;
    std::size_t reconnectMaximumComponentVoxels;
    double reconnectMaximumEndpointDistanceMm;
    double reconnectMinimumEndpointAlignment;
    double reconnectMaximumSecondaryPriorFraction;
    double reconnectMaximumElongation;
    double reconnectMaximumMeanVesselness;
    double reconnectMaximumPeakVesselness;
    double reconnectMinimumMeanIntensityHu;
    double reconnectMaximumMeanIntensityHu;
    std::size_t reconnectMaximumPaths;
    double reconnectPathTubeRadiusMm;
    double reconnectSupportVesselnessMinimum;
    double reconnectSupportIntensityMinimumHu;
    double reconnectSupportIntensityMaximumHu;

    bool localRefinementEnabled;
    double localRefinementRadiusMm;
    double localRefinementMaximumRmsError;
    std::uint32_t localRefinementMaximumIterations;
};
```

The exact field set may grow before profile freeze, but no production-affecting
threshold may be hidden in an implementation constant or environment variable.

```text
xq_vascular_segment_predict
  <ct-dicom-directory> <organ-roi.nii.gz> <coarse-vessel-roi.nii.gz>
  <output.xqvmask> [series-uid]

xq_vascular_segment_inspect <prediction.xqvmask>

xq_vascular_segment_evaluate
  <prediction.xqvmask> <gold-dicom-directory> <baseline-file>
  <gold-foreground-value> <label-name> [series-uid]
```

Target identity:

```text
algorithm_id      xq.itk.roi-topology-vessel-segmentation
algorithm_version 4.0.0
profile_schema    4
artifact_version  7
```

### 3. Contracts

#### Authority and dependency boundary

- Product dependencies are C++17 with the dynamic MSVC CRT, ITK exactly 5.4.0,
  GDCM exactly 3.0.10 and existing VTK exactly 9.3.0. Use explicit ITK module
  closures; never `${ITK_LIBRARIES}` or `ITK_USE_FILE`.
- TotalSegmentator exactly 2.15.0 supplies offline `liver` and
  `portal_vein_and_splenic_vein` NIfTI files. Python, torch, nnU-Net, weights
  and TotalSegmentator never enter product CMake, link, PE, launch or runtime
  closure.
- Optional path optimization reuses ITKMinimalPathExtraction commit
  `35dd8e83b7df2059876e6835a5741eb3d45973bf`, archive SHA-256
  `A2EDCCA4BC07175487BE34E0A6C1B780CC176A67E6F9A1DE20C21D55911D4FB4`,
  Apache-2.0. Its consumed source is compiled against product ITK 5.4; no
  research install prefix or second ITK is linked.
- ITKThickness3D remains the locked thinning implementation. XQ may read its
  materialized skeleton to derive portal-specific endpoint/tangent policy, but
  must not write a thinning kernel.
- Public headers expose XQ/std values only. ITK, NIfTI, GDCM, VTK and Python
  implementation types remain private to adapters.

#### Input, geometry and lineage

- CT and vesselness are finite single-component Float32 values on one LPS/mm
  grid. Source metadata/view, image geometry, vesselness geometry and all
  fingerprints must agree; `acquire_whole()` is called once per run.
- ROI input contains non-empty `Organ` and `CoarseVessel` roles with generator,
  source/aligned hashes, CT fingerprint and full LPS/mm geometry.
- ITK reads NIfTI physical geometry and aligns each role to the CT grid with
  identity physical resampling and nearest-neighbor interpolation. No manual
  axis swap or index-space assumption is permitted.
- `portal_vein_and_splenic_vein` is a coarse prior. It is never copied wholesale
  into output. Organ and coarse masks may have zero direct overlap; domain
  composition uses `organ OR physically_dilated(coarse)`, not `organ AND coarse`.

#### Schema-4 composition

1. Reuse the existing ITK anisotropic-diffusion and Frangi/Sato materialized
   vesselness stages with their exact profile and lineage.
2. Build the physical allowed ROI with ITK dilation and OR filters.
3. Build the trusted root from explicit root intensity/vesselness gates, ITK
   region growing or binary reconstruction, physical morphology and ITK
   connected-component/relabel output. The final root policy may retain more
   than one branch; largest-component-only behavior is prohibited.
4. Build a lower-threshold peripheral candidate inside the allowed ROI using
   ITK threshold/reconstruction filters. ITK labels/relabels additions. XQ may
   consume materialized component size, root contact, prior support, physical
   endpoint distance and topology features to select additions.
5. If reconnection is enabled, XQ supplies explicit physical start/end points
   and a versioned physical speed-image profile to the locked upstream path
   filter. Upstream owns fast marching/path optimization. XQ owns and records
   automatic endpoint selection; it must not attribute that policy to Mueller
   or the upstream module.
6. ITK morphology/reconstruction clips path support to the physical ROI and
   reconnects only selected additions to the trusted root. Any optional local
   refinement uses an official ITK level-set filter in an explicit bounded
   domain with all parameters persisted.
7. Fully update each upstream pipeline, materialize one XQ-owned mask, rerun ITK
   connected components/relabel for final topology diagnostics, then release
   every ITK object before returning success.

The development analyzer may expose gold-only audit columns, but every promoted
decision must be recomputed with reference access removed and produce the same
mask fingerprint. Production source, profile, CLI and desktop callers cannot
read gold, a case number, a development baseline artifact or evaluator output.

#### Result, persistence and callers

- Success records root/candidate counts, every component decision and reason,
  endpoint/topology features used by policy, path endpoints and status, speed
  profile, optional local-refinement status, exact upstream identities/stages,
  geometry/lineage hashes and deterministic output fingerprint.
- Artifact v7 writes schema-4 data atomically through a sibling `.part`, refuses
  overwrite, reopens to an equivalent XQ-owned object and retains v1-v6 reader
  compatibility without treating legacy data as production-valid v4.
- CLI and desktop construct the same schema-4 adapter/profile. The route cannot
  call the old custom `SegmentationService` threshold, 6-neighbor grow or
  largest-component kernels, schema-1/2/3 segmenters, TubeTK, or a fallback
  union. The independent 2D contour workbench remains separate.
- Case 1 is development data: one documented hypothesis at a time, with profile,
  mask hash, metrics and rejection/acceptance recorded. Case 5 remains unopened
  until schema 4 and all values are frozen after automated and user visual
  acceptance.

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| schema is not 4, profile id missing, or any physical/policy value is invalid | typed `InvalidProfile`; no source or ITK execution |
| non-LPS, non-finite/singular geometry, invalid spacing/dimensions | typed `InvalidGeometry`; no output |
| source metadata/type/byte count differs from image | typed `InvalidSource`; no partial output |
| vesselness geometry/buffer/fingerprint differs | `InvalidVesselness` or `LineageMismatch`; no ITK output |
| ROI role missing, empty, invalid or lineage mismatched | `InvalidRoiPrior` or `LineageMismatch`; no output |
| organ/coarse have zero overlap but are physically valid | valid; use `organ OR dilate(coarse)` |
| empty ROI, root, candidate or final output | typed stage-specific failure; no artifact |
| all-foreground root/candidate/final mask | typed processing failure; no artifact |
| selected reconnection has invalid endpoint, non-finite speed or upstream path failure | typed reconnection failure; discard all partial paths/masks |
| optional level-set output is non-finite, leaves its domain or fails to converge per profile | typed refinement failure; no fallback |
| cancellation before/between stages | `Cancelled`; discard every partial ITK/XQ value |
| ITK allocation failure or `std::bad_alloc` | `AllocationFailed`; no half artifact |
| ITK/other exception or geometry drift | typed `ProcessingFailed`; no legacy fallback |
| evaluator frame differs from prediction | evaluation failure; no resampling shortcut |
| case-1 metric misses a frozen line | record failure and next hypothesis; never call it correct |

### 5. Good/Base/Bad Cases

- Good: real portal-venous CT, standard offline ROI and a frozen schema-4
  profile run the ITK root/candidate chain; XQ selects supported peripheral
  additions; locked MinimalPath reconnects explicit endpoints; artifact v7
  reopens and is independently evaluated and rendered in the desktop app.
- Base: an anisotropic synthetic branching tube with one disconnected distal
  segment proves physical thresholds, multiple-component retention, endpoint
  selection, upstream path invocation, ROI clipping, deterministic provenance,
  cancellation and no-partial-state failures.
- Bad: copy/union the coarse prior, keep only the largest component, implement
  flood fill or shortest path in XQ, select a profile after opening case 5,
  branch on case identity, let gold affect prediction, run Python at prediction
  time, silently use schema 3, or present a compiling but metric-failing mask as
  correct.

### 6. Tests Required

- Profile tests validate every schema-4 threshold, ordering constraint,
  physical unit, component/topology limit, reconnection value and optional
  refinement value; fingerprint changes for every production-affecting field.
- Synthetic adapter tests execute real ITK threshold/reconstruction,
  connected-component/relabel, thinning/distance and the locked MinimalPath
  path. Assert explicit endpoints, path/domain clipping, multi-branch retention,
  exact geometry, deterministic hashes, cancellation and zero partial state.
- Gold-firewall tests delete/rename/change evaluator and reference inputs and
  assert the production mask fingerprint is unchanged. Production link/source
  inspection must find no evaluator or case identity dependency.
- Artifact tests round-trip v7 profile, ROI lineage, component decisions,
  topology features, path records and upstream hashes. v1-v6 remain readable
  but cannot satisfy schema-4 production validation.
- Controller/main-window tests invoke the same schema-4 adapter as the CLI and
  prove failure never mutates Scene. Old 3D threshold/region-grow and the 2D
  workbench are not fallbacks.
- Architecture/dependency tests prove public-header purity, exact ITK/GDCM/VTK
  identities, locked MinimalPath source hashes, explicit module closure and no
  TubeTK/Python/TotalSegmentator/second-ITK product closure.
- Before profile freeze, case 1 must pass Dice `>=0.70`, HD95 `<=3.2 mm`, ASSD
  `<=1.2 mm`, clDice `>=0.75` and largest 26-connected component fraction
  `>=0.90`; then the same artifact must be loadable and accepted in MPR/3D.
- Run affected Release targets, focused tests, Flow-ON and Flow-OFF full Release
  CTest, build-graph/dependency/PE audits and `git diff --check` sequentially.

### 7. Wrong vs Correct

#### Wrong

```cpp
auto output = keepLargestConnectedComponent(
    oldSchema3Mask | coarsePriorMask | locallyWrittenShortestPath(candidate));
if (caseNumber == 1 && diceAgainstGold(output) < 0.70) {
    output = fallbackMask;
}
```

#### Correct

```cpp
const auto prior = roiReader.read(image, inputFingerprint, roiFiles);
const auto profile = portalVenousCtAutomaticSegmentationProfileV4();
const auto result = segmenter.run(
    image, source, vesselness, *prior.prior, profile, cancellation);
if (!result.ok()) {
    return result; // typed failure, no fallback and no partial Scene commit
}
```
