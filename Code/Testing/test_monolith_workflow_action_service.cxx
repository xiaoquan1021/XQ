#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_TaskRunner.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowContextService.h"
#include "Core/xq_WorkflowSelectionService.h"

#include <QCoreApplication>

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
    QCoreApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    auto* workflowActions = context->WorkflowActions();

    if (Expect(workflowActions != nullptr,
               "ApplicationContext should expose WorkflowActionService"))
    {
        delete context;
        return 1;
    }

    QString message;
    QString errorMessage;
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "image preprocessing workflow should be selectable"))
    {
        delete context;
        return 1;
    }

    if (Expect(!workflowActions->RequestActiveWorkflowAction(&message),
               "image preprocessing should reject missing selected data"))
    {
        delete context;
        return 1;
    }
    if (Expect(message ==
                   QStringLiteral("Select compatible data before running Image Preprocessing."),
               "missing data rejection should include workflow title"))
    {
        delete context;
        return 1;
    }
    if (Expect(!workflowActions->RunActiveWorkflowAction(&message),
               "running image preprocessing should reject missing selected data"))
    {
        delete context;
        return 1;
    }
    if (Expect(message ==
                   QStringLiteral("Select compatible data before running Image Preprocessing."),
               "missing data run rejection should include workflow title"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Tasks()->History().isEmpty(),
               "rejected workflow action should not create a task"))
    {
        delete context;
        return 1;
    }

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
    if (Expect(workflowActions->RequestActiveWorkflowAction(&message),
               "image preprocessing should accept selected image data"))
    {
        delete context;
        return 1;
    }
    if (Expect(message ==
                   QStringLiteral("Image Preprocessing action requested for CTA Image."),
               "accepted image preprocessing action should include selected data"))
    {
        delete context;
        return 1;
    }
    const int historyBeforeImageRun = context->Tasks()->History().size();
    if (Expect(workflowActions->RunActiveWorkflowAction(&message),
               "running image preprocessing should accept selected image data"))
    {
        delete context;
        return 1;
    }
    const auto historyAfterImageRun = context->Tasks()->History();
    if (Expect(historyAfterImageRun.size() == historyBeforeImageRun + 1,
               "accepted workflow action should create one task"))
    {
        delete context;
        return 1;
    }
    const auto imageTask = historyAfterImageRun.back();
    if (Expect(imageTask.Name == QStringLiteral("Run Image Preprocessing"),
               "workflow action task should include workflow title"))
    {
        delete context;
        return 1;
    }
    if (Expect(imageTask.Succeeded,
               "placeholder workflow action task should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(imageTask.Message ==
                   QStringLiteral("Image Preprocessing action requested for CTA Image."),
               "workflow action task should keep request message"))
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
    if (Expect(!workflowActions->RequestActiveWorkflowAction(&message),
               "meshing should reject selected image data"))
    {
        delete context;
        return 1;
    }
    if (Expect(message ==
                   QStringLiteral("Select compatible data before running Meshing."),
               "incompatible data rejection should include meshing title"))
    {
        delete context;
        return 1;
    }
    const int historyBeforeMeshingReject = context->Tasks()->History().size();
    if (Expect(!workflowActions->RunActiveWorkflowAction(&message),
               "running meshing should reject selected image data"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Tasks()->History().size() ==
                   historyBeforeMeshingReject,
               "rejected meshing action should not create a task"))
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
    if (Expect(workflowActions->RequestActiveWorkflowAction(&message),
               "meshing should accept selected model data"))
    {
        delete context;
        return 1;
    }
    if (Expect(message ==
                   QStringLiteral("Meshing action requested for Aorta Model."),
               "accepted meshing action should include selected model data"))
    {
        delete context;
        return 1;
    }
    const int historyBeforeMeshingRun = context->Tasks()->History().size();
    if (Expect(workflowActions->RunActiveWorkflowAction(&message),
               "running meshing should accept selected model data"))
    {
        delete context;
        return 1;
    }
    const auto historyAfterMeshingRun = context->Tasks()->History();
    if (Expect(historyAfterMeshingRun.size() == historyBeforeMeshingRun + 1,
               "accepted meshing action should create one task"))
    {
        delete context;
        return 1;
    }
    if (Expect(historyAfterMeshingRun.back().Name ==
                   QStringLiteral("Run Meshing"),
               "meshing action task should include workflow title"))
    {
        delete context;
        return 1;
    }

    auto* handlerContext = xq::core::ApplicationContext::CreateDefault();
    auto* handlerActions = handlerContext->WorkflowActions();
    if (Expect(!handlerActions->RegisterHandler(
                   QStringLiteral("missing-workflow"),
                   [](const xq::core::WorkflowContextSnapshot&, QString*) {
                       return true;
                   },
                   &message),
               "unknown workflow handler registration should be rejected"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }
    if (Expect(!handlerActions->RegisterHandler(
                   QStringLiteral("image-preprocessing"),
                   xq::core::WorkflowActionService::WorkflowActionHandler(),
                   &message),
               "empty workflow handler registration should be rejected"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }

    int handlerCalls = 0;
    QString handlerWorkflowId;
    QString handlerSelectedData;
    const bool handlerRegistered = handlerActions->RegisterHandler(
        QStringLiteral("image-preprocessing"),
        [&handlerCalls,
         &handlerWorkflowId,
         &handlerSelectedData](
            const xq::core::WorkflowContextSnapshot& snapshot,
            QString* taskMessage) {
            ++handlerCalls;
            handlerWorkflowId = snapshot.WorkflowId;
            handlerSelectedData = snapshot.SelectedDataDisplayName;
            if (taskMessage)
            {
                *taskMessage =
                    QStringLiteral("Custom preprocessing completed for %1.")
                        .arg(snapshot.SelectedDataDisplayName);
            }
            return true;
        },
        &message);
    if (Expect(handlerRegistered,
               "valid workflow handler registration should succeed"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }
    if (Expect(handlerActions->HasHandler(
                   QStringLiteral("image-preprocessing")),
               "registered workflow handler should be discoverable"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }

    if (Expect(handlerContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "handler image preprocessing workflow should be selectable"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }
    const auto handlerImageImport =
        handlerContext->DataImports()->Import(MakeImport(
                                                  QStringLiteral("handler-image"),
                                                  QStringLiteral("Handler CTA"),
                                                  xq::core::DataWorkflowRole::Image),
                                              &errorMessage);
    if (Expect(handlerImageImport.Succeeded,
               "handler image import should succeed"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }

    const int handlerHistoryBeforeRun =
        handlerContext->Tasks()->History().size();
    if (Expect(handlerActions->RunActiveWorkflowAction(&message),
               "registered workflow handler action should be accepted"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }
    const auto handlerHistoryAfterRun =
        handlerContext->Tasks()->History();
    if (Expect(handlerCalls == 1,
               "registered workflow handler should be called once"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }
    if (Expect(handlerWorkflowId == QStringLiteral("image-preprocessing"),
               "handler should receive active workflow id"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }
    if (Expect(handlerSelectedData == QStringLiteral("Handler CTA"),
               "handler should receive selected data display name"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }
    if (Expect(handlerHistoryAfterRun.size() == handlerHistoryBeforeRun + 1,
               "registered handler run should create one task"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }
    if (Expect(handlerHistoryAfterRun.back().Message ==
                   QStringLiteral("Custom preprocessing completed for Handler CTA."),
               "registered handler task should use handler message"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }

    handlerContext->DataSelection()->SelectCatalogEntry(QStringLiteral("handler-image"));
    if (Expect(handlerContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("path")),
               "path workflow should be selectable"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }
    if (Expect(handlerActions->RunActiveWorkflowAction(&message),
               "workflow without registered handler should preserve placeholder action"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }
    if (Expect(message ==
                   QStringLiteral("Path action requested for Handler CTA."),
               "unregistered workflow should keep placeholder message"))
    {
        delete handlerContext;
        delete context;
        return 1;
    }

    delete handlerContext;
    delete context;
    return 0;
}
