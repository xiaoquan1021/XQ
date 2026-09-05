# Candidate and Metric Plan

## Candidate A: 3D-IRCADb-01

Reason for priority:

- planning documents identify it as a portal/hepatic vascular validation candidate;
- contrast-enhanced liver CT and vessel labels fit the intended first vascular geometry vertical slice;
- a single anatomical site keeps initial traditional segmentation parameters and topology evaluation bounded.

Facts still requiring official verification before acceptance:

- official current download endpoint and access conditions;
- exact license/terms and redistribution limits;
- actual package version and hashes;
- DICOM/label formats and label names;
- whether portal/hepatic veins are separate and sufficiently detailed;
- exact image-label physical transform;
- whether all 20 cases are usable and how exclusions are documented.

## Fallback research rule

Research at least one additional public enhanced CT/CTA vessel segmentation dataset with voxel labels. A fallback must satisfy the same hard gates; it is not enough to be easier to download.

## Metric freeze sequence

1. Inspect actual label semantics and voxel spacing.
2. Decide tune/validation split and immutable case IDs.
3. Define resampling policy, if any, before predictions.
4. Define segmentation, boundary, connectivity, centerline, radius and topology metrics.
5. Set numeric thresholds from label resolution, literature baseline and downstream geometry needs.
6. Hash the baseline document.
7. Only then run final held-out predictions.

## Anti-leak checks

- Production command has no gold path argument.
- Gold directory is unavailable/moved during production run.
- Prediction file is closed and hashed before evaluator starts.
- Evaluator records prediction/gold/baseline hashes.
- Any case-specific parameter change invalidates the frozen run and is reported as tuning, not held-out evaluation.
