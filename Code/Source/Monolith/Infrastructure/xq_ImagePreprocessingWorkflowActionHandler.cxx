#include "xq_ImagePreprocessingWorkflowActionHandler.h"

#include "xq_ImagePreprocessingApplicationCommitService.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_RenderRefreshService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

namespace xq::infrastructure
{

namespace
{

void SetMessage(QString* message, const QString& value)
{
    if (message)
        *message = value;
}

QString ResultCatalogEntryId(
    const xq::core::WorkflowContextSnapshot& snapshot,
    const QString& operationId)
{
    return QStringLiteral("%1-%2").arg(snapshot.SelectedCatalogEntryId.trimmed(),
                                      operationId.trimmed());
}

QString ResultSuffix(const ImagePreprocessingWorkflowActionOptions& options)
{
    const QString suffix = options.ResultSuffix.trimmed();
    return suffix.isEmpty() ? options.OperationId.trimmed() : suffix;
}

mitk::DataNode::Pointer ResolveSourceNode(
    xq::core::ApplicationContext& context,
    const xq::core::WorkflowContextSnapshot& snapshot)
{
    if (auto* dataNodes = context.DataNodes())
    {
        auto node = dataNodes->FindNode(snapshot.SelectedCatalogEntryId);
        if (node.IsNotNull())
            return node;
    }

    return context.ActiveNode();
}

void ActivateResult(xq::core::ApplicationContext& context,
                    const ImagePreprocessingApplicationCommitResult& result,
                    xq::core::RenderRefreshService* renderRefresh)
{
    if (!result.Succeeded)
        return;

    if (!result.CatalogEntryId.trimmed().isEmpty())
    {
        QString selectionMessage;
        context.DataSelection()->SelectCatalogEntry(result.CatalogEntryId,
                                                    &selectionMessage);
    }

    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());
}

} // namespace

bool RegisterImagePreprocessingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    const ImagePreprocessingWorkflowActionOptions& options,
    QString* message)
{
    return RegisterImagePreprocessingWorkflowActionHandler(
        context,
        options,
        nullptr,
        message);
}

bool RegisterImagePreprocessingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    const ImagePreprocessingWorkflowActionOptions& options,
    xq::core::RenderRefreshService* renderRefresh,
    QString* message)
{
    const QString operationId = options.OperationId.trimmed();
    if (operationId.isEmpty())
    {
        SetMessage(message,
                   QStringLiteral(
                       "Image preprocessing operation id is required."));
        return false;
    }

    const auto handler =
        [&context, options, operationId, renderRefresh](
            const xq::core::WorkflowContextSnapshot& snapshot,
            QString* taskMessage) {
            const auto sourceNode = ResolveSourceNode(context, snapshot);
            if (sourceNode.IsNull())
            {
                SetMessage(taskMessage,
                           QStringLiteral(
                               "Active image node is required for image preprocessing."));
                return false;
            }

            ImagePreprocessingApplicationCommitRequest request;
            request.DataStorage = context.DataStorage();
            request.DataCatalog = context.DataCatalog();
            request.DataHierarchy = context.DataHierarchy();
            request.SourceNode = sourceNode;
            request.Context = snapshot;
            request.OperationId = operationId;
            request.Parameters = options.Parameters;
            request.ResultCatalogEntryId =
                ResultCatalogEntryId(snapshot, operationId);
            request.ResultSuffix = ResultSuffix(options);

            ImagePreprocessingApplicationCommitService service;
            const auto result = service.Run(request);
            ActivateResult(context, result, renderRefresh);
            SetMessage(taskMessage, result.Message);
            return result.Succeeded;
        };

    return context.WorkflowActions()->RegisterHandler(
        QStringLiteral("image-preprocessing"),
        handler,
        message);
}

bool RegisterDynamicImagePreprocessingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    QString* message)
{
    return RegisterDynamicImagePreprocessingWorkflowActionHandler(
        context,
        nullptr,
        message);
}

bool RegisterDynamicImagePreprocessingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    xq::core::RenderRefreshService* renderRefresh,
    QString* message)
{
    const auto handler =
        [&context, renderRefresh](
            const xq::core::WorkflowContextSnapshot& snapshot,
            QString* taskMessage) {
            auto* operations = context.WorkflowOperations();
            const QString operationId =
                operations->SelectedOperationId(snapshot.WorkflowId);
            if (operationId.trimmed().isEmpty())
            {
                SetMessage(taskMessage,
                           QStringLiteral(
                               "Image preprocessing operation id is required."));
                return false;
            }

            const auto sourceNode = ResolveSourceNode(context, snapshot);
            if (sourceNode.IsNull())
            {
                SetMessage(taskMessage,
                           QStringLiteral(
                               "Active image node is required for image preprocessing."));
                return false;
            }

            ImagePreprocessingApplicationCommitRequest request;
            request.DataStorage = context.DataStorage();
            request.DataCatalog = context.DataCatalog();
            request.DataHierarchy = context.DataHierarchy();
            request.SourceNode = sourceNode;
            request.Context = snapshot;
            request.OperationId = operationId;
            request.Parameters =
                operations->ParameterValues(snapshot.WorkflowId, operationId);
            request.ResultCatalogEntryId =
                ResultCatalogEntryId(snapshot, operationId);
            request.ResultSuffix = operationId;

            ImagePreprocessingApplicationCommitService service;
            const auto result = service.Run(request);
            ActivateResult(context, result, renderRefresh);
            SetMessage(taskMessage, result.Message);
            return result.Succeeded;
        };

    return context.WorkflowActions()->RegisterHandler(
        QStringLiteral("image-preprocessing"),
        handler,
        message);
}

} // namespace xq::infrastructure
