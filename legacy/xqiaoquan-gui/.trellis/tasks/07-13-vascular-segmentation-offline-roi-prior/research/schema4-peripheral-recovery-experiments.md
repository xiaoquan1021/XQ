# Schema-4 peripheral-recovery experiments

Recorded: 2026-07-13

## Rules

- Case 1 is the only development case. Case 5 remains unopened.
- Run one written hypothesis at a time. Do not run an unrecorded grid.
- Gold is available only to this development analyzer and the independent
  evaluator. A promoted mask must be reproduced with selection logic that does
  not read reference values.
- Compilation, deterministic reproduction and improved Dice are insufficient:
  acceptance still requires Dice `>=0.70`, HD95 `<=3.2 mm`, ASSD `<=1.2 mm`,
  clDice `>=0.75` and largest-component fraction `>=0.90`.

## Recovered baseline

| Variant | Dice | recall | HD95 mm | ASSD mm | clDice |
| --- | ---: | ---: | ---: | ---: | ---: |
| legacy root | 0.774009725 | 0.724174901 | 19.1801262 | 2.23462363 | 0.723033273 |
| fixed g5/i120 topology | 0.783577002 | 0.749326302 | 11.8323212 | 1.56305641 | 0.771635092 |
| fixed g5/i120 plus MinimalPath | 0.786042999 | 0.753943187 | 11.1824560 | 1.47809405 | 0.787422523 |

The g5/i120 candidate itself reaches only about `0.754657` recall. The current
MinimalPath result already reaches `0.753943`, so selecting more components from
that same candidate cannot recover the missing reference surface.

## Experiment 1: lower target domain, unchanged trusted root and path policy

Status: rejected after valid run.

Hypothesis:

```text
fixed g5/i120 trusted root and fixed topology
+ candidate target domain g3/i120 inside the same physical ROI
+ unchanged locked MinimalPath kernel, endpoint policy and path support profile
```

Only one production-relevant value changes: the remote candidate vesselness
minimum changes from `5.0` to `3.0`. Root construction, root component policy,
CT range `120..300 HU`, ROI domain, endpoint thresholds, component limits,
speed image, path limit, path-tube radius and loose reconstruction support stay
unchanged.

Reason: `D:/XQ` requires ROI-constrained recovery of peripheral portal branches,
and the case-1 reference audit shows a material distal fraction below the g5
candidate threshold. Lowering only the target domain tests candidate coverage
without weakening the trusted root or replacing any ITK/upstream kernel.

Promotion rule: reject unless the full metrics improve without a large false
component and the mask is deterministic. Passing quick Dice/recall alone does
not promote the hypothesis.

### Protocol-invalid attempt

The first invocation accidentally omitted the recovered baseline's optional
case-1 `liver_vessels` secondary ROI. This changed the supposedly fixed topology
before the experiment:

| Variant | Dice | recall | HD95 mm | ASSD mm | clDice |
| --- | ---: | ---: | ---: | ---: | ---: |
| fixed topology without secondary ROI | 0.781259908 | 0.749683676 | 11.7303514 | 1.57763458 | 0.769692783 |
| g3/i120 result without secondary ROI | 0.777634666 | 0.752262564 | 11.5416241 | 1.64472798 | 0.755971793 |

The result used 12 paths, 321 path voxels and one final component, but 11 of
the 12 selected endpoint groups contained zero reference voxels. It is rejected
as a protocol-invalid run because more than the declared candidate threshold
changed. It is not used to alter any policy value.

### Valid result

The original secondary ROI was restored and no code or parameter changed. The
fixed topology reproduced the recovered baseline exactly before the experiment:
Dice `0.783577002`, recall `0.749326302`, HD95 `11.8323212 mm`, ASSD
`1.56305641 mm` and clDice `0.771635092`.

| Variant | Dice | precision | recall | HD95 mm | ASSD mm | clDice | largest fraction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| g3/i120 plus unchanged MinimalPath policy | 0.779920652 | 0.810104584 | 0.751905190 | 11.6767492 | 1.63070750 | 0.757813670 | 1.0 |

FastMarching reached all 3,125 remote components. The unchanged policy found 85
eligible components, selected 37 members in 12 endpoint groups, produced 12
paths/321 path voxels and one final component. Eleven of the twelve selected
groups had zero reference overlap; the only true group contributed 42 reference
voxels.

Decision: reject. Lowering the target-domain threshold does expose more remote
components, but the existing endpoint-group ranking preferentially reconnects
false structures and regresses Dice, ASSD and clDice. Do not tune a single
distance/alignment/geodesic threshold: the diagnostic evidence already shows
those scalar features overlap between true and false branches.

## Experiment 2: local ITK hysteresis around accepted g5 paths

Status: rejected after valid run.

Hypothesis:

```text
fixed g5/i120 trusted root and topology
+ original g5/i120 remote targets and unchanged five-path MinimalPath result
+ ITK local binary reconstruction through existing g2/i80 loose support
   only within 3.2 mm of selected remote components and their accepted bridges
```

The low threshold does not nominate endpoints or components. Selection remains
the recovered g5 policy whose five groups contributed 185/208 reference voxels
in the historical audit. After those paths succeed, construct a focus mask from
the selected remote components plus accepted bridge, dilate that focus by
`3.2 mm` with ITK, intersect it with the already-existing `vesselness >= 2` and
`80..300 HU` loose support inside the physical ROI, then run ITK binary
reconstruction from the bridge-only result.

Only one new profile value is introduced: `localRecoveryRadiusMm = 3.2`. The
loose-support thresholds already belong to the recovered MinimalPath bridge.
The radius equals the frozen surface-tolerance scale and prevents the low gate
from reaching unrelated remote anatomy.

