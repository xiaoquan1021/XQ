# Research: Qt / VTK resize performance and 3D initialization

## Sources

- QWidget resize event docs: https://doc.qt.io/qt-6/qwidget.html#resizeEvent
- QTimer docs: https://doc.qt.io/qt-6/qtimer.html
- VTK `QVTKOpenGLNativeWidget`: https://vtk.org/doc/nightly/html/classQVTKOpenGLNativeWidget.html

## Relevant External Guidance

- Qt can deliver many resize events while a window is being resized.
- A restartable single-shot `QTimer` is a standard Qt mechanism for coalescing bursts of events and running work after the last event in the burst.
- `QVTKOpenGLNativeWidget` is intended for `vtkGenericOpenGLRenderWindow` and VTK documents setting the default `QSurfaceFormat` before creating the Qt application.

## Repository Evidence

- `src/visualization/XQMprView.cpp` renders each 2D slice through VTK offscreen and copies the RGBA result into a `QLabel`.
- `XQMprView::eventFilter` handles `QEvent::Resize` by repositioning labels and immediately calling `renderAxisForResizedFrame`.
- `renderAxisForResizedFrame` renders one axis for the resized frame. During normal window resize, axial/sagittal/coronal frames can each emit resize events, creating repeated synchronous VTK render work on the UI thread.
- `src/visualization/XQRenderWidget.cpp` wraps `QVTKOpenGLNativeWidget` and `vtkGenericOpenGLRenderWindow`.
- `src/app/main.cpp` currently creates `QApplication` without first setting `QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat())`.

## Design Implications

- Split resize handling:
  - immediate path: reposition axis labels and overlays only;
  - deferred path: coalesce dirty axes and render the latest size after a short single-shot timer.
- Avoid calling VTK offscreen render directly inside the resize event path.
- Keep the existing pixmap visible during drag. Optional: scale or center the existing pixmap cheaply until the deferred render completes.
- Add VTK/Qt initialization before `QApplication` in `main.cpp` if build dependencies allow it.
- Trigger a first `XQRenderWidget::render()` after the widget has a valid size/show state, not only after scene selection.

## Validation Ideas

- Add a focused test or instrumentation-friendly path to prove repeated resize events collapse to fewer render requests.
- Keep full `ctest` as required, but final acceptance must include manual true-window drag and 3D cell inspection through `run_xq.bat`.
