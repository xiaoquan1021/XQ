#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_TaskRunner.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"

#include <QCoreApplication>
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
    const int registered =
        xq::domain::RegisterDefaultWorkflowActionHandlers(
            *context->WorkflowActions());

    const QStringList dataDependentWorkflowIds = {
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

    if (Expect(registered == dataDependentWorkflowIds.size(),
               "domain registrar should report registered handler count"))
    {
        delete context;
        return 1;
    }

    for (const auto& workflowId : dataDependentWorkflowIds)
    {
        if (Expect(context->WorkflowActions()->HasHandler(workflowId),
                   "data-dependent workflow should have a domain handler"))
        {
            delete context;
            return 1;
        }
    }

    for (const auto& workflowId :
         {QStringLiteral("project"),
          QStringLiteral("data"),
          QStringLiteral("python-api")})
    {
        if (Expect(!context->WorkflowActions()->HasHandler(workflowId),
                   "non-data workflow should not have a domain handler"))
        {
            delete context;
            return 1;
        }
    }

    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "image preprocessing workflow should be selectable"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto importResult =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA Image"),
                                           xq::core::DataWorkflowRole::Image),
                                       &errorMessage);
    if (Expect(importResult.Succeeded, "image import should succeed"))
    {
        delete context;
        return 1;
    }

    QString actionMessage;
    const int historyBeforeRun = context->Tasks()->History().size();
    if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                   &actionMessage),
               "registered image preprocessing handler should run"))
    {
        delete context;
        return 1;
    }

    const auto historyAfterRun = context->Tasks()->History();
    if (Expect(historyAfterRun.size() == historyBeforeRun + 1,
               "domain handler run should append one task"))
    {
        delete context;
        return 1;
    }
    if (Expect(historyAfterRun.back().Name ==
                   QStringLiteral("Run Image Preprocessing"),
               "domain handler task should keep workflow task name"))
    {
        delete context;
        return 1;
    }
    if (Expect(historyAfterRun.back().Message ==
                   QStringLiteral("Image Preprocessing domain workflow accepted CTA Image."),
               "domain handler should provide task message"))
    {
        delete context;
        return 1;
    }

    auto* operationContext = xq::core::ApplicationContext::CreateDefault();
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *operationContext->WorkflowActions(),
        operationContext->WorkflowOperations());
    const auto segmentation2dOperations =
        operationContext->WorkflowOperations()->OperationsForWorkflow(
            QStringLiteral("segmentation-2d"));
    if (Expect(segmentation2dOperations.size() == 3,
               "2D segmentation should register workflow operations"))
    {
        delete operationContext;
        delete context;
        return 1;
    }
    if (Expect(segmentation2dOperations.at(0).Id ==
                       QStringLiteral("threshold-contour") &&
                   segmentation2dOperations.at(0).Parameters.size() == 2,
               "2D threshold contour should expose threshold parameters"))
    {
        delete operationContext;
        delete context;
        return 1;
    }
    const auto segmentation3dOperations =
        operationContext->WorkflowOperations()->OperationsForWorkflow(
            QStringLiteral("segmentation-3d"));
    if (Expect(segmentation3dOperations.size() == 3,
               "3D segmentation should register workflow operations"))
    {
        delete operationContext;
        delete context;
        return 1;
    }
    if (Expect(segmentation3dOperations.at(1).Id ==
                       QStringLiteral("region-growing") &&
                   segmentation3dOperations.at(1).Parameters.size() == 3,
               "3D region growing should expose seed and threshold parameters"))
    {
        delete operationContext;
        delete context;
        return 1;
    }

    const auto segmentationImport =
        operationContext->DataImports()->Import(MakeImport(
                                                    QStringLiteral("seg-image"),
                                                    QStringLiteral("Seg CTA"),
                                                    xq::core::DataWorkflowRole::Image),
                                                &errorMessage);
    if (Expect(segmentationImport.Succeeded,
               "segmentation image import should succeed"))
    {
        delete operationContext;
        delete context;
        return 1;
    }
    if (Expect(operationContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("segmentation-2d")),
               "2D segmentation workflow should be selectable"))
    {
        delete operationContext;
        delete context;
        return 1;
    }
    if (Expect(operationContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("segmentation-2d"),
                   QStringLiteral("loft-profiles"),
                   &errorMessage),
               "2D segmentation operation should be selectable"))
    {
        delete operationContext;
        delete context;
        return 1;
    }
    if (Expect(operationContext->WorkflowActions()->RunActiveWorkflowAction(
                   &actionMessage),
               "2D segmentation handler should run selected operation"))
    {
        delete operationContext;
        delete context;
        return 1;
    }
    if (Expect(actionMessage ==
                   QStringLiteral("Loft Profiles segmentation operation accepted Seg CTA."),
               "2D segmentation handler should report selected operation"))
    {
        delete operationContext;
        delete context;
        return 1;
    }

    if (Expect(operationContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("segmentation-3d")),
               "3D segmentation workflow should be selectable"))
    {
        delete operationContext;
        delete context;
        return 1;
    }
    if (Expect(operationContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("segmentation-3d"),
                   QStringLiteral("region-growing"),
                   &errorMessage),
               "3D segmentation operation should be selectable"))
    {
        delete operationContext;
        delete context;
        return 1;
    }
    if (Expect(operationContext->WorkflowActions()->RunActiveWorkflowAction(
                   &actionMessage),
               "3D segmentation handler should run selected operation"))
    {
        delete operationContext;
        delete context;
        return 1;
    }
    if (Expect(actionMessage ==
                   QStringLiteral("Region Growing segmentation operation accepted Seg CTA."),
               "3D segmentation handler should report selected operation"))
    {
        delete operationContext;
        delete context;
        return 1;
    }
    delete operationContext;

    auto* plainContext = xq::core::ApplicationContext::CreateDefault();
    if (Expect(plainContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "plain image preprocessing workflow should be selectable"))
    {
        delete plainContext;
        delete context;
        return 1;
    }
    const auto plainImport =
        plainContext->DataImports()->Import(MakeImport(
                                                QStringLiteral("plain-image"),
                                                QStringLiteral("Plain CTA"),
                                                xq::core::DataWorkflowRole::Image),
                                            &errorMessage);
    if (Expect(plainImport.Succeeded, "plain image import should succeed"))
    {
        delete plainContext;
        delete context;
        return 1;
    }
    if (Expect(plainContext->WorkflowActions()->RunActiveWorkflowAction(
                   &actionMessage),
               "plain workflow action should still run placeholder"))
    {
        delete plainContext;
        delete context;
        return 1;
    }
    if (Expect(actionMessage ==
                   QStringLiteral("Image Preprocessing action requested for Plain CTA."),
               "plain context should preserve placeholder message"))
    {
        delete plainContext;
        delete context;
        return 1;
    }

    delete plainContext;
    delete context;
    return 0;
}
