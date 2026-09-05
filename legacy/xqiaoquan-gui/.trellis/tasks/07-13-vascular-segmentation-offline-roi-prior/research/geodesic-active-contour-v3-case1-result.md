# Geodesic active-contour v3 case-1 result

Recorded: 2026-07-13

## Status

The schema-3 implementation and automated gates are green. The frozen case-1
prediction and independent evaluation each ran exactly once. The prediction is
valid and operable, but it fails the frozen quantitative lines because it
under-segments the portal-vein tree. No profile or algorithm value was changed
after evaluation, and no second run was made.

The Trellis task remains `in_progress`, uncommitted and unarchived for user
visual acceptance.

## Automated gate before case access

Canonical evidence directory:

```text
.trellis/workspace/ocean/shell-a-canonical-logs/20260713-231428
```

- Flow-ON fresh Release clean build: `353/353`.
- Flow-ON Release CTest: `100/100`.
- Flow-OFF fresh Release clean build: `331/331`.
- Flow-OFF Release CTest: `92/92`.
- Both builds: generated build graph clean; 95 PE binaries scanned.
- Explicit-package negative probes: 7/7 passed.
- Focused schema-3 checks: 4/4 passed.
- `git diff --check`: passed.
- Production source search: no TubeTK target/call and no old automatic
  threshold/region-grow/largest-component call.

## Frozen inputs

```text
CT:
D:/XQ/data/vascular/3D-IRCADb-01/derived/frame-v1/3Dircadb1.1/patient/PATIENT_DICOM

organ ROI:
D:/XQ/data/vascular/3D-IRCADb-01/derived/offline-roi-v1/3Dircadb1.1/totalsegmentator-2.15.0-standard/liver.nii.gz

coarse-vessel ROI:
D:/XQ/data/vascular/3D-IRCADb-01/derived/offline-roi-v1/3Dircadb1.1/totalsegmentator-2.15.0-standard/portal_vein_and_splenic_vein.nii.gz

case-1 gold, evaluator only:
D:/XQ/data/vascular/3D-IRCADb-01/derived/frame-v1/3Dircadb1.1/masks/MASKS_DICOM/portalvein
```

- CT slices: 129.
- Gold slices: 129.
- liver SHA-256:
  `9d5c547b654e134f47da841cf0721f807e4b4cb71e3ee70a0dce5059ae0f534e`.
- coarse-vessel SHA-256:
  `8df49b24f3dcc10a3654b8307cb97edec4ac73c2659f21aaa4486924956129c1`.
- Frozen task PRD/evaluator baseline SHA-256:
  `10983dd1f475ccc9e9ac96d2d837254cce3449cdb9f7548fd47b12a389e2649f`.
- `D:/XQ` remained read-only. The result was written under the ignored
  canonical Flow-ON build tree.
- Held-out case 5 was not used by prediction or evaluation.

## One production prediction

Artifact:

```text
XQ/build_shell_a_on/case1-geodesic-active-contour-v3-fixed.xqvmask
```

- invocation count: 1.
- process exit: 0.
- artifact bytes: `101453809`.
- artifact version: 6.
- artifact SHA-256:
  `ae578e03ac97a6f5eac5ae7a885e89bc91236eecffab14b2309f3de9aad362ca`.
- reopen integrity: true.
- algorithm:
  `xq.itk.geodesic-active-contour-vessel-segmentation` `3.0.0`.
- ITK: `5.4.0`.
- input fingerprint:
  `xq-vascular-input-v1:sha256:cc7577325310c74443fd8e9e6e2140490708d20ab256fe7c6cdb8bdf90d92781`.
- vesselness fingerprint:
  `xq-vesselness-output-v1:sha256:92d2c28bd0169d450b1818ebc763b0dff00a3c709afbda464eee6936a5015b8f`.
- ROI-prior fingerprint:
  `xq-vascular-roi-prior-v1:sha256:c818af5db517ffe7e5b790f2332a2813a3d101dc6e7a14b48fe184a18ce20586`.
- profile fingerprint:
  `xq-automatic-vessel-profile-v3:sha256:4517f5e028c97c18f9b09972c39eb719fb98bcf04866b9b6f6eb5422f98864e8`.
- output fingerprint:
  `xq-automatic-vessel-mask-v3:sha256:a46bcb06b7cd13caebc8cd68063aae5e388cf01243b4e850987c76dfb62a9cfb`.

Observed execution:

| Measure | Actual |
| --- | ---: |
| preprocessing elapsed | 31,102.2914 ms |
| segmentation elapsed | 6,878.84 ms |
| allowed-domain voxels | 2,926,162 |
| initial-surface voxels | 42,406 |
| positive edge-potential voxels | 2,926,162 |
| edge-potential range | 0.909377277..0.999999523 |
| level-set iterations | 6 |
| final RMS change | 0.0158153631 |
| converged | true |
| output foreground voxels | 35,532 |
| output components | 2 |

The artifact records the expected twelve upstream stages from physical coarse
dilation through reporting-only relabeling.

## One independent evaluation

- invocation count: 1.
- process exit: 0.
- evaluator opened and hashed artifact v6 before `gold.open_begin=true`.
- gold foreground: 103,533 voxels.
- gold frame UID matched prediction:
  `2.25.19169018961501444279444379442458358927`.

| Metric | Actual | Frozen line | Result |
| --- | ---: | ---: | --- |
| Dice | 0.484205228 | >= 0.70 | fail |
| precision | 0.947540245 | report | recorded |
| recall | 0.325191002 | report | recorded |
| prediction-to-reference P95 | 1.59999847 mm | report | recorded |
| reference-to-prediction P95 | 77.3510742 mm | report | recorded |
| HD95 | 77.3510742 mm | <= 3.2 mm | fail |
| ASSD | 20.9378990 mm | <= 1.2 mm | fail |
| clDice | 0.304502753 | >= 0.75 | fail |
| components | 2 | report | recorded |
| largest-component fraction | 0.992260498 | >= 0.90 | pass |

Topology precision is `1.0`, topology sensitivity is `0.179594956`, and
reference coverage is poor. Compared with the rejected methods, v3 avoids the
v2.0 abdominal mass and raises precision above the TubeTK v2.1 result, but the
large reference-to-prediction distance and low recall demonstrate severe
under-segmentation.

## Frozen conclusion

This result is not authority to tune the propagation weight, edge sigma,
iteration cap, ROI dilation, preprocessing profile, or component policy. It is
also not authority to add TubeTK, a baseline union, topology separation,
reconnection, MinimalPath, centerline or mesh work to this task.

Any further algorithmic response requires a new planning decision with
paper/upstream authority. This task only retains the v3 implementation,
automated evidence, the single immutable case-1 artifact, and pending visual
acceptance.

## Manual-acceptance reachability

Post-run source inspection found no desktop/project caller of
`readAutomaticVesselSegmentationArtifact`. The artifact reader is currently
used only by the predictor reopen check, inspector and evaluator. The desktop
`Automatic Segmentation` action always reruns preprocessing, ROI import and
the schema-3 segmenter before committing a Scene mask.

Consequently, the immutable one-run case-1 artifact cannot currently be opened
in the GUI for visual judgment. Pressing the desktop action with the same case
would be a second case-1 prediction and would violate this task's exact-once
constraint. Resolving that conflict requires an explicit scope decision:

1. authorize one additional GUI acceptance run without treating it as tuning;
   or
2. authorize a separately planned artifact-to-Scene import/view path.

Neither action is taken in this task without user direction.
