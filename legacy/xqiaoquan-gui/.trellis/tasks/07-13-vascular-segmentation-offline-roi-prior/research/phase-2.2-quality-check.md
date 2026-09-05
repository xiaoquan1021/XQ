# Phase 2.2 quality check

Recorded: 2026-07-13

## Status

The implementation/build checks are green. The one fixed-profile case-1 run is
functionally complete but fails the frozen quantitative lines. No parameter,
profile or algorithm change was made in response. Real-machine acceptance is
still pending, so the Trellis task remains `in_progress` and is not committed,
finished or archived.

## Product behavior checked

- The desktop segmentation page exposes one three-dimensional action:
  active image + output name + explicit liver/coarse-vessel NIfTI paths ->
  `preprocess -> ROI reader -> automatic v2 segmenter -> scene command`.
- The old three-dimensional threshold, estimate, seed and region-grow controls
  are absent. The separate two-dimensional contour workbench remains in scope
  of its own service and is not a fallback from automatic v2.
- The controller publishes the materialized mask and its source-image relation
  with `AddNodeWithSourceRelationCommand`; undo/redo and failure short-circuiting
  are covered by the controller checks.
- A 50 ms delayed window-layout pass could hide a stage dock opened immediately
  after show. The callback now runs only while the dock is still explicitly
  hidden, so it cannot override a user stage action.

## Release build and tests

- Canonical Flow-ON wrapper completed a fresh Release clean build: 353 build
  steps, followed by the dependency build/PE checker.
- Dependency checker result: build graph clean; 95 PE binaries scanned.
- An initial full CTest run exposed the dock timing defect above: 99/100, with
  only `test_main_window` failing before the new segmentation assertions.
- After the scoped fix, the adjacent GUI group passed 3/3 and the final
  single-threaded Release suite passed 100/100 in 20.88 seconds.
- `git diff --check` passed.

## Fixed case-1 product run

The real predictor was run exactly once with:

- frame-v1 case-1 CT DICOM;
- TotalSegmentator 2.15.0 standard 1.5 mm `liver` and
  `portal_vein_and_splenic_vein` files;
- built-in profile `xq.portal-venous-ct.auto-segmentation.roi.v2`;
- ITK 5.4.0 algorithm identity
  `xq.itk.automatic-vessel-segmentation` version `2.0.0`.

The predictor completed preprocessing, ROI import/alignment and all nine
recorded upstream ITK stages. Artifact v5 was written and reopened with matching
integrity:

- artifact path:
  `D:/XQ/data/vascular/3D-IRCADb-01/derived/offline-roi-v1/3Dircadb1.1/totalsegmentator-2.15.0-standard/xq-automatic-vessel-segmentation-v2-fixed.xqvmask`
- artifact SHA-256:
  `424a224a1744e058573377280da7a8d6d8a7df778b9e3f62a973a67c673bdc34`
- input fingerprint:
  `xq-vascular-input-v1:sha256:cc7577325310c74443fd8e9e6e2140490708d20ab256fe7c6cdb8bdf90d92781`
- ROI-prior fingerprint:
  `xq-vascular-roi-prior-v1:sha256:c818af5db517ffe7e5b790f2332a2813a3d101dc6e7a14b48fe184a18ce20586`
- profile fingerprint:
  `xq-automatic-vessel-profile-v2:sha256:1448aae5519597c992a7829215e14d4d8d9be3e40b5f0cd6e038142924f5bce4`
- output fingerprint:
  `xq-automatic-vessel-mask-v2:sha256:6c49b71e933b85bed82e60de152260122efd2a982b5a5596d628e9d3e57b07ee`

The separate evaluator was then run exactly once. It opened and hashed the
closed prediction before opening case-1 `portalvein` gold. Results were:

| Metric | Actual | Frozen line | Result |
| --- | ---: | ---: | --- |
| Dice | 0.0656456983 | >= 0.70 | fail |
| prediction-to-reference P95 | 30.2240334 mm | report | recorded |
| reference-to-prediction P95 | 73.2234344 mm | report | recorded |
| HD95 | 73.2234344 mm | <= 3.2 mm | fail |
| ASSD | 14.6386223 mm | <= 1.2 mm | fail |
| clDice | 0.0376997249 | >= 0.75 | fail |
| components | 1 | report | recorded |
| largest-component fraction | 1.0 | >= 0.90 | pass |

