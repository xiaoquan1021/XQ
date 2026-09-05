# Implement Plan: GUI 产品级优化

## Preflight

- Work in `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`.
- Confirm dirty state before editing: only `xq_app_dist/` should be unrelated/untracked.
- Do not modify `xq_app_dist/`.
- Use existing `build_gui_wt.bat` / `run_xq.bat` paths unless a verified path problem is found.

## Steps

1. **Add i18n test and diagnostics**
   - Add/extend a Qt test that initializes `xq_resources` and asserts `.qm` load plus a known Chinese translation.
   - Add runtime diagnostics for missing resource, failed load, empty translator, and failed key translation.
   - Verify language menu still switches Chinese -> English -> Chinese.

2. **Set VTK/Qt surface format and stabilize initial 3D paint**
   - Set `QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat())` before `QApplication` construction if supported by the current include/link graph.
   - Add a safe initial render path for `XQRenderWidget` once it has a valid size/show state.
   - Keep renderer ownership and scene selection flow unchanged.

3. **Coalesce MPR resize rendering**
   - Add dirty-axis state and a single-shot timer inside `XQMprView::Impl`.
   - On frame resize, reposition labels immediately and schedule the affected axis.
   - Render only dirty axes when the timer fires.
   - Preserve immediate render behavior for slice sliders, crosshair toggles, and seed picking.

4. **Polish current QSS and local UI details**
   - Adjust `resources/xq.qss` and small app/view properties only.
   - Prioritize visible rough edges from the true app: toolbar, docks, tree, status bar, panels, MPR/3D frames, disabled/hover states.
   - Do not add a new theme dependency.

5. **Validate**
   - Build Release.
   - Run focused tests first, then full ctest.
   - Run `run_xq.bat` for true-window validation:
     - default Chinese;
     - language switch;
     - resize drag;
     - 3D cell visible;
     - overall UI polish.

## Validation Commands

```bat
cd /d C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ
build_gui_wt.bat
```

```bat
cd /d C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui
set QT_QPA_PLATFORM=offscreen
"C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" -C Release --output-on-failure
```

```bat
cd /d C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ
run_xq.bat
```

## Review Gates

- Do not mark complete until full ctest passes and true app behavior is checked.
- If a test fails, fix the root cause; do not skip tests or weaken assertions.
- If a product choice blocks AC4, stop after technical fixes and ask whether to keep minimal QSS or approve a larger theme-library pass.

## Final Validation Record

- Updated on 2026-07-01 in `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`, branch `feat/gui-mitk-layout`.
- `git diff --check`: passed after final dock / toolbar tightening.
- Release build via `cmd /c build_gui_wt.bat`: passed.
- Focused offscreen tests passed: `test_i18n_resources`, `test_main_window`, `test_scene_renderer`, `test_image_viewer` (4/4).
- Full offscreen `ctest -C Release --output-on-failure`: 51/51 passed.
- True Windows run via `run_xq.bat`: launched `build_gui\xq_app.exe` successfully.
- Final DPI-aware screenshot: `C:\Users\OCEAN\AppData\Local\Temp\codex-xq-final-layout-2026-07-01_19-11-40.png`.
- Screenshot check against the user-provided MITK-style reference:
  - default Chinese UI is visible;
  - `Data Manager` and `Image Navigator` are stacked in one aligned left column;
  - `Standard Display` is the central tab;
  - workflow `Stage` dock is hidden by default and opens only from stage toolbar actions;
  - MPR/3D 2x2 grid is equal-sized: pixel scan found top red/green cells at ~605/604 px wide with matching row heights;
  - bottom-right 3D cell paints a dark VTK-backed area;
  - toolbar no longer truncates the visible Chinese labels at 1724x1040;
  - fake demo scene nodes are absent;
  - reference-layout placeholder controls remain visible where needed for MITK-like layout fidelity, but disabled until backed by real image data / real workflow inputs.
- UI polish check: restrained cold-gray desktop palette, compact 1px borders, 4px control radii, monochrome line icons, dense professional toolbar/docks, and no web-card/gradient/glass styling.

## Rollback Points

- i18n diagnostics/test can remain even if translator implementation changes.
- Resize scheduler is isolated to `XQMprView`; rollback should not affect main window workflow.
- 3D init changes are isolated to `main.cpp` / `XQRenderWidget`.
- QSS changes are isolated to resources and should not mix with behavioral fixes.
