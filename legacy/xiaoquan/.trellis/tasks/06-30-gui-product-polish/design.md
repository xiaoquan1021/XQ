# Design: GUI 产品级优化

## Scope and Worktree

- Trellis task files live in `C:\Users\OCEAN\Desktop\XIAOQUAN`.
- Code implementation target is `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`.
- Editable implementation scope: `src/app`, `src/visualization`, `resources`, `tests/app`, `tests/visualization`, and build/test glue needed for those tests.
- Forbidden scope: core payload semantics, io readers, service algorithms, real pipeline data contracts, and untracked `xq_app_dist/`.

## Architecture Boundaries

- Preserve `app -> services -> adapters -> io -> core`.
- UI remains a thin intent/controller layer. This task may adjust how the app renders or styles existing scene data, but must not add business logic to widgets.
- VTK stays in app/visualization private implementation. Do not expose VTK types through core/service APIs.
- Scene mutation still goes through `XQCommandStack` and existing controllers.

## R1 i18n Design

1. Add an i18n probe close to the translator load path:
   - `QFile::exists(":/i18n/xq_zh_CN.qm")`;
   - `QTranslator::load(...)`;
   - `!translator.isEmpty()`;
   - `translate("XQMainWindow", "&File")` returns a Chinese string.
2. Log a concise `qWarning()` when any probe fails. The app may still run in English, but failure must be visible during verification.
3. Add a Qt test for the resource translator. The test should initialize `xq_resources`, load the `.qm`, assert the key translation, and not depend on screenshots.
4. Keep one canonical translator instance for runtime switching. If moving translator ownership is necessary, ensure English removal and Chinese reinstall remain symmetric.
5. Validate with true app startup via `run_xq.bat`, not only test binaries.

## R2 Resize Performance Design

1. Replace immediate resize-time VTK render in `XQMprView::eventFilter` with a coalesced render scheduler.
2. Keep resize event work cheap:
   - reposition axis labels;
   - mark the affected axis dirty;
   - restart a short single-shot timer, roughly 50-100 ms.
3. When the timer fires, render only dirty axes at current label size, then clear dirty flags.
4. Existing pixmaps remain visible during drag. If needed, use label alignment/centering or cheap pixmap scaling while waiting for the deferred render.
5. Slider/crosshair/seed changes may still render immediately because they are discrete user actions, not high-frequency window resize streams.

## R3 3D View Design

1. Before constructing `QApplication`, set the VTK/Qt surface format recommended by `QVTKOpenGLNativeWidget`.
2. Ensure `XQRenderWidget` has a stable non-zero minimum/render area inside the 3D frame.
3. Trigger an initial render after the widget is shown or resized to a valid size so the cell paints black/VTK background even before a geometry node is selected.
4. Keep selection-driven geometry rendering in `XQMainWindow::onSceneSelectionChanged`; do not duplicate geometry mapping elsewhere.
5. If the widget still appears blank, add diagnostics for widget size, render window presence, renderer actor count, and OpenGL/context errors available through Qt/VTK logs.

## R4 UI Polish Design

1. Use the current QSS/icon resource system. Do not introduce third-party theme dependencies in this pass.
2. Improve the visible product surface:
   - toolbar and dock density;
   - data tree row/selection/hover states;
   - status bar legibility;
   - stage panel hierarchy and disabled states;
   - MPR/3D panel frame contrast and overlay readability;
   - dialog/popup consistency.
3. Keep changes scoped and reversible: a failed style tweak should be removable without touching app logic.
4. Avoid large marketing-style visuals. This is an operational medical imaging workbench, so the UI should be dense, quiet, and scannable.

## Compatibility and Rollback

- If the i18n load change breaks switching, revert only the translator ownership/loading helper and keep tests as diagnosis.
- If resize coalescing causes stale slices, lower debounce delay or render dirty axes on resize release/final layout events; do not reintroduce synchronous per-event VTK rendering.
- If the VTK surface-format change causes startup failure, guard the include/build integration and keep the render timing diagnostics.
- UI QSS changes should be isolated in `resources/xq.qss` and easy to revert independently.
