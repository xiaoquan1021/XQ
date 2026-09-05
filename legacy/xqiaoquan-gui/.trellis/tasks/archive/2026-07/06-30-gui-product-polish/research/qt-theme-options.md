# Research: Qt theme / visual polish options

## Sources

- Qt Style Sheets Reference: https://doc.qt.io/qt-6/stylesheet-reference.html
- Qt Style Sheets Examples: https://doc.qt.io/qt-6/stylesheet-examples.html
- Project external dependency rules: `.trellis/spec/XQ/architecture/external-libs.md`

## Current Situation

- GUI worktree already has `resources/xq.qss` and bundled SVG icons under `resources/icons/`.
- Theme resources are compiled into the app via `resources/xq_resources.qrc`.
- The project spec treats dependencies as locked and requires new external libraries to stay within architecture boundaries and dependency governance.

## Options

### Option A: Minimal stable polish on existing QSS (recommended)

- Keep current dependency footprint.
- Tighten existing `xq.qss`: toolbar/dock/statusbar spacing, tree row height, selected/hover/disabled states, splitter and tab styling, MPR/3D panel chrome, message/dialog polish.
- Use existing SVG icon resources and add only local icons if necessary.
- Lower risk for Release build and Windows deployment.

### Option B: Introduce a third-party Qt theme library

- Could improve visual baseline faster, but adds licensing, packaging, stylesheet interaction, and dependency-update risk.
- Requires user approval plus dependency/spec update before implementation.
- Not recommended for the current stabilization task because i18n, resize, and VTK 3D are already high-priority correctness/performance risks.

## Design Implication

Proceed with Option A unless the user explicitly chooses a larger UI overhaul.