Promotion rule: the bridge-only output must first reproduce Dice `0.786042999`,
HD95 `11.1824560 mm`, ASSD `1.47809405 mm` and clDice `0.787422523`. Reject the
local result unless full metrics improve without losing one-component topology.

### Result

The bridge-only output reproduced the historical result exactly before local
recovery. ITK local reconstruction then added 532 voxels:

| Variant | Dice | precision | recall | HD95 mm | ASSD mm | clDice | largest fraction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| bridge only | 0.786042999 | 0.820997718 | 0.753943187 | 11.1824560 | 1.47809405 | 0.787422523 | 1.0 |
| 3.2 mm local hysteresis | 0.785991905 | 0.818563106 | 0.755913573 | 11.1824560 | 1.47382850 | 0.787964584 | 1.0 |

Decision: reject. The local stage slightly improves recall, ASSD and clDice,
but does not move reference-to-prediction P95/HD95 and slightly regresses Dice.
The remaining failure is distal topology, not a boundary-thickness deficit
around the five already accepted paths.

## Diagnostic A: size-ranked g5 remote-component audit

Status: complete.

Purpose: print a deterministic size-ranked audit of all g5/i120 components that
do not overlap the fixed root. This does not alter the mask, profile or path
selection. It is needed because the existing audit prints only reference-hit or
primary-prior-hit components and therefore hides feature-identical false
components. The audit will decide whether a reference-independent low-alignment
selection rule exists before any Experiment 3 is proposed.

Result: the 30 largest remote components, ranging from 167 to 10,859 voxels,
all have zero reference overlap. Several have high endpoint alignment and low
geodesic ratios despite being false. This confirms the recovered
`maximumVoxels = 120` guard is necessary and that accepting larger components
cannot recover the missing portal topology.

## Diagnostic B: non-directional g5 eligibility audit

Status: complete.

Purpose: evaluate only 3..120-voxel remote components that satisfy every
existing MinimalPath rule except endpoint alignment. Sort them by geodesic cost
ratio and print their full gold-only audit features. This leaves the output
unchanged and tests whether low-alignment true branches can be separated from
false candidates using upstream FastMarching cost and prior support before an
Experiment 3 selection rule is proposed.

Result: 80 non-directional components were printed in ascending FastMarching
geodesic-ratio order. True and false components are interleaved throughout the
ranking. Representative rows include false label 103 at ratio `1.323`, true
label 41 at `1.637` (67/108 reference voxels), true label 96 at `1.730`
(40/44), false label 79 at `1.789`, true label 119 at `1.864` (20/30), and
false label 130 at `2.080`. Primary- and secondary-prior support are usually
zero for both classes.

Decision: component-local scalar thresholds are exhausted. Geodesic ratio,
endpoint alignment, component size, intensity/vesselness and prior fractions
do not provide a reference-independent separator by themselves. Experiment 3
must use the topology of upstream MinimalPath support across related component
chains or another explicitly authorized ITK path/level-set operation; it must
not introduce another scalar component cutoff disguised as a new policy.

## Experiment 3: upstream MinimalPath corridor-chain recovery

Status: rejected after valid run.

Hypothesis:

```text
fixed g5/i120 trusted root, candidate and non-directional eligibility domain
+ the existing multi-source ITK FastMarching arrival image
+ locked upstream ArrivalFunctionToPathFilter paths from every eligible remote
  target back to the root front
+ XQ path-corridor topology: retain a path only when its existing 1.0 mm
  corridor encounters at least one additional eligible component with a lower
  arrival time on the same upstream route
+ existing g2/i80 loose support and ITK binary reconstruction
```

This reverses the rejected selection order. Endpoint alignment and the local
weighted score no longer nominate a component before path extraction. The
locked upstream module first supplies the physical route. XQ then materializes
a directed component chain from that route, which is the portal-topology work
assigned to the project by `D:/XQ`. A target plus an upstream component is the
minimum graph edge, not a new scalar ranking threshold; isolated candidates
remain rejected.

For each path, the analyzer records the upstream root endpoint ordinal, target
and centroid physical location, ordered corridor-member labels, path vertices,
and loose-support continuity. Reference intersections are printed in separate
audit fields only. Chain construction must be recomputed without reading those
fields. If multiple targets describe the same chain, retain only the maximal
path and use label/order tie-breaks for deterministic output.

No existing numeric domain changes in this experiment: candidate
vesselness/intensity remains `g5/i120..300`, loose support remains
`g2/i80..300`, path-tube radius remains `1.0 mm`, and the locked speed image and
MinimalPath optimizer settings remain unchanged. The rejected 3.2 mm local
hysteresis result is not part of the output.

Promotion rule: first reproduce the bridge-only baseline exactly. Then reject
the corridor-chain result unless all full metrics are deterministic, largest
component fraction remains at least `0.90`, and the frozen Dice/HD95/ASSD/clDice
gates all pass. Gold-derived audit fields may explain a rejection but may not
alter chain membership, path choice or output.

### Result

The bridge-only output again reproduced the recovered baseline exactly before
the new result was evaluated:

| Variant | Dice | precision | recall | HD95 mm | ASSD mm | clDice | largest fraction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| bridge only | 0.786042999 | 0.820997718 | 0.753943187 | 11.1824560 | 1.47809405 | 0.787422523 | 1.0 |
| corridor chain | 0.776157261 | 0.803382052 | 0.750717163 | 11.4073706 | 1.71444439 | 0.746060731 | 1.0 |

