#ifndef XQ_VISUALIZATION_VOLUME_VIEW_WIDGET_H
#define XQ_VISUALIZATION_VOLUME_VIEW_WIDGET_H

#include <QWidget>

class QVTKOpenGLNativeWidget;
class QEvent;
class QResizeEvent;
class QLabel;

namespace xq {

class XQRenderScene;

// Interactive 3D view: a QWidget wrapping a QVTKOpenGLNativeWidget whose render
// window mounts XQRenderScene's Volume3D renderer (three orthogonal slice planes
// + all resident node actors). Default trackball camera interaction (the QVTK
// default). The scene owns all actors; this widget only mounts the renderer and
// repaints.
class XQVolumeViewWidget : public QWidget {
    Q_OBJECT

public:
    // Must be called before QApplication is constructed so VTK and Qt agree on
    // the OpenGL surface format used by QVTKOpenGLNativeWidget. This is the one
    // entry point main() / tests call.
    static void configureDefaultSurfaceFormat();

    // scene: the resident render scene (borrowed; must outlive this widget).
    explicit XQVolumeViewWidget(XQRenderScene* scene, QWidget* parent = nullptr);
    ~XQVolumeViewWidget() override;

    XQVolumeViewWidget(const XQVolumeViewWidget&) = delete;
    XQVolumeViewWidget& operator=(const XQVolumeViewWidget&) = delete;

    // Repaints the 3D view after the scene changed.
    void renderNow();

    // Shows/hides the centered "nothing to render" hint. Hidden by default.
    void setEmptyHintVisible(bool visible);

protected:
    void changeEvent(QEvent* event) override; // retranslates the empty hint
    void resizeEvent(QResizeEvent* event) override;

private:
    XQRenderScene* scene_;
    QVTKOpenGLNativeWidget* vtkWidget_;
    QLabel* emptyHint_ = nullptr;
};

} // namespace xq

#endif // XQ_VISUALIZATION_VOLUME_VIEW_WIDGET_H
