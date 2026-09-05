# Design: reuse-first portal-vein segmentation v4

## Design rule

```text
paper/upstream method -> locked official implementation -> private XQ adapter
                      -> explicit XQ portal strategy -> XQ-owned result
```

External libraries own kernels. XQ owns the missing portal-vein strategy that
`D:/XQ` explicitly says must be assembled and tuned in the project.

## Reuse matrix

| Capability | Reused implementation | XQ-owned work |
| --- | --- | --- |
| DICOM decode | GDCM 3.0.10 + ITK IOGDCM | result, diagnostics, fingerprint |
| NIfTI alignment | ITK reader/resampler | roles, lineage, validation |
| denoising | ITK curvature anisotropic diffusion | schema-1 preprocessing profile |
| vessel enhancement | ITK multiscale Hessian objectness (Frangi/Sato) | materialization and lineage |
| root segmentation | ITK threshold/ConfidenceConnected/morphology/reconstruction | seed/core and physical profile |
| branch labeling | ITK ConnectedComponent + RelabelComponent | component feature policy |
| skeleton support | locked ITKThickness3D + ITK distance map | endpoint/tangent/topology policy |
| optional reconnect | ITKMinimalPathExtraction locked commit | endpoint selection and speed-image profile |
| optional local refine | official ITK level-set filters | where/when to apply and fixed parameters |
| offline prior | TotalSegmentator 2.15.0 output files | import contract; never product runtime |

## Production data flow

```text
GDCM/ITK DICOM -> XQImageVolume + IVoxelSource
  -> ITK diffusion -> Frangi/Sato vesselness
  -> ITK-align liver + coarse-vessel ROI
  -> root mask
       ITK intensity/vesselness core
       -> ITK ConfidenceConnected / binary reconstruction
       -> ITK physical morphology
  -> peripheral candidates
       lower vesselness/intensity inside physical ROI
       -> ITK connected components + relabel
       -> XQ component/topology policy
  -> optional branch reconnect
       locked ITK thinning/distance support
       -> XQ explicit endpoint selection
       -> locked ITKMinimalPathExtraction over physical speed image
       -> ITK reconstruction/domain enforcement
  -> optional local ITK level-set refinement
  -> validated XQ mask + provenance -> artifact v7 -> CLI/desktop
```

## Recovered baseline

`VascularRoiDevelopmentAnalyze.cpp` is restored only as a development/evidence
tool. It consumes an explicitly supplied legacy v1 root artifact, then runs the
frozen candidate/topology and MinimalPath stages. Gold is used for printed
development metrics/audits only. It is not linked into or called by production.

The recovered fixed profile is:

```text
root candidate vesselness       >= 5
root candidate CT               >= 120 HU
component voxels                30..700
maximum root-contact fraction   0.40
maximum endpoint distance       12 mm
maximum secondary-prior fraction 0.25

MinimalPath candidate voxels    3..120
maximum endpoint distance       22.5 mm
minimum endpoint alignment      0.70
maximum elongation              2.0
maximum mean/peak vesselness    7.8 / 12.5
mean CT range                   140..175 HU
maximum paths                   12
```

These values reproduce the old result but are not assumed to pass the surface
gate. Further case-1 changes require a written hypothesis and a new explicit
profile revision.

## Component and topology boundary

ITK produces labels, component sizes, morphology, reconstruction and distance
images. XQ may iterate over those materialized values to compute portal-specific
features such as root contact, ROI support, endpoint distance/alignment and
anatomical exclusion. It may rank/select components because this is the
project-owned core strategy identified by `D:/XQ`; it may not implement label
propagation or connectivity traversal itself.

MinimalPath receives an explicit start, end and physical speed image. Upstream
performs Fast Marching/path optimization. XQ's automatic endpoint choice is a
versioned policy with its own provenance. This distinction prevents claiming
that upstream supplied an automatic portal-vein segmenter.

## Development and gold firewall

```text
production predictor: CT + ROI + profile -> close/hash artifact
development evaluator: closed artifact + gold -> metrics/audit
```

Case 1 is development data, so controlled iterations are allowed. The
production executable has no gold path or evaluator link that can influence
the mask. The development analyzer may print gold-derived diagnostics, but
selection used by a candidate production profile must be recomputed and proved
identical with reference access removed.

Case 5 remains outside both the development analyzer and normal test data. It
is opened only after the schema-4 profile is frozen.

## Version and replacement semantics

- New behavior uses profile schema 4, algorithm 4.0.0 and artifact v7.
- v3 GAC remains preserved as failed evidence/read compatibility, not fallback.
- v2/TubeTK and v1 legacy artifacts remain readable, not fallback or union.
- The normal CLI and desktop route call only the schema-4 adapter after cutover.
- The independent 2D contour workbench remains manual and outside this route.

## Failure and ownership

All upstream pipelines are fully updated before XQ materializes values.
Cancellation and exceptions discard root/candidate/path partial state. A
successful return owns all buffers and records exact geometry, fingerprints,
profile, component decisions, path endpoints and upstream versions without
retaining ITK/NIfTI object lifetime.
