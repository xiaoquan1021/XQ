#include "xq_ImagePreprocessingWorkflowActionHandler.h"

#include "xq_ImagePreprocessingApplicationCommitService.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataNodeRegistryService.h"
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

} // namespace

bool RegisterImagePreprocessingWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    const ImagePreprocessingWorkflowActionOptions& options,
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
        [&context, options, operationId](
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
    const auto handler =
        [&context](const xq::core::WorkflowContextSnapshot& snapshot,
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
            SetMessage(taskMessage, result.Message);
            return result.Succeeded;
        };

    return context.WorkflowActions()->RegisterHandler(
        QStringLiteral("image-preprocessing"),
        handler,
        message);
}

} // namespace xq::infrastructure