The prediction contains 1,726,625 foreground voxels versus 103,533 reference
voxels; precision is 0.0347909940 and recall is 0.5802111404. This is severe
over-segmentation evidence, not authority to tune the fixed profile.

## Real failure path

The production ROI reader was invoked with the real case-1 CT, the valid liver
ROI and a confirmed-missing coarse-vessel path. It returned:

```text
roi.status=source_read_failed
roi.diagnostic.0=vascular_roi.source_read_failed
roi_process.exit_code=5
```

No ROI prior or segmentation artifact was produced, and the automatic
segmentation predictor was not invoked again.

## Closure audit

- Exact product roots remain ITK 5.4.0, GDCM 3.0.10, VTK 9.3.0, isolated Qt
  6.7.0, HDF5 1.14.3 and TinyXML2 8.0.0; generated compile flags are C++17 and
  dynamic CRT (`-MD`).
- Product source/link inspection found no package-wide `${ITK_LIBRARIES}` or
  `ITK_USE_FILE`, no research TubeTK/MinimalPath prefix, and no Python, torch,
  nnU-Net or TotalSegmentator runtime dependency. TotalSegmentator strings in
  product code are data-lineage identity only.
- No app/UI/product-predictor reference remains to `SegmentationService`,
  `prepareThreshold`, `prepareRegionGrow`, `ThresholdIntent` or
  `RegionGrowIntent`.
- `test_arch_boundaries` confirms public core/controller headers expose no ITK,
  VTK, GDCM, Qt, NIfTI or Python implementation type.
- Held-out case 5 was not opened.

## Remaining gate

The user must run the desktop workflow on the real machine and judge the visible
result/failure behavior. The fixed case-1 metrics are below the frozen baseline;
any response that changes algorithm semantics or profile values must return to
planning with paper/upstream authority instead of being implemented in this
task's quality-check phase.

## Real-machine reachability correction

A subsequent user run invalidated the earlier desktop-reachability assertion.
The case-1 DICOM imported and rendered correctly, but the toolbar Segmentation
action still swapped the dock to the independent two-dimensional contour panel.
The ROI-v2 page existed in `xqStagePanel`, so the prior GUI test passed by finding
its children without proving that a user action made the page visible.

The scoped correction now makes the toolbar Segmentation action show the
three-dimensional ROI-v2 page against the MPR. That page exposes one explicit
`2D Contour Workbench` command for the separately retained path-contour workflow;
returning through the toolbar restores ROI-v2. The two-dimensional threshold,
region-grow and level-set controls remain outside the ROI-v2 page and are not a
three-dimensional fallback.

Verification after the correction:

- Qt 6.7 `lrelease`: 435 finished translations, 0 unfinished.
- Focused Release checks: `test_main_window` and `test_i18n_resources`, 2/2.
- Incremental full Flow-ON Release build: success.
- Final single-threaded Flow-ON Release CTest: 100/100.
- `git diff --check`: pass.
- The rebuilt canonical `xq_app.exe` was launched through `run_xq.bat`.
- Windows UI Automation invoked the real toolbar Segmentation button and found
  visible, finite-bounds liver ROI, coarse-vessel ROI, Automatic Segmentation and
  2D Contour Workbench controls. It then opened the contour workbench, observed
  the visible threshold action, returned through the toolbar, and re-observed the
  three ROI-v2 controls.

This corrects entry reachability only. No predictor/evaluator run, parameter
change, algorithm change or held-out case access occurred. User inspection of
the actual segmentation output remains the final acceptance gate.

## Superseding paper-backed v2.1 case-1 run

After the rejected v2.0 composition was replaced by the complete
Aylward-Bullitt / ITK-TubeTK 1.3.5 workflow, the final automated gate passed:

- canonical Flow-ON Release CTest: 100/100;
- canonical Flow-OFF Release CTest: 92/92;
- dependency build/PE audit: clean build graph, 97 binaries scanned;
- `git diff --check` and the forbidden-call-path audit: pass.

The corrected predictor was then invoked exactly once with the same frame-v1
case-1 CT and standard TotalSegmentator 2.15.0 liver/coarse-vessel files. No
profile value was changed and no second prediction was run. The product result
was:

