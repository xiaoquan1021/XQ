# Fix XQProject close state reset

## Goal

`XQProject::close()` must return the whole project-owned runtime state to a closed, clean state. It currently clears `XQScene` only, leaving `AssetRegistry` records and asset lineage alive after close/reopen. Reusing the same `XQProject` instance can therefore mix old asset metadata with a new scene.

## Requirements

- `close()` on an open project clears both scene state and project-owned asset state.
- Asset records, node-asset bindings implied by cleared nodes, and asset relations must not survive `close()`.
- `reopen()` must expose the clean closed state produced by `close()`; old assets must not be findable after reopen.
- Invalid lifecycle transitions must remain non-mutating.
- Keep the fix inside core ownership boundaries. Do not add UI, IO, or resource-manager behavior.
- Preserve existing scene identity guarantees across open/close/reopen.

## Acceptance Criteria

- `test_project_lifecycle` fails before the fix when an open project with asset records and relations is closed.
- After the fix, `close()` leaves `assetRegistry().assetCount() == 0` and `relationCount() == 0`.
- After `reopen()`, old asset IDs are not findable and the registry remains empty until new assets are created.
- Existing lifecycle tests and asset registry tests still pass.
- Full Release `ctest` passes.

## Non-Goals

- Do not change native project file format.
- Do not add project loading into existing `XQProject` instances.
- Do not implement resource cache eviction; this task covers project-owned core state only.
