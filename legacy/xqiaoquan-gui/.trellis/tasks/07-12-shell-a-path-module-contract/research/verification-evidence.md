# Path/module implementation and automated evidence

Date: 2026-07-12

Status: the earlier additive GUI implementation was superseded after the user
clarified that Path/Modules must replace the old Flow shell entry. Replacement
implementation and automated re-verification are complete; the task remains
`in_progress`. Whether the shell satisfies the requested real-machine behavior
is reserved for the user's inspection.

## Implemented scope

- Added `VesselPathV1` plus typed validator issues, LPS/mm/equivalent-circle
  radius, minimal source claim, and exact Profile derivation stamp.
- Added Profile-to-Path snapshot, stable PHI-minimized dump, and geometry-only
  smoke summary.
- Added static `Noop` and `PathValidate` modules behind a Path-only interface,
  registry and synchronous project controller.
- Replaced the old fifth-page Flow shell entry with one Modules workflow. Path
  input preparation and Path-only module execution now share that page, and
  `xqPathModuleRun` is the shell's only module execution entry.
- Removed the retired Flow page/source/protocol/run object contracts and Flow
  capability-state wiring from the shell. The backend Flow controller remains
  independently available only in Flow-ON builds.
- A newly prepared Path input now becomes the selected module source. Changing
  the selected source or module immediately clears the prior source/geometry/dump
  so the page cannot present a result from a different input.
- Added an explicit copy command for the current validated canonical dump and
  PHI-free application logging for module id, honest source, station count, and
  radius range. Failure clears prior output and logs status/diagnostic.
- Kept `VesselProfileV1` as the only persisted physical geometry authority; no
  Path payload, Scene domain, or project schema was added.
- Added core/service/registry/controller/GUI/i18n/architecture regressions.

## Review findings fixed

- Rejected an optional `AssetId` whose value is invalid even when a fingerprint
  is present; optional presence alone is not a valid provenance stamp.
- Propagated the original `VesselProfileValidationResult` through
  `PathModuleController` instead of reducing an invalid Profile to a generic
  snapshot failure.
- Replaced raw radius accumulation with an online mean so valid maximum finite
  radii do not overflow on MSVC, where `long double` is not wider than `double`.
- Added an executable architecture guard that prevents the module public header
  from growing Scene/Contour/Flow types; the existing general guard covered only
  third-party leakage.
- A targeted build launched from plain PowerShell failed before compilation
  because MSVC standard include paths were absent. The correct project recipe is
  `vcvars64.bat` followed by `VSLANG=1033`; no source change was made for that
  environment failure.
- Trellis channel dispatch previously failed with `spawn codex ENOENT`. No
  Claude provider or fallback was used; implementation/check remained inline.
- The replacement GUI exposed a real save/reopen defect: native save auto-bound
  an Asset to an unbound Path input but left `contentFingerprint` empty, so the
  strict controller rejected the reopened source. The writer now fingerprints
  canonical payload text plus blob references; controller validation was not
  weakened. The focused DICOM -> Path module -> save/reopen -> Path module flow
  passes.

## Current replacement verification

- Flow ON and Flow OFF incremental Release builds passed after the replacement.
- Focused replacement/data-integrity checks passed in both trees: ON `7/7`, OFF
  `7/7`. These cover the Modules page, real DICOM -> Path input -> PathValidate
  -> save/reopen -> PathValidate, Flow capability boundaries, i18n, and writer
  fingerprint round trips.
- Final full Release CTest ran strictly sequentially:
  - Flow ON: `98/98` passed.
  - Flow OFF: `90/90` passed.
- Product source/resources/CMake contain none of the retired
  `xqStagePage_Flow`, `xqFlowSourceCombo`, `xqFlowSmokeProtocol`, or
  `xqFlowSmokeRun` identifiers. Their remaining occurrences are negative GUI
  assertions.
- The Flow-OFF `build.ninja` contains none of the six Flow execution sources,
  while `VesselPathSnapshotService`, `ShellGeometrySmokeService`,
  `PathModuleRegistry`, and `PathModuleController` are present.
- At the pre-hardening replacement baseline, `git diff --check` passed and the
  generated TS/QM catalog state was `405 finished, 0 unfinished`.

## Function-first UX hardening after the full baseline

- Both current Flow-ON and Flow-OFF `xq_app` targets build with the source
  selection, stale-result invalidation, copy action, logging, and regenerated
  translation resource.
- The real DICOM Shell-A GUI workflow now prepares a second Path input, confirms
  that exact new node is selected, invalidates the old result on module change,
  copies only the current successful dump, and still saves/reopens for another
  PathValidate run.
- Purpose-bound checks for that workflow and its Chinese UI resource passed in
  both trees: ON `2/2`, OFF `2/2`.
- The generated catalog now reports `407 finished, 0 unfinished`.
- The earlier full `98/98` and `90/90` results below predate this UX hardening.
  They are retained as the replacement baseline, not claimed as full-suite
  evidence for the exact current source. No additional full suite was launched
  merely to improve a number; user real-machine inspection remains pending.

## Superseded verification baseline

- The prior Flow ON/OFF results below apply to the superseded additive GUI and
  are not evidence for the replacement behavior.
- Flow ON incremental Release build: passed.
- Flow OFF incremental Release build: passed.
- Focused post-review checks: ON `9/9`, OFF `9/9`.
- Final sequential full Release CTest:
  - Flow ON: `98/98` passed.
  - Flow OFF: `89/89` passed.
- `test_arch_boundaries` passed with the new Path-only type guard.
- `git diff --check`: passed.
- TS/QM state from the implementation pass: 382 finished, 0 unfinished; exact
  Path action/source runtime lookups are covered by `test_i18n_resources`.

Automated green is evidence only. It does not close the child or replace the
user's real-machine GUI and source review.
