#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataManagementService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QLabel>
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

QString StatusLabelObjectName(const QString& workflowId)
{
    return QStringLiteral("xqWorkflowContextStatus_%1").arg(workflowId);
}

QLabel* FindStatusLabel(xq::presentation::MainWindow& window,
                        const QString& workflowId)
{
    return window.findChild<QLabel*>(StatusLabelObjectName(workflowId));
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
        if (Expect(FindStatusLabel(window, workflowId) != nullptr,
                   "data-dependent workflow page should expose a context status label"))
        {
            delete context;
            return 1;
        }
    }
    if (Expect(FindStatusLabel(window, QStringLiteral("project")) == nullptr,
               "project page should not expose a workflow context status label"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindStatusLabel(window, QStringLiteral("data")) == nullptr,
               "data page should not expose a workflow context status label"))
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
    auto* imageStatus =
        FindStatusLabel(window, QStringLiteral("image-preprocessing"));
    if (Expect(imageStatus->text() ==
                   QStringLiteral("Select compatible data to continue."),
               "image preprocessing without data should show missing-input status"))
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
    if (Expect(imageStatus->text() == QStringLiteral("Using CTA Image."),
               "compatible image should update image preprocessing status"))
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
    auto* meshingStatus = FindStatusLabel(window, QStringLiteral("meshing"));
    if (Expect(meshingStatus->text() ==
                   QStringLiteral("Selected data is not compatible with Meshing."),
               "image data should show incompatible meshing status"))
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
    if (Expect(meshingStatus->text() == QStringLiteral("Using Aorta Model."),
               "compatible model should update meshing status"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataManagement()->RenameEntry(
                   QStringLiteral("model-001"),
                   QStringLiteral("Renamed Model"),
                   &errorMessage),
               "selected model rename should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(meshingStatus->text() == QStringLiteral("Using Renamed Model."),
               "renaming selected data should refresh active workflow status"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
