#include "xq_WorkflowActionHandlers.h"

#include "xq_ImagePreprocessingWorkflowService.h"

#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <QString>
#include <QStringList>

#include <memory>

namespace xq::domain
{

namespace
{

QString SelectedDataLabel(
    const xq::core::WorkflowContextSnapshot& snapshot)
{
    const QString displayName = snapshot.SelectedDataDisplayName.trimmed();
    if (!displayName.isEmpty())
        return displayName;

    return snapshot.SelectedCatalogEntryId;
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateDefaultHandler()
{
    return [](const xq::core::WorkflowContextSnapshot& snapshot,
              QString* message) {
        if (message)
        {
            *message = QStringLiteral("%1 domain workflow accepted %2.")
                           .arg(snapshot.WorkflowTitle,
                                SelectedDataLabel(snapshot));
        }
        return true;
    };
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateImagePreprocessingHandler(xq::core::WorkflowOperationService* operations)
{
    auto service = std::make_shared<ImagePreprocessingWorkflowService>();
    return [service, operations](
               const xq::core::WorkflowContextSnapshot& snapshot,
               QString* message) {
        if (operations)
        {
            const QString operationId =
                operations->SelectedOperationId(snapshot.WorkflowId);
            if (!operationId.trimmed().isEmpty())
            {
                const auto operationResult =
                    service->RunOperation(snapshot, operationId);
                if (message)
                    *message = operationResult.Message;
                return operationResult.Succeeded;
            }
        }

        const auto result = service->Run(snapshot);
        if (message)
            *message = result.Message;
        return result.Succeeded;
    };
}

QVector<xq::core::WorkflowOperationDescriptor>
ImagePreprocessingOperations()
{
    ImagePreprocessingWorkflowService service;
    QVector<xq::core::WorkflowOperationDescriptor> operations;
    for (const auto& operation : service.Operations())
    {
        xq::core::WorkflowOperationDescriptor descriptor;
        descriptor.Id = operation.Id;
        descriptor.Title = operation.Title;
        operations.push_back(descriptor);
    }

    return operations;
}

} // namespace

int RegisterDefaultWorkflowActionHandlers(
    xq::core::WorkflowActionService& actions,
    xq::core::WorkflowOperationService* operations)
{
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

    int registered = 0;
    if (operations)
    {
        operations->RegisterOperations(QStringLiteral("image-preprocessing"),
                                       ImagePreprocessingOperations());
    }

    if (actions.RegisterHandler(QStringLiteral("image-preprocessing"),
                                CreateImagePreprocessingHandler(operations)))
    {
        ++registered;
    }

    for (const auto& workflowId : dataDependentWorkflowIds)
    {
        if (workflowId == QStringLiteral("image-preprocessing"))
            continue;

        if (actions.RegisterHandler(workflowId, CreateDefaultHandler()))
            ++registered;
    }

    return registered;
}

} // namespace xq::domain