The path-topology policy selected 12 maximal paths and 30 remote components.
Relative to the fixed-topology root it added only 144 net true voxels while
adding 2,120 false voxels. Representative selected chains on root routes 92
(`219:372:417`), 62 (`472:702`) and 64 (`409:465`) contained zero reference
voxels despite having multiple upstream corridor members. The output remained
one component, so the regression is semantic false continuation rather than a
connectivity failure.

Decision: reject. Multi-fragment path continuity alone does not distinguish a
portal branch from another continuous vessel or liver-edge structure. Do not
tune member count, path count or unsupported-run thresholds on this result.
The next hypothesis must add an independent portal-topology/anatomical
constraint or use a different `D:/XQ`-authorized ITK refinement operation; it
cannot be a scalar retuning of corridor-chain evidence.

## Diagnostic C: true-route versus selected-false-route audit

Status: complete.

Purpose: rerun the unchanged Experiment-3 analyzer while retaining only
reference-hit non-directional/corridor rows, selected corridor rows, recovered
five-path groups and final metrics. Compare upstream root-route ordinal,
physical target/centroid location, path length, centerline loose-support gaps
and component-chain membership. This diagnostic does not alter a mask or
selection rule. It decides whether the next documented experiment has evidence
for an ITK Hessian/orientation-backed path constraint or whether the remaining
error instead requires an explicitly bounded official ITK level-set stage.

Result: at least 10 of the 12 selected maximal chains had zero reference
voxels. The one retained true-bearing example visible in the complete filtered
audit was route 92, labels `205:308:345`, with 25 reference voxels. True remote
components occurred on many different routes (including 32, 35, 49, 56, 57,
73, 79, 86 and 92), and both true and false paths exhibited short and long
loose-support gaps. Route identity, member count and centerline support do not
provide an independent separator. Most selected false chains had larger
arrival/path length, but Diagnostic B already shows true and false candidates
interleave in scalar path-cost order, so an arrival cutoff is not promoted.

Decision: do not add a Hessian/path score or tune a maximum path length from
this audit. Return to the explicit `D:/XQ` composition: the offline prior gives
seed/ROI evidence and an official ITK vesselness-gated level set performs
bounded peripheral refinement. First localize the remaining >3.2 mm reference
error so that any level-set domain is stated before an output-changing run.

## Diagnostic D: distant-reference component localization

Status: complete.

Purpose: use ITK SignedMaurerDistanceMap on the exactly reproduced bridge-only
prediction, threshold reference voxels farther than `3.2 mm`, then run ITK
ConnectedComponent/RelabelComponent. For each distant reference component,
print voxel count, LPS centroid/bounds, CT and vesselness summaries, organ/
coarse/secondary/allowed-ROI fractions and existing g2/i80 loose-support
fraction. This is evaluator-only evidence and does not alter prediction.

The diagnostic decides whether an Experiment-4 local level-set can operate in
the already authorized ROI/support domain. If the dominant distant reference
surface lies outside that domain, the ROI contract must be corrected first;
the level set must not be tuned to cross an unstated anatomical boundary.

Result: the exactly reproduced bridge-only prediction has 48,393 reference
surface voxels, of which 5,705 (11.7889%) are farther than `3.2 mm`. Only 1,722
of those distant surface voxels (30.1840%) are in the current allowed ROI, and
only 1,004 (17.5986%) also satisfy the existing g2/i80 loose support.

The three largest distant components contain 2,116, 780 and 678 surface voxels
(3,574 combined). Their allowed-ROI fractions are respectively 0, 0.0308 and
0, even though their mean vesselness values are 11.24, 8.27 and 5.64. The
largest component reaches 25.67 mm from the prediction and the third reaches
38.43 mm. These are not boundary-thickness errors and they cannot be recovered
by any stage hard-clipped to the current domain.

Decision: the current `raw liver OR dilate(coarse, 2 mm)` mask is an invalid
hard exclusion boundary for the frozen full portal-vein reference. `D:/XQ`
assigns the offline label as seed/ROI evidence, not anatomical ground truth;
the schema-4 search domain must be derived physically with ITK and persisted.
Do not run a level set until that domain is corrected.

## Diagnostic E: physical prior-distance distribution

Status: complete.

Purpose: on the same distant reference surface, compute official ITK signed
Maurer distances to the liver prior and to the combined coarse-vessel priors.
Record distribution quantiles and per-component distances without changing the
prediction. This determines one explicit physical search-domain hypothesis;
it avoids guessing an organ-dilation radius or running an output parameter
grid.

Result:

| Distance from distant reference surface | p50 mm | p75 mm | p90 mm | p95 mm | maximum mm |
| --- | ---: | ---: | ---: | ---: | ---: |
| raw liver prior | 24.3653 | 72.6948 | 83.3757 | 87.2556 | 114.2863 |
| combined coarse-vessel priors | 19.1942 | 41.2640 | 69.4866 | 80.2877 | 89.9861 |

The largest missing component spans `5.49..89.99 mm` from the combined prior
while retaining mean vesselness `11.24`; it is a long connected vessel branch,
not a compact hole around the liver label. A fixed 80-90 mm Euclidean dilation
would admit a large abdominal volume and would not be an honest ROI.

Decision: do not repair the domain with a large organ/prior dilation. Use the
raw priors as seed/topology evidence and let an official ITK vesselness-gated
level set propagate geodesically through a CT-constrained feature domain. The
offline labels must not remain a hard output clip.

## Experiment 4: bridge-seeded ITK threshold level set without raw-ROI clipping

Status: rejected after valid run.

Hypothesis:

