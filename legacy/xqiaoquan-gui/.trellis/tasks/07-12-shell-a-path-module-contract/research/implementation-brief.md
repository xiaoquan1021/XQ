# Implementation brief: Path/module contract

Active task: `.trellis/tasks/07-12-shell-a-path-module-contract`

Implement the reviewed PRD/design/implement plan end to end. The current task is the sole scope; do not judge or close the parent Shell A requirements.

Provider constraint: this run is Codex-only. Do not invoke Claude or any Claude provider/tool.

Editable scope:

- `XQ/src/core/` for `VesselPathV1` and validator only.
- `XQ/src/services/profile/`, `XQ/src/services/path/`, `XQ/src/services/modules/` for snapshot, dump, geometry-only smoke and static Path registry.
- `XQ/src/ui/controllers/`, `XQ/src/app/XQWorkflowSession.*`, `XQ/src/ui/panels/XQStageWidgets.*`, and minimal `XQ/src/app/XQMainWindow.*` wiring.
- `XQ/CMakeLists.txt`, focused tests under `XQ/tests/`, and `XQ/resources/i18n/xq_zh_CN.ts/.qm` when GUI strings change.
- Task-local research notes only if a non-obvious decision must be recorded.

Hard constraints:

- `VesselProfileV1` remains the only persistent physical geometry authority.
- `VesselPathV1` is an immutable runtime snapshot; no new Scene domain/payload/project schema.
- Module interface accepts only `const VesselPathV1&`; no Scene/Contour/Flow/Qt/ITK/VTK types.
- Registry is static and always built in Flow ON/OFF; do not change `WorkflowCapabilities` from its Flow-only meaning.
- Do not delete or weaken the existing Flow geometry smoke backend/service/controller tests. Replace its Shell-A GUI entry with the Path/Modules page; the backend is retained for later core use.
- GUI must call shared controller/services; no radius/source/validation logic in widgets.
- The engineering objective is the real Shell-A workflow required by `D:\XQ`,
  not a green test suite. Before changing a test, state the input, user action,
  observable product result, and failure behavior it protects. Never change an
  assertion to legitimize missing or incorrect product behavior.
- Replacement means the old shell entry is absent from the product, not hidden,
  disabled, renamed, or retained for compatibility unless the requirement says
  otherwise.
- Preserve all unrelated dirty worktree changes. No `git add .`, commit, clean, reset, old-tree deletion, or Externals edits.
- Tests must be purposeful: build affected targets and run focused tests tied to the replacement behavior, including absence of old Flow controls in ON/OFF. Do not launch full ON/OFF suites until the implementation is stable; the main session owns final sequential verification.

Expected completion report:

- Start with what the user can actually do in the product, the exact input ->
  action -> observable result workflow, and what is still missing.
- Then list files/contracts changed and focused build/tests as secondary evidence.
- Any unimplemented AC or uncertainty stated explicitly.
- Automated green never closes this task; keep it `in_progress` until the user
  completes real-machine inspection and accepts the behavior.
