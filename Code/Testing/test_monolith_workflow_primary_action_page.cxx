#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QPushButton>
#include <QStringList>

#include <iostream>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

QString ActionObjectName(const QString& workflowId)
{
    return QStringLiteral("xqWorkflowPrimaryAction_%1").arg(workflowId);
}

QPushButton* FindActionButton(xq::presentation::MainWindow& window,
                              const QString& workflowId)
{
    return window.findChild<QPushButton*>(ActionObjectName(workflowId));
}

xq::core::DataImportRequest MakeImport(const QString& id,
                                       const QString& displayName,
                                       xq::core::DataWorkflowRole role)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = QStringLiteral("C:/studies/") + id;
    request.DisplayName = displayName;
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = role;
    return request;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    const QStringList dataDependentWorkflows = {
        QStringLiteral("image-preprocessing"),
        QStringLiteral("path"),
        QStringLiteral("segmentation-2d"),
        QStringLiteral("segmentation-3d"),
        QStringLiteral("modeling"),
        QStringLiteral("meshing"),
        QStringLiteral("flow-simulation"),
        QStringLiteral("rom-simulation"),
        QStringLiteral("multiphysics"),
    };

    for (const auto& workflowId : dataDependentWorkflows)
    {
        if (Expect(FindActionButton(window, workflowId) != nullptr,
                   "data-dependent workflow page should expose a primary action button"))
        {
            delete context;
            return 1;
        }
    }
    if (Expect(FindActionButton(window, QStringLiteral("project")) == nullptr,
               "project page should not expose a primary action button"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindActionButton(window, QStringLiteral("data")) == nullptr,
               "data page should not expose a primary action button"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindActionButton(window, QStringLiteral("python-api")) == nullptr,
               "python api page should not expose a primary action button"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "image preprocessing workflow should be selectable"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    auto* imageAction =
        FindActionButton(window, QStringLiteral("image-preprocessing"));
    if (Expect(!imageAction->isEnabled(),
               "image preprocessing action should be disabled without selected data"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto imageImport =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA Image"),
                                           xq::core::DataWorkflowRole::Image),
                                       &errorMessage);
    if (Expect(imageImport.Succeeded, "image import should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(imageAction->isEnabled(),
               "compatible image should enable image preprocessing action"))
    {
        delete context;
        return 1;
    }

    QStringList diagnostics;
    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&diagnostics](const QString& message) {
                         diagnostics.append(message);
                     });
    imageAction->click();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Image Preprocessing action requested for CTA Image.")),
               "enabled image preprocessing action should post a diagnostic"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("meshing")),
               "meshing workflow should be selectable"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    auto* meshingAction = FindActionButton(window, QStringLiteral("meshing"));
    if (Expect(!meshingAction->isEnabled(),
               "image data should disable meshing action"))
    {
        delete context;
        return 1;
    }

    const auto modelImport =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("model-001"),
                                           QStringLiteral("Aorta Model"),
                                           xq::core::DataWorkflowRole::Model),
                                       &errorMessage);
    if (Expect(modelImport.Succeeded, "model import should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(meshingAction->isEnabled(),
               "compatible model should enable meshing action"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