```text
exactly reproduced bridge-only MinimalPath mask
-> ITK SignedMaurerDistanceMap initial surface (inside negative, spacing on)
-> vesselness feature masked by ITK CT threshold 80..300 HU
-> ITK ThresholdSegmentationLevelSetImageFilter, vesselness 2..maximum
-> preserve the bridge-only seed by ITK OR
-> ITK ConnectedComponent/RelabelComponent reporting
```

This is the concrete `D:/XQ` instruction "offline prior gives seed/ROI;
vesselness-gated level set recovers peripheral branches". It reuses the prior
development level-set implementation and its already stated fixed values:
propagation `1.0`, curvature `0.25`, edge weight `0`, smoothing iterations `0`,
maximum RMS error `0.001`, maximum iterations `80`, isosurface `0`, and image
spacing enabled. It does not introduce a parameter grid. The only method
correction is replacing the disproved raw-liver hard clip with the existing
CT/vesselness physical feature domain.

Gold, distant-component labels and distance quantiles are absent from the
level-set inputs and output construction. Promotion requires deterministic
output and every frozen Dice/HD95/ASSD/clDice/connectivity line; otherwise the
experiment is rejected rather than retuned in place.

### Result

The ITK level set ran all 80 iterations and ended at RMS `0.00398355687`; its
output remained one component.

| Variant | Dice | precision | recall | HD95 mm | ASSD mm | clDice | largest fraction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| bridge only | 0.786042999 | 0.820997718 | 0.753943187 | 11.1824560 | 1.47809405 | 0.787422523 | 1.0 |
| bridge-seeded threshold level set | 0.786308510 | 0.799329435 | 0.773705002 | 10.9866343 | 1.48658393 | 0.764722308 | 1.0 |

Decision: reject. Removing the invalid raw-liver clip is necessary and raises
recall by about two percentage points, but propagation from the bridge-only
surface mostly expands local foreground and does not reach the dominant long
missing branches. HD95 and ASSD remain outside the frozen lines. Do not tune
the iteration cap or curvature in place.

## Experiment 5: gated coarse-prior seeded ITK threshold level set

Status: rejected after valid run.

Hypothesis: retain every Experiment-4 feature-domain and level-set value, but
also initialize the same official ITK level set from primary
`portal_vein_and_splenic_vein` prior voxels that independently satisfy the
existing `vesselness >= 2` and `80..300 HU` gates. The secondary
`liver_vessels` prior is not a seed because its standalone portal precision is
insufficient.

The initial surface is `bridge-only OR gated-primary-prior`. After evolution,
ITK binary reconstruction uses bridge-only as the marker and
`bridge-only OR evolved-inside` as the mask. Therefore a disconnected coarse
prior or a disconnected level-set island cannot enter output, and the prior is
not copied wholesale. This is the direct `D:/XQ` reuse order: offline prior
gives seed evidence, ITK vesselness-gated level set refines it, and ITK
connectivity enforces attachment to the trusted portal tree.

Only seed evidence changes from Experiment 4. Promotion again requires every
frozen quantitative line and deterministic one-component output; otherwise
the result is rejected without parameter retuning.

### Result

The bridge-only and Experiment-4 results first reproduced their historical
metrics exactly. This also proves that extracting the shared ITK level-set
execution from a local lambda into a named probe function did not change the
algorithm or its output. The gated primary prior supplied 35,803 seed voxels;
the official ITK threshold level set ran all 80 iterations and ended at RMS
`0.00394985676` with one reconstructed output component.

| Variant | Dice | precision | recall | HD95 mm | ASSD mm | clDice | largest fraction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| bridge-seeded threshold level set | 0.786308510 | 0.799329435 | 0.773705002 | 10.9866343 | 1.48658393 | 0.764722308 | 1.0 |
| gated-primary-seeded threshold level set | 0.785864457 | 0.797642260 | 0.774429409 | 10.9866343 | 1.48942725 | 0.760586621 | 1.0 |

Decision: reject. The extra prior seed raises recall by only `0.000724407`,
does not move either the reference-to-prediction P95 or HD95, and regresses
Dice, ASSD and clDice. Disconnected coarse-prior surfaces do not establish the
missing long portal connections under the unchanged feature domain. Do not
tune the iteration count, curvature or add the secondary prior as another seed;
the next hypothesis must reuse a different paper/upstream operation that
represents elongated branch continuation rather than another local surface
expansion.

## Experiment 6: unclipped ITK hysteresis reconstruction

Status: rejected after valid run.

Hypothesis:

```text
exactly reproduced bridge-only MinimalPath mask as marker
+ vesselness >= 2 AND CT 80..300 HU over the CT physical frame as mask
-> ITK BinaryReconstructionByDilation, fully connected
-> ITK ConnectedComponent/RelabelComponent reporting
```

This reuses the region-growing/threshold branch explicitly selected by
`D:/XQ`. It tests elongated low-threshold continuation rather than another
local surface expansion. The bridge-only mask is the sole marker; the offline
prior has already served seed/topology selection upstream and is neither
copied nor used as a hard output boundary. The physical CT/vesselness feature
domain replaces the disproved `liver OR dilate(coarse, 2 mm)` clip.

No numeric value changes from Experiments 2/4: loose vesselness remains `2`,
CT remains `80..300 HU`, and the bridge-only root/path result is unchanged.
Gold and distant-reference diagnostics are absent from construction. Reject
unless the deterministic full result passes every frozen metric and largest
component line; do not retune either loose threshold from this run.

### Result

The bridge-only and Experiment-4 metrics again reproduced exactly before this
result was evaluated. ITK reconstruction added 1,658,351 voxels and retained a
single connected component.

