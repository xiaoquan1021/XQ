# Implementation plan: reuse-first portal-vein segmentation v4

Execute in order. Do not open case 5 or start centerline/mesh/flow work.

## 1. Correct and reproduce the baseline

1. Correct the task authority to match `D:/XQ`.
2. Restore the deleted development analyzer from original JSONL patches only
   through the last verified MinimalPath revision.
3. Restore ITKMinimalPathExtraction from the locked `D:/XQ/research` source,
   verify every upstream hash and build it against product ITK 5.4.
4. Reproduce the legacy root, fixed topology and MinimalPath metrics exactly.

Gate: the recovered result must match Dice `0.786042999`, HD95 `11.1824560`,
ASSD `1.47809405` and clDice `0.787422523` before migration begins.

## 2. Establish the schema-4 contract

1. Add a new profile/identity rather than altering schema-3 meaning.
2. Represent root thresholds/region-growing parameters, physical morphology,
   component/topology policy, optional MinimalPath and optional local level-set
   parameters explicitly.
3. Add typed stages/diagnostics and artifact-v7 persistence while retaining
   read compatibility for v1-v6.
4. Keep public headers XQ/std-only.

## 3. Extract the production adapter

1. Reuse existing image/source/vesselness/ROI validation and ITK import helpers.
2. Restore the legacy ITK root behavior as private schema-4 composition; do not
   call the old custom segmentation service or depend on a saved case artifact.
3. Move candidate threshold/reconstruction/components to private ITK helpers.
4. Move only the frozen, reference-independent component/topology policy from
   the analyzer. Remove gold audit fields from the production path.
5. Wrap the locked MinimalPath implementation behind a private adapter that
   receives explicit physical endpoints and speed image.
6. Materialize one final XQ mask, provenance and component/path records.

Gate: synthetic tests prove production output is unchanged if evaluator/gold
files are absent, renamed or modified.

## 4. Improve peripheral recovery on case 1

1. Use the separate evaluator to localize the remaining reference-to-prediction
   surface error and missing distal topology.
2. Run one documented hypothesis at a time using only `D:/XQ`-authorized
   mechanisms: ROI-constrained ITK region growing/threshold, component policy,
   optional MinimalPath reconnection or local ITK level set.
3. Record the profile, prediction hash, metrics and rejection/acceptance reason
   for every run. Do not hide failures or perform an unrecorded parameter grid.
4. Promote a change only after rerunning without any gold-dependent selection
   and proving deterministic output.

Gate: case 1 passes Dice `>=0.70`, HD95 `<=3.2 mm`, ASSD `<=1.2 mm`, clDice
`>=0.75` and connected-component fraction `>=0.90`.

## 5. Product cutover and verification

1. Route CLI and desktop prediction through schema 4; no legacy fallback/union.
2. Add artifact-v7 round-trip and legacy-reader regression tests.
3. Build affected Release targets and run focused tests.
4. Run canonical Flow-ON and Flow-OFF full CTest, dependency/build-graph/PE
   audit and `git diff --check`.
5. Produce one loadable case-1 artifact/project and document exact desktop
   operations for MPR/3D visual acceptance.

## 6. Freeze before held-out evaluation

After automated and user visual acceptance, freeze schema 4, all parameters and
the case-1 artifact. Only then may case 5 be opened once through the independent
evaluator. Do not tune after that result.

## Hard stops

- Do not open case 5 before the freeze.
- Do not let gold, case identity or evaluator output enter production code.
- Do not hand-write ITK/upstream image or path kernels.
- Do not claim completion from build/test success while the mask is visibly or
  quantitatively wrong.
- Do not use Claude or subagents.
- Do not revert unrelated dirty-worktree changes.
