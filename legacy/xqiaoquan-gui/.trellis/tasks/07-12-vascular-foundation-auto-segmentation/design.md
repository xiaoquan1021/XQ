# Design: 自动三维分割

## Pipeline

```text
image + vesselness + profile
  -> automatic intensity/vesselness statistics
  -> high-confidence core mask (ITK threshold)
  -> low-threshold candidate mask (ITK threshold)
  -> ITK morphology
  -> ITK connected components + relabel/statistics
  -> XQ branch-aware component policy retaining candidates connected to core
  -> optional ITK level-set refinement from the automatic mask
  -> validated XQSegmentationMask
```

The XQ policy chooses among component records; it does not implement voxel connectivity. Parameters are stored in `AutomaticVesselSegmentationProfileV1` with an immutable fingerprint.

## Gold Firewall

Production code has no gold dependency. The evaluator loads prediction and reference mask separately and verifies image/profile/prediction/metric fingerprints before scoring.

## Migration

Keep current primitive service only until the E2E cutover is complete. Mark it legacy/non-production in code ownership docs; no new feature is added to it. The final UI removes it from the normal workflow.