| Dice | precision | recall | prediction-to-reference P95 mm | reference-to-prediction P95 mm | HD95 mm | ASSD mm | clDice | largest fraction |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0.0979449757 | 0.0518641199 | 0.878367284 | 58.5538063 | 1.27455807 | 58.5538063 | 26.1755753 | 0.0282823133 | 1.0 |

Decision: reject. The unclipped loose support does reach the missing reference
tree, as shown by the reference-to-prediction P95 falling below `1.3 mm`, but
ordinary connectivity also reaches a 1.65-million-voxel abdominal/liver
foreground mass. The remaining problem is portal-specific discrimination, not
candidate visibility or a local gap. Do not tune the loose scalar thresholds
or keep this result as a union/fallback. Before proposing another output run,
audit whether the existing SimVascular/old-project vascular level-set code
provides an upstream shape/edge term that can replace the generic threshold
level set without introducing an XQ-written evolution kernel.

## Diagnostic F: existing SimVascular vascular level-set applicability

Status: complete; rejected before an output run.

The 12 files under `XQ/third_party/sv_levelset` are byte-identical to the local
SimVascular source under `Externals/src/SimVascular/Code/Source/sv3/
ITKSegmentation`. They are genuine BSD-licensed upstream work, already wrapped
by `ItkVascularSegmenter`; no kernel would need to be rewritten.

However, the authoritative upstream caller `sv3_LevelSetContour.cxx` applies
the two phases to `m_VtkImageSlice`, creates one circular seed, disables image
spacing and returns one two-dimensional path contour. XQ's existing adapter
likewise uses `itk::Image<float, 2>` and a section image in pixel coordinates.
The earlier foundation audit explicitly classifies it as a 2D cross-section
tool, not a 3D automatic vessel-tree segmenter.

Decision: retain and reuse this code only for its existing 2D contour workflow.
Do not claim its filter templates establish a validated 3D portal-tree method,
do not run it slice-by-slice, and do not replace the current 3D experiment with
an unsupported extrapolation of the upstream work.

## Experiment 7: unclipped trusted-gate ITK reconstruction

Status: rejected after valid run.

Hypothesis:

```text
exactly reproduced bridge-only MinimalPath mask as marker
+ existing trusted gate vesselness >= 5 AND CT 120..300 HU over the CT frame
-> ITK BinaryReconstructionByDilation, fully connected
-> ITK ConnectedComponent/RelabelComponent reporting
```

Only the disproved raw `liver OR dilate(coarse, 2 mm)` hard clip is removed
from the already frozen trusted root gate. No threshold changes: `g5/i120` is
the exact fixed-topology/root candidate used by the recovered baseline.
Diagnostic D found mean vesselness `11.24`, `8.27` and `5.64` in the three
largest distant-reference components, while Experiment 6 proved `g2/i80`
admits a massive connected false domain. This run tests whether the established
high-confidence feature domain retains those elongated branches without the
loose-domain leakage.

Gold and component audit fields do not enter reconstruction. Reject unless all
frozen metrics and connectivity pass; do not follow with a `g3/g4` or CT grid.

### Result

The bridge-only and Experiment-4 outputs first reproduced their historical
metrics exactly. The trusted unclipped domain then added `556,705` voxels and
remained one connected component.

| Dice | precision | recall | prediction-to-reference P95 mm | reference-to-prediction P95 mm | HD95 mm | ASSD mm | clDice | largest fraction |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0.220866791 | 0.127975305 | 0.805656168 | 54.5491066 | 1.6000061 | 54.5491066 | 23.9478545 | 0.0999479086 | 1.0 |

Decision: reject. Raising the reconstruction domain from the loose
`g2/i80` support to the already established trusted `g5/i120` support removes
about two thirds of the leaked foreground, but still admits more than half a
million false connected voxels. The reference-to-prediction P95 improves to
`1.6000061 mm`, confirming again that the missing true tree is visible in the
feature domain, while the prediction-to-reference P95 and all shape metrics
show that scalar connectivity cannot distinguish it from other continuous
enhanced anatomy. Do not try `g3/g4`, another CT threshold, or retain any part
of this mask. The next output-changing hypothesis must add an independent
portal-specific topology/anatomical discriminator and must first audit the
remaining reusable upstream TubeTK operations rather than writing another
voxel traversal or scalar gate.

## Diagnostic G: remaining TubeTK and Sato reuse applicability

Status: complete.

The remaining C++ operations in the locked `D:/XQ/research/ITKTubeTK-1.3.5`
source were checked against the failure exposed by Experiments 6 and 7:

- `tube::SegmentConnectedComponents` is a thin wrapper around ITK
  `ConnectedComponentImageFilter`; its optional seed mask retains already
  connected labels and supplies no independent vessel discriminator.
- `itk::tube::MinimumSpanningTreeVesselConnectivityFilter` consumes an
  already extracted set of `TubeSpatialObject`s and only assigns tube-tree
  connectivity from endpoint distance/radius and continuity angle. It cannot
  recover voxels absent from the rejected TubeTK extraction, whose fixed
  case-1 recall was `0.2444341418`.
- `itk::tube::SegmentTubeUsingMinimalPathFilter` wraps the same locked
  MinimalPathExtraction family already used by the bridge-only baseline and
  still requires explicit endpoints and a speed image. It does not solve the
  portal-versus-false-target decision rejected by Diagnostics B and C.
- `TubeEnhancingDiffusion2DImageFilter` materializes only `Dxx`, `Dxy` and
  `Dyy` and implements the cited two-dimensional method. It is not a valid
  three-dimensional portal-tree replacement. The separate hybrid diffusion
  filter is a direction-sensitive denoiser, not a portal classifier.

