#include "visualization/XQRenderWidget.h"

#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

namespace xq {

XQRenderWidget::XQRenderWidget(QWidget* parent)
    : QWidget(parent)
    , vtkWidget_(new QVTKOpenGLNativeWidget(this))
    , renderer_()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(vtkWidget_);

    // Give the QVTK widget a render window and mount our renderer on it. The
    // renderer is exposed as an opaque void* (XQSceneRenderer.h is VTK-free);
    // this same-sink .cpp casts it back to vtkRenderer*. Casting an opaque
    // handle that the renderer documents as vtkRenderer* is the agreed internal
    // contract between the two sink files.
    vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
    vtkWidget_->setRenderWindow(renderWindow.Get());

    vtkRenderer* renderer = static_cast<vtkRenderer*>(renderer_.vtkRendererHandle());
    renderWindow->AddRenderer(renderer);
}

XQRenderWidget::~XQRenderWidget() = default;

XQSceneRenderer& XQRenderWidget::renderer()
{
    return renderer_;
}

void XQRenderWidget::render()
{
    if (vtkWidget_->renderWindow() != nullptr) {
        vtkWidget_->renderWindow()->Render();
    }
    vtkWidget_->update();
}

} // namespace xq
