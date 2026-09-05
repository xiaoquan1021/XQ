# Vascular segmentation: reuse-first portal-vein geometry front end

> Parent: `07-12-vascular-foundation-auto-segmentation`
>
> Delivery tier: post-Shell-A v2 vascular-foundation work. This task does not
> block `07-12-shell-a-v1-alignment` or its A1-A15 acceptance.
>
> Execution disposition (2026-07-14): preserve all research and negative
> evidence, but defer further segmentation/model work while Shell A v1 remains
> incomplete. See `scope-correction.md`.
>
> Status remains `in_progress`. Case 1 is the development case. Case 5 remains
> closed until a profile is frozen. Do not finish, archive or commit before the
> user accepts the rendered result on the real desktop application.

## Goal

Produce a correct, operable three-dimensional portal-vein mask by implementing
the method and reuse boundary actually selected in `D:/XQ`:

```text
ITK anisotropic diffusion
-> ITK Frangi/Sato multiscale vesselness
-> ITK region growing / thresholding
-> ITK ConnectedComponent / RelabelComponent
-> project-owned peripheral-branch recovery and optional reconnection
```

TotalSegmentator 2.15.0 supplies offline liver and coarse-vessel ROI data. It
is not a product runtime. XQ owns the versioned pipeline, parameters, portal
topology policy, external-branch recovery, adapters, persistence and callers;
ITK and the locked upstream modules own image-processing/path kernels.

## Authority correction

The previous v3 task contract incorrectly prohibited component policy,
reconnection, topology rules and development tuning. That conflicts with the
highest-authority project documents:

- `D:/XQ/可复用工作打包说明.md` says ITK region growing/level set and
  ConnectedComponent/RelabelComponent are directly reusable, warns that
  largest-component filtering loses disconnected peripheral branches, and
  requires a reconnection strategy.
- `D:/XQ/规划-血管前处理与血流.md` selects the traditional ITK chain, requires
  ROI-constrained peripheral portal-branch recovery and optional graph
  reconnection, and states that hybrid-pipeline design and tuning are project
  work.
- The same documents classify segmentation parameters, portal seeds and
  topology-separation rules as configurable project-owned core strategy.

Frangi/Sato and ITK define reusable kernels. They do not supply a complete,
validated abdominal portal-vein product. XQ must assemble and validate that
missing strategy without reimplementing ITK kernels.

## Recovered evidence

The deleted case-1 development implementation was recovered from the original
Codex JSONL and rebuilt against the locked sources. A fresh reproduction gave:

| Variant | Dice | recall | HD95 mm | ASSD mm | clDice |
| --- | ---: | ---: | ---: | ---: | ---: |
| legacy ITK root | 0.774009725 | 0.724174901 | 19.1801262 | 2.23462363 | 0.723033273 |
| fixed component/topology policy | 0.783577002 | 0.749326302 | 11.8323212 | 1.56305641 | 0.771635092 |
| locked upstream MinimalPath bridge | 0.786042999 | 0.753943187 | 11.1824560 | 1.47809405 | 0.787422523 |

This is the implementation baseline, not completion evidence. It still fails
the frozen HD95 and ASSD lines and therefore must not be presented as correct.

## Fixed dependency baseline

- C++17, MSVC dynamic CRT, no Debug/Release mixing.
- ITK exactly `5.4.0`; explicit module closures only.
- GDCM exactly `3.0.10` through ITK IOGDCM.
- VTK exactly `9.3.0` for the existing shell and visualization bridge; no VTK
  segmentation kernel.
- TotalSegmentator exactly `2.15.0`, offline data generation only. Python,
  torch, nnU-Net and weights never enter product targets or launch paths.
- ITKMinimalPathExtraction commit
  `35dd8e83b7df2059876e6835a5741eb3d45973bf`, Apache-2.0, archive SHA-256
  `A2EDCCA4BC07175487BE34E0A6C1B780CC176A67E6F9A1DE20C21D55911D4FB4`.
- ITKThickness3D remains the locked upstream thinning implementation already
  used by XQ; XQ does not write a thinning kernel.

## Target product identity

The corrected production route will use a new identity rather than silently
changing v3 semantics:

```text
algorithm_id      xq.itk.roi-topology-vessel-segmentation
algorithm_version 4.0.0
profile_schema    4
artifact_version  7
```

Artifacts v1-v6 remain readable according to their existing compatibility
rules and are never automatic fallbacks or unions.

## Requirements

