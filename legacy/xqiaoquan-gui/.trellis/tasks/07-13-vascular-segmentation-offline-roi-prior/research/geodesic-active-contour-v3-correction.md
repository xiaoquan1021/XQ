# Geodesic active-contour v3 correction

Recorded: 2026-07-13

## Applicability decision

The TubeTK 2.1 production attempt is rejected as an applicability error, not
as a parameter error. Aylward-Bullitt ridge traversal extracts seeded
centerlines and radii; the cited CTA-Head example does not establish an
automatic abdominal portal-vein segmentation method. Its fixed case-1 result
recovered only `25,307 / 103,533` reference voxels (Dice `0.3804991731`, recall
`0.2444341418`). It remains readable evidence in artifact v5 and is never a
v3 fallback, union input or tuning baseline.

The earlier ConfidenceConnected/reconstruction composition is likewise
rejected. It produced a liver/abdominal mass (Dice `0.0656456983`, precision
`0.0347909940`).

## Authority

The v3 method follows the family explicitly requested by `D:/XQ`:

1. Frangi et al. 1998 and Sato et al. 1998 authorize multiscale tubular
   enhancement; the existing ITK 5.4 preprocessor is retained unchanged.
2. Caselles, Kimmel and Sapiro, "Geodesic Active Contours", IJCV 22(1),
   61-79, 1997, authorizes the edge-potential surface evolution.
3. Official ITK 5.4 source and
   `Examples/Segmentation/GeodesicActiveContourImageFilter.cxx` define the
   executable filter convention: an initial level-set image, an edge-potential
   feature image, unit curvature/advection weights, RMS termination and
   thresholding of values at or below the zero level set.
4. SimVascular's `sv4guiImageProcessingUtils::geodesicLevelSet` proves the
   adopted ITK filter is used in a real vascular-imaging integration. Its UI
   supplies propagation `0.7` and a 100-iteration cap. ITK semantics win where
   integration defaults differ.

The `D:/XQ/参考文献/论文与资源清单.md` Rouchdy-Besson 2007 entry names an image
registration paper. It is not used as vessel-segmentation authority.

## Fixed v3 composition

```text
aligned liver + coarse-vessel roles
  -> ITK physical coarse-mask dilation
  -> organ OR dilated coarse allowed domain
  -> ITK SignedMaurerDistanceMap(coarse, inside negative, spacing on)
  -> ITK rescale vesselness [0,1]
  -> ITK GradientMagnitudeRecursiveGaussian(sigma 1.0 mm)
  -> ITK BoundedReciprocal: g = 1 / (1 + |gradient|)
  -> ITK mask g outside the allowed domain to zero
  -> ITK GeodesicActiveContourLevelSet
  -> ITK BinaryThreshold(levelSet <= 0)
  -> ITK And with allowed domain
  -> ITK connected components/relabel for reporting only
  -> XQ-owned mask and artifact v6
```

The coarse role initializes evolution but is not copied into the output. The
organ and coarse roles may be disjoint after physical alignment; therefore the
domain is `organ OR dilate(coarse)`, never `organ AND coarse`.

## Frozen initial profile

```text
coarse_domain_dilation_mm = 2.0
edge_sigma_mm              = 1.0
propagation_scaling        = 0.7
advection_scaling          = 1.0
curvature_scaling          = 1.0
maximum_rms_error          = 0.02
maximum_iterations         = 100
isosurface_value           = 0.0
use_image_spacing          = true
```

Identity is
`xq.itk.geodesic-active-contour-vessel-segmentation` `3.0.0`, profile schema
3 and artifact version 6. No environment override or case label may change
these values.

## Hard boundary

- No TubeTK target in the current product link graph; vendored source may
  remain unused for legacy provenance.
- No parameter grid, gold-derived adjustment, repeated case-1 run or case 5.
- No topology separation, reconnection, MinimalPath, centerline or component
  selection in this task.
- Case 1 may be predicted and independently evaluated exactly once only after
  all synthetic, Release, Flow-ON/OFF, dependency and diff checks pass.
