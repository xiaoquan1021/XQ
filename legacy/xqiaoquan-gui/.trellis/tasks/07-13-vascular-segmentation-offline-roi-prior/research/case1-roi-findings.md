# Case 1 offline ROI findings

Recorded: 2026-07-13

## Data generation

Both runs used TotalSegmentator 2.15.0, nnU-Net 2.8.1, torch
2.13.0+cu126 and the GPU device. Only `liver` and
`portal_vein_and_splenic_vein` were requested. Python and model weights remain
under `D:\XQ\research` and are not part of the XQ product runtime.

### Fast 3 mm model

- output directory: `totalsegmentator-2.15.0-fast`
- runtime: 137.69 seconds
- liver SHA-256:
  `65e54fa96f1a24a615eebad092f54a1612814335226b3e7fc7427afad22e90de`
- coarse-vessel SHA-256:
  `ed34968fc1dfd7baa1376859350c52920c4e1072b49ac4f062145abee4882249`
- aligned foreground: liver 2,859,919; coarse vessel 36,063

### Standard 1.5 mm model

- output directory: `totalsegmentator-2.15.0-standard`
- first-run runtime including official weight downloads: 771.5 seconds
- liver SHA-256:
  `9d5c547b654e134f47da841cf0721f807e4b4cb71e3ee70a0dce5059ae0f534e`
- coarse-vessel SHA-256:
  `8df49b24f3dcc10a3654b8307cb97edec4ac73c2659f21aaa4486924956129c1`
- aligned foreground: liver 2,846,877; coarse vessel 42,406
- XQ prior fingerprint:
  `xq-vascular-roi-prior-v1:sha256:24ea1106a911e0571847bb36f0e561b3fe60ad557f8d88fe0a9f82a9ecc601bc`

The C++ reader returned `ok`, zero diagnostics and foreground-count-preserving
nearest-neighbor physical remapping for both standard masks.

## Critical label-composition finding

For both model resolutions, aligned `liver` and
`portal_vein_and_splenic_vein` have exactly zero direct voxel overlap. The first
probe incorrectly built `gatedCoarse = response AND intensity AND organ AND
coarse`, which erased the entire coarse marker. The corrected physical domain
is `organ OR dilate(coarse, 2 mm)`; CT/vesselness gates are applied after that
composition.

## Development metrics

Frozen v1 baseline:

| Dice | HD95 mm | ASSD mm | clDice |
| ---: | ---: | ---: | ---: |
| 0.774009725 | 19.1801262 | 2.23462363 | 0.723033273 |

Corrected gate-10 reconstruction:

| ROI source | Dice | HD95 mm | ASSD mm | clDice | Recall |
| --- | ---: | ---: | ---: | ---: | ---: |
| fast 3 mm | 0.771138762 | 15.2850199 | 1.88699957 | 0.722825750 | 0.728791786 |
| standard 1.5 mm | 0.772200930 | 14.8538103 | 1.84626580 | 0.726943029 | 0.729776979 |

The standard model is a real improvement over fast, but neither reconstruction
meets HD95 `<=3.2 mm`, ASSD `<=1.2 mm` or clDice `>=0.75`.

A valid signed-distance ITK threshold level-set used the gate-10 reconstruction
as initial surface, 80 iterations, RMS limit 0.001 and curvature 0.25. With the
standard ROI it produced Dice 0.773004114, HD95 18.057909 mm, ASSD 2.11319366 mm
and clDice 0.716543355. It is rejected. An earlier binary-seed attempt had the
level-set sign reversed and selected almost the entire ROI; it is invalid
evidence, not an algorithm result.

## Why the next branch changes

Case-1 gold distribution shows that 10% of reference voxels are below 116 HU
and 25% have vesselness below 4.71295. The v1 `120 HU` and gate-10 candidate
therefore excludes a material fraction of peripheral branches before topology
processing. The next development branch is a bounded ROI-guided ITK hysteresis
or region-growing grid with lower CT/vesselness thresholds, followed by
connected-component/relabel policy. It must be selected on case 1 and frozen
before case 5 remains eligible to open.

## Dedicated liver-vessels and combined-prior rejection

The TotalSegmentator 2.15.0 `liver_vessels` task produced a separate aligned
mask with SHA-256
`854bd82b4b2df9566e3e35f575c7cb5098e29bfd0287c94d45cafbe11dc8d5` and 28,707
foreground voxels. Its raw result contained 52 components and reached only
Dice `0.289972777`, recall `0.185187332`, HD95 `91.1561127 mm` and clDice
`0.337727431` against the development portal-vein label. It is not a usable
standalone prior for this target.

OR-composing standard `portal_vein_and_splenic_vein` with `liver_vessels`
changed the baseline-marker reconstruction by at most 43 voxels in the loose
grid and did not change any selected full-metric result. The combined raw mask
had 53 components, Dice `0.533042060`, HD95 `77.1376953 mm` and clDice
`0.431777761`.

A bounded multi-marker branch then used only CT/vesselness-validated prior
voxels as remote markers, ITK binary reconstruction as the grow kernel, and ITK
fully-connected components/relabel before XQ component selection. It did not
copy either prior into the output. Nevertheless the best Dice candidate was:

| Dice | HD95 mm | ASSD mm | clDice | components | largest fraction |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 0.714577831 | 19.1433029 | 3.08877805 | 0.673058065 | 12 | 0.923512901 |

The highest-recall candidate that kept Dice above 0.70 reached recall
`0.746641168`, but HD95 `20.200325 mm`, ASSD `3.36506494 mm` and clDice
`0.645851384`. The remote markers admitted liver-vessel components that are not
portal branches; minimum physical component volume did not separate them.

Decision: stop TotalSegmentator prior variants and do not keep multi-marker
reconstruction as a fallback. Continue with baseline-rooted peripheral-branch
recovery: ITK components/relabel expose candidate additions, XQ scores branch
support, and an Apache-2.0 ITK MinimalPathExtraction kernel may bridge only
selected additions through the ROI speed image. The separate TubeTK-built ITK
prefix under `D:\XQ\research` must not be linked into the product; reusable
module source must compile against the locked product ITK 5.4 prefix.

## Superseding reuse-first decision

The continuation proposed above was executed only as a development probe. A
fixed-topology variant reached Dice `0.783577`, HD95 `11.8323 mm`, ASSD
`1.56306 mm` and clDice `0.771635`. A stricter MinimalPath bridge reached Dice
`0.786043`, HD95 `11.182456 mm`, ASSD `1.478094 mm` and clDice `0.787423`.

That branch is rejected for production even though it improved the metrics:

- Mueller's MinimalPath method requires explicit start/end/optional way-points;
  the probe's automatic remote-component and endpoint selection was XQ-invented
  policy, not the cited upstream method.
- The branch added weighted component rules and case-driven thresholds outside
  the shell's reuse boundary.
- It still missed the frozen surface lines, so further tuning would only extend
  an unsupported local algorithm.

Decision: stop all threshold grids, component-weight searches and automatic
MinimalPath experiments. The development analyzer and its local MinimalPath
vendoring are removed after preserving these findings. Production v2 may only
compose the paper-backed upstream ITK filters named in `design.md`; MinimalPath
is reserved for a future explicit-point semi-automatic adapter if requested.
