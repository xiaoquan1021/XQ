#ifndef XQ_VISUALIZATION_RENDER_WIDGET_H
#define XQ_VISUALIZATION_RENDER_WIDGET_H

#include "visualization/XQSceneRenderer.h"

#include <QWidget>

class QVTKOpenGLNativeWidget;

namespace xq {

// Interactive 3D view: a QWidget wrapping a QVTKOpenGLNativeWidget whose render
// window is driven by an owned XQSceneRenderer. The main window assembles actors
// through renderer() (clear() + add*) and calls render() to repaint.
//
// Header stays VTK-free: QVTKOpenGLNativeWidget (a Qt+VTK type) is only
// forward-declared and held as a pointer; the renderer's vtkRenderer is mounted
// through XQSceneRenderer's opaque void* handle inside the .cpp. The widget owns
// the renderer so callers get one coherent scene per view.
class XQRenderWidget : public QWidget {
    Q_OBJECT

public:
    explicit XQRenderWidget(QWidget* parent = nullptr);
    ~XQRenderWidget() override;

    XQRenderWidget(const XQRenderWidget&) = delete;
    XQRenderWidget& operator=(const XQRenderWidget&) = delete;

    // The scene renderer mounted on this widget's render window. Drive it with
    // clear() + add*(), then call render() to repaint.
    XQSceneRenderer& renderer();

    // Repaints the interactive view after the scene changed.
    void render();

private:
    QVTKOpenGLNativeWidget* vtkWidget_;
    XQSceneRenderer renderer_;
};

} // namespace xq

#endif // XQ_VISUALIZATION_RENDER_WIDGET_H