These are genuine reusable upstream implementations, but none supplies the
missing 3D candidate discriminator. Do not link TubeTK merely to rename an
existing ITK component/path operation.

The other implementation explicitly named by `D:/XQ` has not yet been
executed in this task: Sato et al. 1998's 3D multiscale line measure, provided
directly by ITK 5.4 as `Hessian3DToVesselnessMeasureImageFilter`. Unlike the
current generic Frangi-style objectness scalar, the Sato filter explicitly
uses the three Hessian eigenvalues to reject bright plate/blob shapes while
preserving bright line structures. It is therefore an independent upstream
shape test appropriate for the liver-edge leakage seen in Experiments 6/7.

## Experiment 8: official ITK Sato/Frangi consensus reconstruction

Status: rejected after valid run.

Hypothesis:

```text
CT
-> the existing profile's ITK curvature anisotropic diffusion
-> ITK 5.4 multiscale Hessian + official Sato 3D line measure

exactly reproduced bridge-only MinimalPath mask as marker
+ existing trusted Frangi gate vesselness >= 5 AND CT 120..300 HU
+ Sato line response > 0
-> ITK BinaryReconstructionByDilation, fully connected
-> ITK ConnectedComponent/RelabelComponent reporting
```

This is not another scalar-threshold search. Frangi and CT keep their already
established values. The Sato stage uses the existing preprocessing profile's
diffusion settings and logarithmic `0.6..4.0 mm`, six-scale range, plus the ITK
upstream defaults `Alpha1=0.5` and `Alpha2=2.0`. The only Sato predicate is the
paper/filter sign condition for a bright line response (`>0`); no response
cutoff is fitted to reference data.

Gold and distant-reference components are absent from construction. First
reproduce bridge-only and Experiment 4 exactly. Reject unless every frozen
metric and the connectivity line pass; do not follow this run with a Sato
threshold or alpha grid.

### Result

The official Sato stage used ITK defaults `Alpha1=0.5`, `Alpha2=2.0` and the
existing six logarithmic scales over `0.6..4.0 mm`. It produced
`27,190,439` positive-response voxels with maximum response `335.097107`.
Bridge-only and Experiment 4 first reproduced exactly, and the process exited
zero.

Every voxel in the root-connected trusted Frangi `g5/i120` domain also had a
positive Sato line response. The consensus reconstruction therefore added the
same `556,705` voxels as Experiment 7 and produced an identical mask:

| Dice | precision | recall | prediction-to-reference P95 mm | reference-to-prediction P95 mm | HD95 mm | ASSD mm | clDice | largest fraction |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0.220866791 | 0.127975305 | 0.805656168 | 54.5491066 | 1.6000061 | 54.5491066 | 23.9478545 | 0.0999479086 | 1.0 |

Decision: reject. The paper-defined sign test distinguishes bright line
responses from non-line responses in the full CT, but it does not distinguish
portal vessels from the already Frangi-positive connected anatomy. Do not tune
Sato response thresholds or `Alpha1`/`Alpha2`: that would turn this result into
an unrecorded parameter search without adding portal-specific evidence. The
actual Sato implementation remains reusable preprocessing evidence, but it is
not a replacement for the missing anatomical/topology policy.

## Experiment 9: locked TubeTK ridge traversal from bridge-only endpoints

Status: planned; no output has been accepted.

Hypothesis:

```text
exactly reproduced bridge-only mask
-> locked ITKThickness3D skeleton and the existing XQ endpoint policy
-> ITK clips endpoint seeds to the persisted physical ROI
-> locked ITK-TubeTK 1.3.5 ridge traversal from those explicit seeds
-> locked TubeTK radius-aware rasterization
-> ITK nearest-neighbor back-map and physical-ROI clipping
-> ITK OR with the unchanged bridge-only mask
-> ITK ConnectedComponent/RelabelComponent reporting
```

This run tests the directly reusable TubeTK C++ work named by `D:/XQ`; it does
not repeat the rejected full v2.1 mask and does not write a local ridge walker.
The only method change from the archived v2.1 implementation is the seed
source. The old distance-from-coarse-prior seed mask is replaced by explicit
endpoints of the current bridge-only tree, which the upstream
`tube::SegmentTubes` implementation accepts through
`SetSeedsInObjectSpaceList` before `ProcessSeeds`.

The recovered v2.1 input preparation and extraction values remain fixed:

```text
high-resolution isotropic spacing = minimum CT spacing (0.57 mm on case 1)
input blur sigma                  = 0.4 mm
input window                      = 0.5..300 -> 0..300
vesselness mask                   = 0..1000
minimum curvature                = 0
minimum roundness                 = 0.02
minimum ridgeness                 = 0.5
minimum levelness                 = 0
initial radius                    = 0.8 mm
border                            = 3 voxels
optimize radius                   = true
dynamic scale                     = TubeTK upstream default true
rasterize with radius             = true
```

The source is the byte-locked vendored subset of ITK-TubeTK release 1.3.5;
the archive identity remains SHA-256
`26972916EB332275106A6AF47CAE3182B0D035ECA329EC56069D630A24747A4C`.
It is compiled only by a development CMake project against the locked product
ITK 5.4 modules. The production CMake/link/PE graph remains TubeTK-free and no
binary from `D:/XQ/research/itk-tubetk-install` is consumed.

