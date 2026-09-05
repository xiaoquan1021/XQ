# Frozen Vascular Validation Baseline v1

Frozen: 2026-07-12, before any final downstream prediction on the held-out set.

This file is the metric contract for the ITK preprocessing, automatic
segmentation, centerline/radius/tree, bridge, meshing, and real-data E2E tasks.
Thresholds must not be lowered after inspecting final outputs. A changed
contract requires a new version and invalidates prior held-out claims.

## Dataset split

3D-IRCADb-01 case IDs are frozen as follows:

- Development/tuning: `1,2,3,4,6,7,8,9,10`
- Validation/profile selection: `11,12,13,14,15`
- Held-out: `5,16,17,18,19,20`

Only case 5 is fully acquired and accepted today. Unavailable or not-yet-audited
cases remain assigned to their split; they may not be silently deleted or moved.
Until the remaining cases pass the same source/license/hash/alignment checks,
case 5 is an honest single-case held-out engineering gate, not a population
efficacy claim.

Primary target: `portalvein`. Secondary separately reported target:
`venoussystem`. They must not be merged for the primary score.

## Input and gold separation

Production receives only:

```text
derived CT directory + versioned parameter profile
```

It must have no gold path, mask handle, gold-derived seed, ROI, threshold,
Path, Contour, centerline, or mesh input. The gold directory is unavailable to
the production process. The prediction is closed and SHA-256 hashed before the
evaluator opens gold.

The evaluator records:

- CT series fingerprint and decoded buffer hash;
- prediction hash;
- gold DICOM series fingerprint and XQ binary-mask hash;
- this baseline's SHA-256;
- algorithm/profile version and profile hash.

## Label and space rules

- Stored label values are exactly background `0`, foreground `255`.
- Evaluator maps foreground to XQ label `1`; any third value is a hard failure.
- Geometry must equal the CT in dimensions, spacing, origin, direction, and
  FrameOfReferenceUID after documented frame-v1 normalization.
- No resampling is used for case 5. Future resampling must use nearest-neighbor
  labels and be frozen in a new baseline before prediction.
- Distances use physical LPS millimetres.

## Segmentation metrics and thresholds

For each primary held-out case:

- Dice coefficient: `>= 0.70`.
- 95th-percentile symmetric Hausdorff distance (HD95): `<= 3.2 mm`.
- Average symmetric surface distance (ASSD): `<= 1.2 mm`.
- topology-aware clDice: `>= 0.75`.
- largest 26-connected predicted component: `>= 90%` of predicted foreground.

Surface distances use voxel-face surfaces in physical space. Empty prediction
or empty gold is a hard failure, not a zero/ignored metric.

## Centerline and topology metrics

Gold and prediction centerlines are extracted with the same frozen 3D thinning
and physical distance-transform evaluator, never used by production.

- Gold centerline recall within `1.6 mm`: `>= 0.80`.
- Predicted centerline precision within `1.6 mm`: `>= 0.75`.
- Largest connected graph component length fraction after pruning branches
  shorter than `3.2 mm`: `>= 0.95`.
- Zero zero-length edges, duplicate consecutive points, non-finite points, or
  radius values.

IRCAD masks do not provide stable branch IDs. Exact named-branch and endpoint
F1 are reported as unsupported rather than invented. Endpoint count and branch
count are reported descriptively after the frozen 3.2 mm pruning rule.

## Radius metrics

Gold radius is the physical Euclidean distance-transform value at matched gold
centerline points. Prediction radius is compared only where centerlines match
within 1.6 mm.

- Median absolute radius error: `<= 1.0 mm`.
- 95th-percentile absolute radius error: `<= 2.5 mm`.
- Matched centerline coverage must meet the centerline recall threshold above.

## Surface and volume mesh metrics

The predicted surface is compared to the prediction mask, not to gold before
the prediction is frozen.

- Closed surface: zero boundary edges and zero non-manifold edges.
- Zero self-intersections, degenerate triangles, NaN, or infinite coordinates.
- Surface-to-prediction-boundary HD95: `<= 1.6 mm`.
- Volume mesh: zero inverted/non-positive-volume tetrahedra and zero elements
  outside the closed surface.
- Scaled Jacobian: minimum `> 0`, 5th percentile `>= 0.10`, median `>= 0.40`.
- Radius-edge ratio: 95th percentile `<= 3.0`.

## Failure and reporting rules

- Source/hash/license/alignment/profile mismatch is a hard case failure.
- Missing output, crash, timeout, empty result, or evaluator error is a failed
  case and remains in the denominator.
- Any manual correction or case-specific parameter change converts that run to
  tuning; it cannot be called held-out.
- Case 5 must satisfy every applicable threshold for the first real-shell gate.
- When all six held-out cases are available, every hard validity rule must pass,
  at least five of six cases must meet all numeric segmentation/centerline/radius
  thresholds, and all individual failures must remain listed.
- Automated results are evidence only. Final shell completion remains subject
  to the user's physical-machine functional review.
