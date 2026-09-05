# Case 1 automatic segmentation findings

Recorded: 2026-07-12

## Scope

This note records development-case evidence for the current gold-free production
contract. It is not a held-out result and it does not close the task.

Production input:

```text
CT DICOM + portal-venous preprocess profile + automatic segmentation profile
```

Gold is opened only by the separate evaluator after the prediction artifact has
been written and hashed. Held-out case 5 remains unopened by the predictor.

## Current production chain

```text
XQ Float32 CT + XQ-owned ITK vesselness
  -> automatic anatomical seed selection
  -> ITK ConfidenceConnected
  -> ITK BinaryDilate with a physical-mm radius
  -> ITK vesselness and intensity gating
  -> optional ITK close/open
  -> ITK fully-connected components + relabel
  -> XQ component scoring and retention
  -> XQSegmentationMask + lineage + v4 artifact
```

The production call graph does not use `SegmentationService::thresholdMask`,
`SegmentationService::regionGrowMask`, or
`SegmentationService::keepLargestConnectedComponent`.

## Frozen default profile used by the best artifact

- `confidenceMultiplier = 2.0`
- `confidenceIterations = 3`
- `confidenceInitialNeighborhoodRadiusVoxels = 1`
- `confidenceExpansionRadiusMm = 0.8`
- candidate vesselness floor: `0.003 * maximum`, combined with the profile
  quantile rule
- core vesselness floor: `0.08 * maximum`, combined with the profile quantile
  rule
- hard CT window: `120..300`
- close/open radius: `0 mm`

Artifact:

```text
D:\XQ\data\vascular\3D-IRCADb-01\reports\case1-dev-auto-seg-v4-20260712.xqvmask
SHA-256 2e5867d71a0281f46f7f7e75327d35032921989d412ac5288a42dfbfcb27d168
algorithm xq.itk.automatic-vessel-segmentation 1.1.0
profile SHA-256 a6e2bb62581504d0c786a482b8ff512cd72ef5cc68c8a7ebb7bbb47c96d6b3f2
```

Observed production provenance:

- automatic seed: `(313,216,33)`, inside development gold
- ConfidenceConnected: 53,843 voxels, mean `196.2281438218653`, variance
  `165.48982725621624`
- physical dilation: voxel radii `(2,2,1)` for `0.8 mm`
- retained output: one 26-connected component, 90,201 voxels
- ITK threshold, ConfidenceConnected, dilation/morphology, connected component
  and relabel stages all executed

## Case 1 evaluator result

Frozen baseline SHA-256:
`53f3f92771570405e59379c7a5a7ab24cc6e7c5e80d781d9f9a426f6c8268047`.

| Metric | Result | Frozen line | Status |
| --- | ---: | ---: | --- |
| Dice | 0.774009725 | >= 0.70 | pass |
| HD95 | 19.1801 mm | <= 3.2 mm | fail |
| ASSD | 2.23462 mm | <= 1.2 mm | fail |
| clDice | 0.723033 | >= 0.75 | fail |
| largest component fraction | 1.0 | >= 0.90 | pass |

The overlap is acceptable, but peripheral branch recovery and boundary placement
do not meet the frozen contract. This is a real algorithm limitation, not a test
failure to hide.

## Routes already rejected on the development case

| Route | Observed failure |
| --- | --- |
| ConfidenceConnected `2.0 / 3` without physical expansion | Dice about `0.66493`; insufficient peripheral recall |
| Slightly larger confidence multiplier | abrupt leakage rather than controlled branch recovery |
| Multiple ConfidenceConnected seeds from the initial mask | large false-positive growth |
| Lowering the score for additional components | added components contributed no new gold intersection |
| NeighborhoodConnected grid | either remained near the seed or expanded to hundreds of thousands/millions of voxels |
| Binary reconstruction by dilation | recall rose only with prediction size above one million voxels |
| Fixed normalized-ROI reconstruction | the best `vesselness >= 20` candidate improved Dice only to `0.780377` and recall to `0.742807`; HD95 `17.2222 mm`, ASSD `1.88515 mm` and clDice `0.741620` still failed the frozen lines, so the probe was removed |
| Expansion radius `3 mm` | Dice `0.7128`, clDice `0.5427`; worse topology, reverted |
| Threshold level-set from the gold-free initial mask | contracted or expanded into the wrong anatomy without an external organ/coarse-vessel ROI |
| Vesselness-speed ITK FastMarching from the ConfidenceConnected front | conservative settings did not beat the baseline; settings that raised recall caused large false-positive growth |

These branches were development-only probes. They are not production fallbacks
and are removed from the maintained analyzer after this note is recorded.