Gold is absent from seed generation, ridge extraction, rasterization, clipping
and union. It is read only by the existing development evaluator after the
candidate mask is closed. Reject unless the run is deterministic and passes
all five frozen lines: Dice `>=0.70`, HD95 `<=3.2 mm`, ASSD `<=1.2 mm`, clDice
`>=0.75` and largest-component fraction `>=0.90`. A failed extraction or a
metric miss ends this TubeTK hypothesis; do not follow it with a TubeTK
parameter or seed-selection grid.

### Result

The bridge-only input first reproduced its historical metrics exactly. Its
locked thinning result contained 4,782 skeleton voxels and 96 endpoints. ITK
physical-ROI clipping retained 59 explicit endpoint seeds. TubeTK extracted 49
ridges containing 39,235 points; radius-aware rasterization produced 122,086
working-grid voxels. Back-mapping and physical-ROI clipping retained 43,200
tube voxels, and union with bridge-only added 17,834 net voxels.

| Variant | Dice | precision | recall | HD95 mm | ASSD mm | clDice | components | largest fraction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| bridge only | 0.786042999 | 0.820997718 | 0.753943187 | 11.1824560 | 1.47809405 | 0.787422523 | 1 | 1.0 |
| TubeTK endpoint union | 0.734656539 | 0.704147514 | 0.767929066 | 18.6713123 | 2.76516586 | 0.708935236 | 7 | 0.990231244 |

The process exited zero and the upstream extraction completed, so this is a
valid negative result rather than a build/runtime failure. Dice and the
largest-component line pass; HD95, ASSD and clDice fail. The extra ridge mask
adds only 1,448 reference voxels while adding 16,386 false voxels, so unfiltered
endpoint traversal is dominated by false continuation.

Decision: reject. Do not alter TubeTK curvature/roundness/ridgeness/levelness,
radius, border, dynamic-scale behavior or seed list in a parameter grid, and do
not retain this mask as a union or fallback. Log:
`XQ/build_tubetk_endpoint_recovery/experiment9-case1-20260714.log`.

## Diagnostic H: fixed-domain TubeTK subset ceiling

Status: complete without another output run.

The Experiment-9 union contains bridge-only plus every ROI-clipped voxel from
all 49 extracted TubeTK ridges. Any policy that retains only a subset of those
ridges produces a prediction set that is a subset of this union. Directed
reference-to-prediction nearest-surface distance is monotone under prediction
set inclusion: deleting prediction voxels cannot make any reference voxel
closer to the prediction.

The all-ridge superset already has reference-to-prediction P95
`10.572999 mm`. Therefore every fixed-domain TubeTK ridge subset has
reference-to-prediction P95 at least `10.572999 mm`, and hence HD95 at least
that value. No oracle, tube ranking or XQ topology rule over this fixed output
domain can reach the frozen `3.2 mm` HD95 line.

Decision: the failure is not merely that Experiment 9 retained false ridges.
The raw prior hard domain also prevents the upstream extraction from covering
the missing reference surface. Do not implement a tube-selection policy or
gold-guided subset search on this result.

## Experiment 10: unclipped TubeTK traversal from the same endpoint seeds

Status: planned; no output has been accepted.

Hypothesis:

```text
exactly reproduced bridge-only mask and Experiment-9 endpoint seed list
-> recovered v2.1 high-resolution resampling, blur and intensity window
-> locked TubeTK 1.3.5 ridge traversal on the CT physical frame
-> locked radius-aware rasterization and ITK nearest-neighbor back-map
-> ITK OR with unchanged bridge-only, without raw-prior output clipping
```

Only the already disproved hard-domain policy changes. The physical ROI still
clips the explicit endpoint seed list, so the offline prior remains seed/
topology evidence; it no longer zeroes CT input outside `organ OR coarse` and
no longer deletes an extracted ridge merely because it leaves the raw prior.
This matches Diagnostics D/E: the dominant missing reference branches lie
outside the raw prior while retaining strong vesselness, and `D:/XQ` assigns
the offline masks as seed/ROI evidence rather than anatomical ground truth.

All TubeTK values, source hashes, bridge-only construction and 59-seed order
remain identical to Experiment 9. Gold is absent from extraction and union.
Reject unless all five frozen lines pass. Do not follow with a CT/vesselness,
TubeTK-parameter, output-radius or seed-selection grid.

### Result

Bridge-only again reproduced exactly. The same 59 seeds produced 48 TubeTK
ridges with 38,985 points. Radius-aware rasterization produced 105,680
working-grid voxels; back-mapping without prior clipping retained 37,604 tube
voxels and added 20,439 net voxels to bridge-only.

| Dice | precision | recall | prediction-to-reference P95 mm | reference-to-prediction P95 mm | HD95 mm | ASSD mm | clDice | components | largest fraction |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0.734054937 | 0.695981509 | 0.776535018 | 21.5930595 | 9.31238365 | 21.5930595 | 2.84630965 | 0.711163578 | 4 | 0.997835798 |

Decision: reject. Removing the raw-prior hard clip raises recall but admits
more false ridge surface and still leaves the directed missing-reference P95
far outside the frozen line. No TubeTK parameter changed. Log:
`XQ/build_tubetk_endpoint_recovery/experiment10-case1-20260714.log`.

## Diagnostic I: 59-seed unclipped TubeTK subset ceiling

Status: complete without another output run.

The same set-inclusion argument as Diagnostic H applies. Experiment 10 is the
superset of bridge-only plus every back-mapped ridge voxel from the 59 fixed
seeds, yet its reference-to-prediction P95 is `9.31238365 mm`. Any tube subset
has directed P95 and HD95 at least this large. Tube selection cannot repair the
59-seed unclipped result.

## Experiment 11: unclipped TubeTK traversal from every trusted-tree endpoint

Status: planned; no output has been accepted.

