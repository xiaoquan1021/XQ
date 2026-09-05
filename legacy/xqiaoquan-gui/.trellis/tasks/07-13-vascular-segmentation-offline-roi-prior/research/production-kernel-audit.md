# Production call graph and custom-kernel replacement audit

Recorded: 2026-07-13

## Current product paths

### Desktop segmentation stage

```text
XQStageWidgets::buildSegmentationPage
  -> SegmentationController::prepareThreshold
     -> SegmentationService::thresholdMask
     -> SegmentationService::keepLargestConnectedComponent
  OR
  -> SegmentationController::prepareRegionGrow
     -> SegmentationService::regionGrowMask
  -> SegmentationService::createMaskNodeCommand
  -> XQCommandStack
```

This is an old product path. `SegmentationService` implements scalar traversal,
6-connected flood fill and largest-component extraction in XQ code. The page
also exposes the old threshold/seed controls. All of these must leave the
desktop product path when ROI v2 is wired.

### Command-line production predictor

```text
xq_vascular_segment_predict
  -> GdcmItkDicomSeriesReader
  -> ItkVascularPreprocessor
  -> ItkAutomaticVesselSegmenter v1
  -> writeAutomaticVesselSegmentationArtifact v4
```

The v1 adapter already invokes ITK filters, but it still owns algorithm policy
that is prohibited in v2:

| Existing operation | Current owner | v2 disposition |
| --- | --- | --- |
| histogram/quantile vesselness thresholds | XQ helper loops | delete from product path; use the versioned profile and ITK `BinaryThresholdImageFilter` |
| intensity distribution bounds | XQ helper loops | delete from product path |
| normalized-anchor seed search and distance score | XQ voxel scan | delete; aligned coarse-vessel evidence becomes the TubeTK probability seed mask through ITK image filters |
| confidence region grow | ITK `ConfidenceConnectedImageFilter` | rejected after real-machine over-segmentation; remove from v2 |
| coarse/organ ROI composition | absent from v1 | reproduce upstream CTA-style input/seed preparation with named ITK filters |
| root connectivity | v1 confidence expansion plus later XQ policy | replace with complete TubeTK dynamic-scale height-ridge traversal |
| tube radius/output volume | absent from v1 | TubeTK radius optimization plus `ConvertTubesToImage`, `UseRadius=true` |
| connected-component labeling | ITK `ConnectedComponentImageFilter` | reporting-only after the TubeTK output is closed; cannot alter selection |
| component accumulators, elongation, boundary/core scores, relative-score sorting and retention limit | XQ loops and policy | delete from product path; reconstruction output is final, with no XQ component selection |
| output materialization/fingerprints/typed failures | XQ boundary code | retain; these are XQ-owned adapter responsibilities |

### Separate evaluator

The evaluator is not a predictor dependency. It opens the closed prediction
artifact before gold and uses ITK connected components, distance maps and the
locked thinning backend only to report metrics. It remains separate and cannot
select v2 parameters.

## Superseding v3 replacement call graph

```text
Desktop automatic segmentation action OR xq_vascular_segment_predict
  -> GDCM/ITK DICOM reader (or already resident XQ image)
  -> ItkVascularPreprocessor
  -> ItkVascularRoiPriorReader(organ NIfTI, coarse-vessel NIfTI)
  -> ItkAutomaticVesselSegmenter v3
       -> ITK physical coarse-mask dilation and ROI domain composition
       -> ITK SignedMaurer coarse-mask initial surface
       -> ITK rescale + recursive-Gaussian gradient + bounded reciprocal
       -> ITK GeodesicActiveContourLevelSet on the CT LPS/mm grid
       -> ITK zero-level threshold and hard ROI-domain intersection
       -> ITK connected components (reporting only, no selection)
  -> XQ mask + v6 artifact / XQ scene command
```

The CLI and desktop action must call the same schema-3 public interface. Neither may
fall back to `SegmentationService::thresholdMask`, `regionGrowMask`,
`keepLargestConnectedComponent`, the rejected ConfidenceConnected chain, the v1
automatic adapter, TubeTK, research prefixes or MinimalPath. The earlier v2
table above is retained only to explain what was removed; its TubeTK
dispositions are superseded by `geodesic-active-contour-v3-correction.md`.

## Explicitly outside this replacement

`ContourExtractionService` and the cross-section threshold/region-grow tools
create editable two-dimensional path contours. They are not the three-dimensional
automatic vessel-mask product entry covered by this task. They remain separate;
their existence is not a fallback from ROI v2.
