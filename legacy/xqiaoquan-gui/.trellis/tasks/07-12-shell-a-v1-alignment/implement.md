# Implementation Plan: 壳档 A v1 对齐

## Current Phase

Phase 2 / in_progress. The parent, canonical-entry child and Path/module-contract child are active. Centerline-B and final acceptance retain independent lifecycle gates and must follow their own task status.

## Planned Task Tree

| Order | Child | Deliverable | Dependencies |
| --- | --- | --- | --- |
| 1 | `shell-a-canonical-entry` | canonical ON/OFF/build/run dependency propagation and reproducibility | existing dependency-remediation evidence |
| 2 | `shell-a-path-module-contract` | A5–A8 Path snapshot, source, validator, geometry smoke and static modules | existing Profile/Flow capability contracts |
| 3 | `shell-a-centerline-b` | vmtk-OFF ITK 3D thinning, physical radius, graph/prune and publication | canonical entry + Path contract |
| 4 | `shell-a-acceptance-v1` | A1–A15 full evidence, real GUI ritual and final wording | children 1–3 |

Each child is independently planned, implemented, checked and archived. Parent completion requires all four plus a parent-level matrix review.

## Step 1 — Canonical Entry

Actions:

- Create one shared canonical dependency configuration consumed by current ON/OFF build and GUI run wrappers.
- Migrate isolated QtBase, exact Qt/VTK/ITK/tinyxml2/GDCM roots and locked configure-only Python.
- Use fresh build directories and preserve old caches as evidence.
- Run dependency negative probes, build graph and recursive PE closure checks.
- Run Flow ON and OFF full Release CTest sequentially.

Exit gate:

- A1 dependency/runtime prohibitions pass through the actual developer entry scripts.
- No host Anaconda, old Qt or broad prefix fallback remains in fresh canonical caches.
- Reproduction commands and logs are ready for A15.

## Step 2 — Path and Module Contract

Actions:

- Add immutable `VesselPathV1` module snapshot and validator.
- Add deterministic Profile-to-Path adapter with radius-from-area and source mapping.
- Add Path dump and typed diagnostics.
- Add Path-only static registry, Noop, PathValidate and pure geometry smoke.
- Wire app/session without altering Flow capability semantics.

Exit gate:

- A5/A6/A7/A8 focused tests pass.
- Module public headers contain no Qt/ITK/VTK/Contour/Flow dependencies.
- Flow OFF build runs Noop/PathValidate and geometry browse.
- Existing Profile/Flow/persistence full regression remains green.

## Step 3 — Centerline B

Actions:

- Integrate the audited true 3D thinning source behind an ITK adapter.
- Convert XQ mask geometry to ITK without losing direction/LPS/mm.
- Generate skeleton, physical distance radius, deterministic graph/prune/main path.
- Publish XQPath + segmentation-derived VesselProfile atomically with lineage/stale behavior.
- Prove the path with vmtk absent/OFF.

Exit gate:

- A13 focused synthetic, oblique/anisotropic and non-PHI fixture tests pass.
- Every accepted station has finite LPS-mm position, positive radius and non-decreasing arclength.
- Failure cases produce no partial Scene/Asset state.
- Full canonical ON/OFF regressions remain green.

## Step 4 — Final Acceptance v1

Actions:

- Re-run every A1–A15 item from a fresh canonical setup.
- Run the existing real DICOM gate and preserve LIDC-only wording.
- Capture Path dump, source, geometry smoke and module logs.
- Verify ScaleSlot, TetGen OFF/research-only and existing persistence/lineage behavior.
- Execute the written GUI checklist on the user's physical machine.
- Produce final matrix, limitations, sample IDs and exact commands.

Exit gate:

- No A1–A15 row is partial, skipped or inferred from another test.
- User physical-machine GUI acceptance is recorded.
- Parent wording states only “稳平台 + 真数据 + 几何契约 + 可挂模块 + 薄尺度位”.

## Planned Validation Responsibilities

Exact target names are frozen in child designs, but these responsibilities cannot be removed:

```powershell
cmd /c XQ\build_gui_wt.bat
ctest --test-dir XQ/<canonical-on-tree> -C Release --output-on-failure

cmd /c XQ\build_shell_noflow_wt.bat
ctest --test-dir XQ/<canonical-off-tree> -C Release --output-on-failure

# Focused responsibilities
ctest -R "dependency|vessel_path|shell_module|geometry_smoke|thinning|centerline_b|shell_a_v1"

cmd /c XQ\run_xq.bat
```

Full suites must run sequentially. The final child records the actual commands, counts and build paths rather than copying these placeholders.

## Context and Review Gates

- Every child receives only relevant specs/research in `implement.jsonl` and `check.jsonl`.
- Before code, the selected child must run `trellis-before-dev` and load the exact layer specs.
- After code, a full-scope Trellis check must compare implementation against its child PRD and the parent A1–A15 matrix.
- Any proposed project schema change, second geometry authority, dynamic plugin system or full vascular-v2 expansion requires stopping and updating planning first.

## Workspace Protection

- Preserve the mixed dirty worktree and user-owned untracked evidence.
- Never use `git add .`, `git clean`, hard reset or broad build-directory deletion.
- Do not reparent/archive/rename active Shell A or vascular-foundation tasks as an implementation shortcut; archive genuinely completed tasks only through the Trellis lifecycle command.
- Do not auto-commit task creation or planning changes.

## Current Execution Gate

- [x] Parent is `in_progress` and all current context manifests validate with real entries.
- [x] Canonical-entry and Path/module-contract are independently active.
- [ ] Centerline-B remains `planning` until explicitly started; when active, it owns the minimal A13 fallback and does not wait for v2 tree work.
- [ ] Final acceptance remains `planning` until children 1-3 pass their own checks.
- [x] Dependency-remediation evidence is reused as a completed prerequisite rather than bundled into this task.