1. Production inputs are CT, XQ voxel source, Frangi/Sato vesselness, explicit
   offline ROI roles and one versioned profile. Gold, case number, Path and
   manual contour are absent from the production API and process.
2. ITK owns diffusion, Hessian objectness, thresholding, region growing,
   morphology, reconstruction, connected components, relabel, distance maps,
   thinning and level-set operations. XQ must not implement voxel flood fill,
   connected-component traversal, Hessian, distance transform or level-set
   updates.
3. XQ may and must own the method-specific orchestration that `D:/XQ` assigns
   to the project: parameter profiles, seed/core choice, candidate/component
   selection, portal topology rules, peripheral-branch recovery and deciding
   which explicit endpoints are passed to a reused path kernel.
4. ITK physically aligns `liver` and `portal_vein_and_splenic_vein` NIfTI data
   to the CT LPS/mm grid with nearest-neighbor interpolation. The combined
   vessel label is a coarse prior, not a final portal mask and is never copied
   wholesale into output.
5. The root mask is produced by the reusable ITK region-growing/threshold and
   morphology chain. The old custom `SegmentationService` threshold, 6-neighbor
   grow and largest-component kernels are unreachable from this workflow.
6. Candidate additions are labeled and relabeled by ITK. The versioned XQ
   policy consumes only materialized component statistics and topology support;
   all thresholds and decisions are persisted. It may retain multiple branches
   and must not reduce the result to the largest component.
7. Optional reconnection uses the locked upstream ITKMinimalPathExtraction
   implementation or an official ITK level-set/path kernel. XQ supplies
   explicit endpoints and a physical speed image; it does not implement a path
   optimizer. Automatic endpoint selection is honestly identified as XQ portal
   strategy, not attributed to the upstream paper/module.
8. Invalid geometry/lineage, missing ROI roles, empty root/candidate/output,
   all-foreground output, failed reconnection, cancellation, allocation failure
   and ITK exceptions return typed failures with no partial mask or Scene state.
9. Profile schema 4 records every physical threshold, component/topology rule,
   reconnection limit and upstream implementation/version. No hidden
   environment override or case-label branch exists.
10. The CLI predictor and desktop action use the same adapter and profile.
    Successful values are fully materialized XQ-owned objects; public headers
    expose no ITK/NIfTI/VTK/Python type.
11. Case 1 may be rerun as development data only for a documented hypothesis.
    Gold is read by the separate development analyzer/evaluator after a
    prediction is closed. Each accepted parameter change is recorded; failed
    experiments remain visible.
12. Case 5 remains unopened until schema 4 and its profile are frozen. It is
    evaluated once without parameter changes; a failure stays a failure.

## Acceptance criteria

- [ ] AC1: Source/build/PE audit proves the exact dependency identities above,
  the locked upstream hashes, and no Python/TotalSegmentator/second ITK prefix
  in the product closure.
- [ ] AC2: Synthetic tests execute the real ITK region-grow/threshold,
  components/relabel and optional reconnection path; geometry, determinism,
  cancellation and typed zero-partial-state failures are covered.
- [ ] AC3: Production code cannot read gold or branch on case identity. A test
  proves evaluator/development inputs cannot alter prediction.
- [ ] AC4: Artifact v7 round-trips profile schema 4, ROI lineage, root/candidate
  counts, component/topology decisions, upstream identities, reconnection
  records and hashes; v1-v6 compatibility remains intact.
- [ ] AC5: The case-1 development result passes all frozen data-gate lines:
  Dice `>=0.70`, HD95 `<=3.2 mm`, ASSD `<=1.2 mm`, clDice `>=0.75`, and largest
  26-connected component fraction `>=0.90`.
- [ ] AC6: The same case-1 artifact can be loaded through the normal desktop
  project/workflow and rendered in MPR/3D for user judgment. Automated metrics
  do not replace this visual acceptance.
- [ ] AC7: Affected Release targets, focused tests, Flow-ON and Flow-OFF full
  CTest, build-graph/dependency/PE audits and `git diff --check` pass.
- [ ] AC8: Only after AC1-AC7 and user acceptance is the profile frozen and case
  5 eligible for its one held-out evaluation.

## Out of scope

- Opening or tuning on case 5 before the schema-4 freeze.
- Centerline-tree product delivery, surface/mesh generation or blood flow.
- TotalSegmentator/nnU-Net/Python product runtime.
- Hand-written replacements for kernels supplied by ITK or the locked upstream
  MinimalPath/thinning modules.
- Claiming success from compilation, test count or an artifact that still fails
  the quantitative and real-machine visual gates.
