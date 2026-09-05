#include "visualization/XQVolumeViewWidget.h"

#include "visualization/XQRenderScene.h"

#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QSurfaceFormat>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

namespace xq {
namespace {

constexpr int kMinimumRenderCellSize = 96;

// True when Qt is running on the offscreen platform (headless ctest). That plugin
// provides no usable OpenGL context, so QVTKOpenGLNativeWidget cannot create one
// and would crash inside paintGL. In that case we skip mounting a VTK render
// window: the widget stays an inert placeholder (its object name / structure is
// still there for tests). On a real desktop platform this returns false and the
// render window is mounted normally.
bool isOffscreenPlatform()
{
    return QGuiApplication::platformName() == QLatin1String("offscreen");
}

} // namespace

void XQVolumeViewWidget::configureDefaultSurfaceFormat()
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
}

XQVolumeViewWidget::XQVolumeViewWidget(XQRenderScene* scene, QWidget* parent)
    : QWidget(parent)
    , scene_(scene)
    , vtkWidget_(new QVTKOpenGLNativeWidget(this))
{
    setMinimumSize(kMinimumRenderCellSize, kMinimumRenderCellSize);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vtkWidget_->setMinimumSize(kMinimumRenderCellSize, kMinimumRenderCellSize);
    vtkWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(vtkWidget_);

    // Mount the scene's Volume3D renderer. Default QVTK interaction is trackball
    // camera. vtkRendererHandle documents its void* as vtkRenderer*; casting it
    // here is the agreed sink-file contract. Skipped under the offscreen platform
    // (no GL context available -> QVTK paintGL would crash).
    if (!isOffscreenPlatform()) {
        vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
        vtkWidget_->setRenderWindow(renderWindow.Get());
        vtkRenderer* renderer =
            static_cast<vtkRenderer*>(scene_->vtkRendererHandle(ViewId::Volume3D));
        if (renderer != nullptr) {
            renderWindow->AddRenderer(renderer);
        }
    }

    // Empty-scene hint: hidden by default; the shell toggles it per scene state.
    emptyHint_ = new QLabel(tr("Nothing to render"), this);
    emptyHint_->setObjectName(QStringLiteral("xqEmptySceneHint"));
    emptyHint_->setAlignment(Qt::AlignCenter);
    emptyHint_->setStyleSheet(QStringLiteral(
        "QLabel { color: #5F6B7A; background: transparent; font-size: 13px; }"));
    emptyHint_->hide();
}

XQVolumeViewWidget::~XQVolumeViewWidget() = default;

void XQVolumeViewWidget::renderNow()
{
    if (!vtkWidget_->isValid()) {
        // No GL context yet (before first expose, or an offscreen QPA with no GL
        // as in headless ctest). Do not even queue a repaint: entering QVTK's
        // paintGL without a context crashes. Once the widget is exposed, QVTK
        // drives the first paint itself.
        return;
    }
    if (vtkWidget_->renderWindow() != nullptr) {
        vtkWidget_->renderWindow()->Render();
    }
    vtkWidget_->update();
}

void XQVolumeViewWidget::setEmptyHintVisible(bool visible)
{
    if (emptyHint_ != nullptr) {
        emptyHint_->setVisible(visible);
        if (visible) {
            emptyHint_->raise();
        }
    }
}

void XQVolumeViewWidget::changeEvent(QEvent* event)
{
    if (event != nullptr && event->type() == QEvent::LanguageChange
        && emptyHint_ != nullptr) {
        emptyHint_->setText(tr("Nothing to render"));
    }
    QWidget::changeEvent(event);
}

void XQVolumeViewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (emptyHint_ != nullptr) {
        emptyHint_->setGeometry(rect());
    }
}

} // namespace xq
