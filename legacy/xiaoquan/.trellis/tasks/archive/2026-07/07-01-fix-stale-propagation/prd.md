# Fix transitive stale propagation

## Goal

`XQScene::mark_source_changed(source)` must mark every downstream node reachable
through `derived` relations as stale, not only the source's immediate children.

## Confirmed Evidence

- `XQ/src/core/XQScene.cpp:104-125` only finds
  `derived_by_source_[source]` and marks that single set.
- `XQ/tests/core/test_scene_relations.cpp:18-47` covers only one direct
  `source -> derived` relation.
- `XQ/tests/io/test_project_versioned_save.cpp:224-229` contains persisted
  stale data for a chain `3001 -> 3002 -> 3003` with both downstream nodes stale,
  which is the behavior the in-memory scene API should produce directly.
- `XQ/tests/io/test_project_roundtrip.cpp:101-123` currently works around the
  missing transitive behavior by calling `mark_source_changed(1003)` and then
  `mark_source_changed(1004)` separately.

## Requirements

### R1. Downstream stale propagation is transitive

For a chain `A -> B -> C`, `mark_source_changed(A)` marks both `B` and `C` as
`SourceChanged` and returns `2` when neither was previously stale.

### R2. Repeated marking counts only newly stale nodes

If a downstream node is already stale, it remains stale but is not counted again.
For example, after `mark_source_changed(A)` on `A -> B -> C`, a second call
returns `0`.

### R3. Fan-out, fan-in, and duplicate reachability do not double-count

For `A -> B`, `A -> C`, `B -> D`, `C -> D`, `mark_source_changed(A)` marks
`B`, `C`, and `D` exactly once and returns `3`.

### R4. Source node itself remains non-stale

The changed source must not be marked stale by its own change. If malformed
relations contain a cycle, traversal must terminate and must still not mark the
initial source stale.

## Acceptance Criteria

- [ ] Add a core scene regression for `A -> B -> C`; one call to
  `mark_source_changed(A)` returns `2` and marks `B`, `C`, not `A`.
- [ ] Add a diamond regression; `D` is marked and counted only once.
- [ ] Add a repeated-call regression for a transitive chain; the second call
  returns `0`.
- [ ] Existing direct relation, boundary, remove-cleanup, project roundtrip, and
  versioned-save tests continue to pass.
- [ ] Final child check: Release full `ctest` passes.

## Out of Scope

- Do not change `XQScene::link_derived()` validation or introduce cycle
  rejection in this task.
- Do not add new stale reasons.
- Do not change project file format or persisted stale syntax.
