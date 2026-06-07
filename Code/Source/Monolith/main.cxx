#include "Core/xq_ApplicationContext.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QSurfaceFormat>

#include <QmitkStdMultiWidget.h>
#include <QVTKOpenGLNativeWidget.h>

#include <memory>

int main(int argc, char* argv[])
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("XQ"));
    QCoreApplication::setOrganizationName(QStringLiteral("XQ"));

    std::unique_ptr<xq::core::ApplicationContext> context(
        xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context->WorkflowActions());

    xq::presentation::MainWindow window(*context);

    auto* renderHost = new QmitkStdMultiWidget();
    renderHost->setObjectName(QStringLiteral("xqMitkRenderHost"));
    renderHost->SetDataStorage(context->DataStorage().GetPointer());
    renderHost->InitializeMultiWidget();
    renderHost->AddPlanesToDataStorage();
    window.SetRenderHost(renderHost);

    window.show();

    return app.exec();
}
