# Scope Correction: Shell A Boundary

Recorded: 2026-07-14

Memory source: Codex session `019f5a3b-dbc7-7ca0-b946-3fd0497f89c5`, checked
with `trellis mem`, plus earlier Shell A sessions `019f50d3-5474-7202-a2be-4deb651b8f83`
and `019f5251-4c96-76d0-a916-e74f0cc56b35`.

## Decision

This task is an automatic vascular-segmentation research and production child
for the post-Shell-A v2 vascular foundation. It is not the application shell,
not the definition of Shell A completion, and not a prerequisite for
`07-12-shell-a-v1-alignment`.

The user's current goal is the reusable application shell defined by
`D:/XQ/规划-GoogleEarth全路线与壳子阶段.md` A1-A15. Shell A owns the stable desktop
host, real DICOM/Volume path, patient-mm geometry contract, positive-radius
Path, geometry-only smoke, static Path modules, minimal centerline-B fallback,
ScaleSlot, canonical build and real-machine acceptance. A segmentation result
is one replaceable upstream input to that host.

## Preservation Rule

- Preserve this task's source locks, failed experiments, metrics and negative
  evidence. They remain valid for later v2 work.
- Do not rename or rewrite this task into a shell task.
- Do not use its unresolved HD95/ASSD gate to block Shell A v1.
- While Shell A v1 is incomplete, do not continue model searches, parameter
  experiments, case runs, production cutover or GUI migration under this task.
- Case 5 remains closed and no existing result is promoted as correct.

## Current Routing

Resume the already-existing `07-12-shell-a-v1-alignment` task and its ordered
children:

1. `07-12-shell-a-canonical-entry`
2. `07-12-shell-a-path-module-contract`
3. `07-12-shell-a-centerline-b`
4. `07-12-shell-a-acceptance-v1`

No new Trellis task is required. The shell task already contains the correct
non-blocking boundary; only this segmentation task needed the durable scope
correction.