Hypothesis: keep Experiment 10's CT-frame input, recovered v2.1 preparation,
locked TubeTK extraction/rasterization and unclipped back-map unchanged, but
seed from all 96 endpoints of the exactly reproduced bridge-only tree. The 37
endpoints omitted by Experiments 9/10 were excluded only by the raw ROI already
disproved in Diagnostics D/E. They are derived from the trusted prediction,
not from gold, case identity or an evaluator.

This is one domain-boundary correction, not a seed-selection search. Endpoint
order remains deterministic and no TubeTK value changes. Reject unless all
five frozen lines pass. This is the final TubeTK seed/domain variant: on
failure, do not add endpoint subsets, direction rules, threshold grids or
TubeTK parameter changes.

### Result

Bridge-only again reproduced exactly. All 96 trusted-tree endpoints were
passed in deterministic order to the unchanged TubeTK extractor. Seventy-five
ridges containing 58,318 points were extracted. Radius rasterization produced
197,354 working-grid voxels; the unclipped back-map retained 70,015 tube voxels
and added 35,668 net voxels.

| Dice | precision | recall | prediction-to-reference P95 mm | reference-to-prediction P95 mm | HD95 mm | ASSD mm | clDice | components | largest fraction |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 0.709422140 | 0.635596007 | 0.802652294 | 22.5745773 | 7.24410629 | 22.5745773 | 3.20489979 | 0.703034942 | 6 | 0.993299935 |

Decision: reject. The all-endpoint superset still has directed
reference-to-prediction P95 `7.24410629 mm`; by monotonicity, no subset of
these ridge voxels can satisfy HD95 `<=3.2 mm`. TubeTK parameter changes,
endpoint subsets, direction filters and further seed/domain variants are
prohibited. Log:
`XQ/build_tubetk_endpoint_recovery/experiment11-case1-20260714.log`.

## Diagnostic J: packaged predecessor-work exhaustion audit

Status: complete without another output run.

The remaining `D:/XQ` contents were checked against the task's reuse boundary.
The root documents describe reusable kernels and offline priors, not a complete
automatic portal-vein implementation:

- `D:/XQ/可复用工作打包说明.md` assigns diffusion, Frangi/Sato,
  region-growing/level-set, connected components and relabeling to ITK; it
  explicitly assigns portal seeds, topology separation, peripheral recovery
  and reconnection policy to XQ.
- `D:/XQ/规划-血管前处理与血流.md` states that the TotalSegmentator label is a
  coarse merged prior and that off-the-shelf work does not supply the fine
  portal tree, portal-versus-hepatic/splenic separation, or the recovery
  strategy.
- `D:/XQ/research` contains ITK-TubeTK 1.3.5, the locked
  ITKMinimalPathExtraction source and TotalSegmentator 2.15.0. It contains no
  other packaged three-dimensional portal-vein segmenter or model.
- The local reference PDFs concern hemodynamics and multiscale circulation;
  they do not provide another segmentation implementation. The project
  reference list names ITK/TubeTK and the IRCAD data, but no additional
  packaged portal-vein predictor.

Every executable predecessor operation in that package has now been accounted
for on case 1:

| Reused work | Evidence |
| --- | --- |
| ITK diffusion + Frangi objectness | common preprocessing for the recovered baseline and all schema-4 experiments |
| ITK ConfidenceConnected | the original single-seed baseline reaches Dice `0.774009725` but fails HD95/ASSD/clDice; the parent task already rejected larger multiplier, multiple initial-mask seeds and NeighborhoodConnected because they leak into large false foreground |
| ITK threshold/reconstruction + components/relabel | fixed topology reaches Dice `0.783577002`; unclipped Experiments 6/7 recover the missing reference but add `1,658,351` / `556,705` voxels and fail shape metrics |
| ITK threshold level set | Experiments 4/5 fail HD95 and ASSD |
| ITK GeodesicActiveContour | schema-3 case-1 result Dice `0.484205228`, HD95 `77.3510742 mm` |
| ITK Sato line measure | Experiment 8 is voxel-identical to rejected Experiment 7 within the trusted domain |
| locked ITKMinimalPathExtraction | bridge-only best result Dice `0.786042999`, HD95 `11.1824560 mm`, ASSD `1.47809405 mm`, clDice `0.787422523` |
| ITK-TubeTK 1.3.5 | full v2.1 recall `0.244434142`; endpoint Experiments 9-11 all fail, and the all-endpoint superset still has reference-to-prediction P95 `7.24410629 mm` |
| TotalSegmentator 2.15.0 | standard coarse prior recall `0.362918103`; `liver_vessels` and combined/multi-marker variants were rejected and do not supply the missing fine tree |

The previously suggested bridge-mask-seeded ConfidenceConnected run is not a
new upstream method. The original baseline already uses the locked
`2.0 / 3 / radius-1` ConfidenceConnected profile, and the parent task records
that seeding it from multiple voxels of the initial mask causes large
false-positive growth. Replacing that initial mask with the larger bridge-only
mask supplies more of the same seed population and no independent anatomical
or tubular discriminator. Do not run it as Experiment 12.

Decision: the complete implementations and offline results currently packaged
under `D:/XQ` cannot satisfy the frozen case-1 five-line gate. There is no
unexecuted predecessor result in that package that can honestly be substituted
for the failed masks. Do not migrate schema 4 to production/GUI, do not open
case 5, and do not start another parameter, seed, threshold, TubeTK or level-set
variant. A further output-changing run requires an explicitly added upstream
portal-vein model or paper implementation and a revised source/version lock;
that is a scope decision, not continuation of the existing packaged-work audit.
