# Paper-method correction after real-machine failure

Recorded: 2026-07-13

> Superseded for production by
> `geodesic-active-contour-v3-correction.md`. This document preserves the
> intermediate TubeTK decision and must be read only as negative-history
> evidence after the fixed v2.1 applicability failure.

## Decision

The current automatic-v2 segmentation chain is rejected. It is replaced by the
complete ITK-TubeTK 1.3.5 `tube::SegmentTubes` ridge-traversal workflow backed by
Aylward and Bullitt (2002), followed by TubeTK's own radius-aware tube
rasterizer. This is a method replacement, not a parameter adjustment.

Primary method:

- S. R. Aylward and E. Bullitt, "Initialization, noise, singularities, and
  scale in height ridge traversal for tubular object centerline extraction",
  IEEE Transactions on Medical Imaging, 21(2), 61-75, 2002.
- DOI: `10.1109/42.993126`.
- Author implementation: ITK-TubeTK `1.3.5`, Apache-2.0.

## Real-machine failure evidence

The fixed case-1 prediction visibly formed a large liver/abdominal foreground
mass instead of a vessel tree. The separate evaluator reported:

| Measure | Actual |
| --- | ---: |
| prediction foreground | 1,726,625 voxels |
| reference foreground | 103,533 voxels |
| Dice | 0.0656456983 |
| precision | 0.0347909940 |
| recall | 0.5802111404 |
| HD95 | 73.2234344 mm |
| ASSD | 14.6386223 mm |
| clDice | 0.0376997249 |

The prediction artifact was closed before evaluation. These values document the
failure and are not inputs to the corrected method or its parameters.

## Root cause

The rejected chain used all 42,406 coarse-ROI foreground voxels as one
`itk::ConfidenceConnectedImageFilter` seed population, then accepted every
strictly positive vesselness response (`minimumVesselnessExclusive = 0.0`) in
the organ-or-dilated-coarse domain and retained the root-connected result with
binary reconstruction.

Using the complete coarse mask as one confidence population produces broad CT
statistics. The zero vesselness floor makes nearly the entire positive Frangi
response eligible. Reconstruction therefore retains one large connected liver
candidate. This chain was assembled locally from generic filters; it is not a
complete adopted paper method. The defect cannot be corrected responsibly by
tuning its multiplier, threshold, dilation, or component policy.

## Adopted upstream workflow

The corrected product path is:

```text
CT + existing Frangi vesselness + aligned organ/coarse-vessel ROI
  -> ITK physical resampling to an isotropic working grid
  -> upstream-derived TubeTK input image and ROI probability seed mask
  -> TubeTK SegmentTubes dynamic-scale ridge traversal
  -> TubeTK radius estimation and tube spatial objects
  -> TubeTK ConvertTubesToImage with UseRadius=true
  -> nearest-neighbor ITK physical resampling to the original CT grid
  -> XQ-owned mask, provenance, persistence and caller result
```

The aligned coarse-vessel ROI is seed evidence, not final foreground. It is
resampled through ITK physical coordinates. XQ does not select endpoints,
traverse voxels, reconnect components, union the result with the rejected
baseline, or add a metric-triggered fallback.

The upstream CTA example establishes this profile:

| TubeTK input | Value | Authority |
| --- | ---: | --- |
| `MinCurvature` | 0 | official CTA notebook |
| `MinRoundness` | 0.02 | official CTA notebook |
| `MinRidgeness` | 0.5 | official CTA notebook |
| `MinLevelness` | 0 | official CTA notebook |
| `RadiusInObjectSpace` | 0.8 mm | official CTA notebook |
| `BorderInIndexSpace` | 3 | official CTA notebook |
| `OptimizeRadius` | true | official CTA notebook |
| `UseSeedMaskAsProbabilities` | true | official CTA notebook |
| `SeedExtractionMinimumProbability` | 0.4 | official CTA notebook |

The official CTA notebook does not set the separate `*Start` ridge thresholds
or `DynamicScale`; those values remain the ITK-TubeTK 1.3.5 upstream defaults.
The adapter sets only the values shown above and does not set `SeedRadiusMask`,
which is commented out in the notebook.

The official notebook first masks and smooths the CTA input with the enhanced
vessel image. For seeds it inverts the coarse vessel mask, computes a
Danielsson distance image, blurs that distance image by `0.4 mm`, and zeros
values outside `0.1..10 mm`; the resulting blurred distance values are passed
as the probability seed mask. XQ must express those operations with official
ITK filters and the already available Frangi/TotalSegmentator evidence; no
manual voxel algorithm is permitted.

## Source and dependency boundary

- Product ITK remains exactly `5.4.0` and the product C++ standard/CRT remain
  C++17 and dynamic MSVC CRT.
- Consume the fixed ITK-TubeTK `1.3.5` source archive recorded in
  `dependency-source-lock.md`.
- Vendor only the upstream source/include files needed by `SegmentTubes` and
  `ConvertTubesToImage`, preserving Apache-2.0 notices and provenance.
- Compile that source against the product ITK component closures. Do not link
  `D:/XQ/research/itk-tubetk-install`, because it belongs to a different ITK
  build.
- No Python, wrapped ITK, TotalSegmentator runtime, MinimalPathExtraction,
  research prefix, or second ITK/VTK enters the product build or runtime.
- TotalSegmentator 2.15.0 remains an offline ROI data generator only.

## Rejected alternatives

- The current ConfidenceConnected/positive-vesselness/reconstruction chain.
- Threshold or profile grid search against case-1 gold.
- The prior TubeTK probe's 12/local seed policy and union with the old baseline.
- Automatic endpoint selection or MinimalPath wrapping.
- XQ-owned reconnection, weighted component ranking, or voxel traversal.

## Verification gate

1. Structural tests prove the production path records TubeTK extraction and
   rasterization stages and records no ConfidenceConnected/reconstruction stage.
2. Synthetic tests prove physical isotropic resampling, ROI seed use,
   radius-aware rasterization, CT-grid back-mapping, cancellation/failure
   behavior and deterministic XQ materialization.
3. The affected Release targets, focused tests, full Release CTest and runtime
   dependency audit must pass.
4. Case 1 may then be run once through the corrected fixed profile and the
   separate evaluator. No metric-driven second run is allowed.
5. Held-out case 5 remains closed. The task remains `in_progress` until the user
   accepts the visible real-machine output.

## Local upstream references

- `D:/XQ/research/ITKTubeTK-1.3.5/Readme.md`
- `D:/XQ/research/ITKTubeTK-1.3.5/include/tubeSegmentTubes.h`
- `D:/XQ/research/ITKTubeTK-1.3.5/src/Segmentation/itktubeTubeExtractor.hxx`
- `D:/XQ/research/ITKTubeTK-1.3.5/include/tubeConvertTubesToImage.h`
- `D:/XQ/research/ITKTubeTK-1.3.5/examples/CTA-Head/3-SegmentEnhancedVessels.ipynb`
- `D:/XQ/research/ITKTubeTK-XQ-probe/TubeTkCase1Probe.cpp` (negative evidence
  only; its seed selection and baseline union are not reusable product logic)
