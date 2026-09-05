# Design: XQProject close state reset

## Problem

`XQProject` owns two authoritative state containers: `XQScene` and `AssetRegistry`. `close()` clears the scene but leaves asset records and asset lineage intact. A subsequent `reopen()` keeps stale registry state that no longer has corresponding scene nodes.

## Approach

1. Add an explicit `AssetRegistry::clear()` operation.
2. `clear()` removes all records and relations. The asset id counter remains monotonic for the lifetime of the registry so the existing `createAsset()` contract ("never collides with any previously seen id") is not weakened.
3. Call `assetRegistry_.clear()` from `XQProject::close()` immediately after `scene_.clear()`.
4. Keep `reopen()` unchanged; it should not perform cleanup because `close()` is the lifecycle transition that owns cleanup.

## Edge Cases

- `close()` on `Created` remains `InvalidTransition` and must not mutate scene or registry.
- `open()` on an already open project remains `InvalidTransition` and must not mutate state.
- `reopen()` on `Created` remains `InvalidTransition` and must not mutate state.
- Asset relations must be cleared along with records to avoid dangling lineage.

## Test Strategy

- Extend `test_project_lifecycle` with a project that has scene nodes, asset records, node asset ids, and asset relations; assert close/reopen removes all asset state.
- Extend `test_asset_registry` to verify `clear()` removes records and relations while future created ids do not reuse ids seen before clear.
- Run targeted core tests, then full Release `ctest`.