Directional evaluator evidence confirms the missing-branch diagnosis:

- voxel precision `0.831210297`, voxel recall `0.724174901`
- prediction-to-reference surface P95 `4.61374 mm`
- reference-to-prediction surface P95 `19.1801 mm`

For the FastMarching probe, `t10/g20` added only 133 reference voxels and
reduced Dice to `0.772305`; `t100/g1.5` raised recall to `0.762356` but expanded
the prediction to 313,343 voxels and reduced Dice to `0.378669`. This is not a
controlled peripheral-branch recovery mechanism.

A final development-only ITK binary-reconstruction probe bounded candidates by
a fixed normalized box derived from the automatic seed-search profile. At the
only precision-preserving gate (`vesselness >= 20`), prediction grew from
90,201 to 93,564 voxels and intersection from 74,976 to 76,905. Precision was
`0.821951` and recall `0.742807`, but directional surface P95 remained
`5.69755 / 17.2222 mm`, HD95 `17.2222 mm`, ASSD `1.88515 mm`, and clDice
`0.741620`. The small gain does not satisfy the frozen contract, and the fixed
normalized box is a coarse ROI that cannot be hidden inside v1. The environment
grid and evaluator-only probe were removed rather than retained as a production
fallback.

## Contract conclusion

`D:\XQ` describes peripheral portal-branch recovery as a coarse vessel/organ ROI
plus vesselness-gated level-set workflow. The frozen v1 production contract and
baseline explicitly prohibit ROI input. Therefore ROI cannot be slipped into
this task to improve the number.

The remaining permitted route is to explicitly create a versioned ROI-capable
offline child and a new baseline contract before any held-out run. TubeTK was
also audited and executed as described below; its initial case-1 extraction is
not suitable for the maintained v1 production chain.

## ITK-TubeTK build and development-case audit

The official Apache-2.0 sources were obtained and fingerprinted:

- TubeTK v1.3.5 archive:
  `D:\XQ\research\ITKTubeTK-1.3.5.tar.gz`, SHA-256
  `26972916eb332275106a6af47cae3182b0d035eca329ec56069d630a24747a4c`.
- ITKMinimalPathExtraction commit `35dd8e83...` archive:
  `D:\XQ\research\ITKMinimalPathExtraction-35dd8e83.tar.gz`, SHA-256
  `a2edcca4bc07175487be34e0a6c1b780cc176a67e6f9a1de20c21d55911d4fb4`.

An isolated MSVC `/MD` build against ITK 5.4 completed with
`TubeTK_USE_VTK=OFF` and `ITK_WRAP_PYTHON=OFF`. The install at
`D:\XQ\research\itk-tubetk-install` contains the real TubeTK CMake package,
`itkTubeTK-5.4.lib`, and `itkMinimalPathExtraction-5.4.dll`.

The focused probe at `D:\XQ\research\ITKTubeTK-XQ-probe` reuses XQ's DICOM
reader, ITK preprocessor, automatic seed domain, maintained production
baseline, and independent evaluator. TubeTK ridge extraction does not honor
anisotropic spacing, so the valid probe explicitly resamples CT and vesselness
to a `1.0 mm` isotropic LPS grid and nearest-neighbor maps the rasterized tube
mask back to the original reference geometry before scoring.

With 12 automatic seeds, both CT and vesselness input modes produced nine
tubes. Their unions with the maintained baseline were:

| TubeTK input/union | Dice | HD95 mm | ASSD mm | clDice | Outcome |
| --- | ---: | ---: | ---: | ---: | --- |
| CT, raw | 0.738265144 | 23.4706993 | 3.23070688 | 0.701525184 | worse |
| CT, vesselness/intensity gated | 0.767430591 | 17.3554268 | 2.50497759 | 0.692780158 | worse topology |
| vesselness, raw | 0.677825881 | 42.1885719 | 5.36857384 | 0.676177471 | worse |
| vesselness, vesselness/intensity gated | 0.743821304 | 16.5880127 | 2.74893099 | 0.688695437 | 59 components; worse topology |

The vesselness-gated result contained 102,538 predicted voxels, 76,640 true
positive voxels, precision `0.747430221`, recall `0.740247071`, and 59 connected
components. Although its HD95 was lower than the maintained baseline, Dice,
ASSD and clDice regressed and fragmentation increased sharply. The extracted
ridges add false branches instead of controlled peripheral portal branches.

TubeTK is therefore a technically reusable, successfully built dependency, but
this case-1 ridge-extraction configuration is algorithmically rejected for v1.
It is not linked into the production adapter and is not a fallback path.

Until one route satisfies the frozen metrics, this task remains `in_progress`
and held-out case 5 stays closed.