- algorithm: `xq.itk-tubetk.automatic-vessel-segmentation` `2.1.0`;
- ITK / TubeTK: `5.4.0` / `1.3.5`;
- working grid: `512 x 512 x 362`, isotropic `0.569999992847443 mm`;
- working/domain/seed voxels: `94,896,128 / 8,111,565 / 139,864`;
- extracted tubes / points: `13 / 8,918`;
- radius-rasterized working voxels: `83,241`;
- CT-grid foreground/components: `29,487 / 3`;
- artifact:
  `D:/XQ/data/vascular/3D-IRCADb-01/derived/offline-roi-v1/3Dircadb1.1/totalsegmentator-2.15.0-standard/xq-automatic-vessel-segmentation-tubetk-v2.1.0-fixed.xqvmask`;
- artifact SHA-256:
  `94b3912721eb5de16fe47673b3c0b61ac804b2bcea1eb3a09cd61f05d68b6f68`;
- profile fingerprint:
  `xq-automatic-vessel-profile-v2:sha256:8776a2559a8b9d3b50b7c3be380519f863404e4aaa996acc920ef7d66baf3a08`;
- output fingerprint:
  `xq-automatic-vessel-mask-v2:sha256:a5f64b4447d3e4e9474e205283f15f0ab53af60809f44f59be279a11a028f23c`.

The separate evaluator was invoked exactly once. It opened and hashed that
closed artifact before opening the case-1 portal-vein label. Actual metrics:

| Metric | Actual | Frozen line | Result |
| --- | ---: | ---: | --- |
| Dice | 0.3804991731 | >= 0.70 | fail |
| precision | 0.8582426154 | report | recorded |
| recall | 0.2444341418 | report | recorded |
| prediction-to-reference P95 | 9.789795876 mm | report | recorded |
| reference-to-prediction P95 | 73.91715240 mm | report | recorded |
| HD95 | 73.91715240 mm | <= 3.2 mm | fail |
| ASSD | 17.46218709 mm | <= 1.2 mm | fail |
| clDice | 0.3765716386 | >= 0.75 | fail |
| components | 3 | report | recorded |
| largest-component fraction | 0.9671380608 | >= 0.90 | pass |

This correction removes the prior liver/abdominal mass failure: foreground
dropped from `1,726,625` to `29,487` voxels and precision rose from
`0.0347909940` to `0.8582426154`. It instead exposes severe under-segmentation:
only `25,307 / 103,533` reference voxels are recovered. The paper-backed method
is implemented and operable, but this fixed CTA-derived profile is not an
acceptable portal-venous case-1 algorithm.

TubeTK emitted `NAN at ...` messages for some ridge candidates. The vendored
1.3.5 `RidgeExtractor` explicitly detects non-finite spline value/derivative/
Hessian evaluations, zeros that candidate's ridge measures and rejects it.
These messages do not identify non-finite radii in the 13 returned tubes; the
binary result and artifact remained valid. The upstream source is not patched.

No tuning, repeat prediction/evaluation, alternative algorithm, baseline union
or held-out case-5 access follows this result. The task remains `in_progress`
for user visual judgment and a planning decision about method applicability.

## Superseding geodesic-active-contour v3 result

The method required by `D:/XQ` was subsequently restored as schema 3:
Frangi/Sato vesselness, the standard TotalSegmentator organ/coarse ROI and ITK
GeodesicActiveContour. TubeTK was removed from the product target graph and
artifact v5 became read-only.

After Flow-ON `100/100`, Flow-OFF `92/92`, clean build/PE audits, dependency
negative probes and `git diff --check` passed, v3 case 1 prediction and the
independent evaluator each ran exactly once. The artifact is valid v6 and
reopens with SHA-256
`ae578e03ac97a6f5eac5ae7a885e89bc91236eecffab14b2309f3de9aad362ca`.
Actual Dice is `0.484205228`, precision `0.947540245`, recall `0.325191002`,
HD95 `77.3510742 mm`, ASSD `20.9378990 mm` and clDice `0.304502753`.
The result is severe under-segmentation and fails the frozen quantitative
lines. No tuning or repeat run follows.

The complete immutable evidence is in
`research/geodesic-active-contour-v3-case1-result.md`. User visual acceptance
remains pending, so this task stays `in_progress`, uncommitted and unarchived.
The desktop cannot open the immutable artifact directly; its action would rerun
prediction. The recorded exact-once constraint therefore requires an explicit
user decision before real-result GUI acceptance.
