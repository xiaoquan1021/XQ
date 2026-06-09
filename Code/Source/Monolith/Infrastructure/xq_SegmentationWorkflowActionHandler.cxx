#include "xq_SegmentationWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_RenderRefreshService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <xq_PipelineDataUtils.h>
#include <xq_SegmentationPipeline.h>

#include <QStringList>

namespace xq::infrastructure
{

namespace
{

constexpr const char* kSegmentation2DWorkflowId = "segmentation-2d";
constexpr const char* kManualContourOperationId = "manual-contour";
constexpr const char* kSegmentationsFolderId = "segmentations";
constexpr const char* kSegmentationsFolderTitle = "Segmentations";

void SetMessage(QString* message, const QString& value)
{
    if (message)
        *message = value;
}

QString OperationTitle(xq::core::WorkflowOperationService* operations,
                       const QString& workflowId,
                       const QString& operationId)
{
    if (!operations)
        return {};

    for (const auto& operation :
         operations->OperationsForWorkflow(workflowId))
    {
        if (operation.Id == operationId)
            return operation.Title;
    }

    return {};
}

bool RunUnsupportedSegmentationOperation(
    xq::core::WorkflowOperationService* operations,
    const xq::core::WorkflowContextSnapshot& snapshot,
    const QString& operationId,
    QString* message)
{
    const QString operationTitle =
        OperationTitle(operations, snapshot.WorkflowId, operationId);
    const QString displayOperation =
        operationTitle.trimmed().isEmpty() ? operationId : operationTitle;
    SetMessage(message,
               QStringLiteral(
                   "%1 is not wired to a native %2 runtime yet.")
                   .arg(displayOperation, snapshot.WorkflowTitle));
    return false;
}

QString ResultCatalogEntryId(
    const xq::core::WorkflowContextSnapshot& snapshot,
    const QString& operationId)
{
    return QStringLiteral("%1-%2").arg(snapshot.SelectedCatalogEntryId.trimmed(),
                                      operationId.trimmed());
}

QString HierarchyNodeId(const QString& catalogEntryId)
{
    return QStringLiteral("data-%1").arg(catalogEntryId.trimmed());
}

QString VirtualSourcePath(const QString& catalogEntryId)
{
    return QStringLiteral("xq://generated/segmentation/%1")
        .arg(catalogEntryId.trimmed());
}

mitk::DataNode::Pointer ResolvePathNode(
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

QString FirstDiagnosticMessage(const xq::pipeline::OperationStatus& status)
{
    if (!status.diagnostics.empty())
        return QString::fromStdString(status.diagnostics.front().message);

    return QStringLiteral("Segmentation operation failed.");
}

QString ResultDisplayName(const xq_CreateContourGroupResult& result)
{
    if (result.node.IsNotNull())
    {
        const QString nodeName =
            QString::fromStdString(result.node->GetName()).trimmed();
        if (!nodeName.isEmpty())
            return nodeName;
    }

    return QStringLiteral("Segmentation result");
}

QString PreflightCommitTarget(xq::core::ApplicationContext& context,
                              const QString& entryId)
{
    if (!context.DataCatalog())
        return QStringLiteral("Segmentation data catalog is required.");
    if (!context.DataHierarchy())
        return QStringLiteral("Segmentation data hierarchy is required.");
    if (entryId.trimmed().isEmpty())
        return QStringLiteral("Segmentation result catalog entry id is required.");
    if (context.DataCatalog()->FindById(entryId))
        return QStringLiteral("Duplicate data id.");

    const auto* segmentationsFolder = context.DataHierarchy()->FindNode(
        QString::fromLatin1(kSegmentationsFolderId));
    if (segmentationsFolder &&
        segmentationsFolder->Kind != xq::core::DataHierarchyNodeKind::Folder)
    {
        return QStringLiteral("Duplicate hierarchy node id.");
    }

    if (context.DataHierarchy()->FindNode(HierarchyNodeId(entryId)))
        return QStringLiteral("Duplicate hierarchy node id.");

    return {};
}

bool CommitSegmentationResult(xq::core::ApplicationContext& context,
                              const QString& entryId,
                              const xq_CreateContourGroupResult& result,
                              QString* message)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = entryId;
    entry.DisplayName = ResultDisplayName(result);
    entry.SourcePath = VirtualSourcePath(entryId);
    entry.Modality = QStringLiteral("Generated");
    entry.WorkflowRole = xq::core::DataWorkflowRole::Segmentation;

    if (!context.DataCatalog()->RegisterEntry(entry, message))
        return false;

    if (!context.DataHierarchy()->FindNode(
            QString::fromLatin1(kSegmentationsFolderId)))
    {
        if (!context.DataHierarchy()->AddFolder(
                QString::fromLatin1(kSegmentationsFolderId),
                context.DataHierarchy()->RootId(),
                QString::fromLatin1(kSegmentationsFolderTitle),
                message))
        {
            return false;
        }
    }

    if (!context.DataHierarchy()->AddDataEntry(
            HierarchyNodeId(entryId),
            QString::fromLatin1(kSegmentationsFolderId),
            entryId,
            entry.DisplayName,
            message))
    {
        return false;
    }

    if (context.DataNodes() &&
        !context.DataNodes()->BindNode(entryId, result.node, message))
    {
        return false;
    }

    SetMessage(message,
               QStringLiteral("Registered segmentation result catalog entry."));
    return true;
}

bool RunManualContour(xq::core::ApplicationContext& context,
                      xq::core::RenderRefreshService* renderRefresh,
                      const xq::core::WorkflowContextSnapshot& snapshot,
                      const QString& operationId,
                      QString* message)
{
    const auto pathNode = ResolvePathNode(context, snapshot);
    if (pathNode.IsNull() ||
        !xq::pipeline::IsPathNode(pathNode.GetPointer()))
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active path node is required for 2D segmentation."));
        return false;
    }

    const QString entryId = ResultCatalogEntryId(snapshot, operationId);
    const QString preflightMessage = PreflightCommitTarget(context, entryId);
    if (!preflightMessage.isEmpty())
    {
        SetMessage(message, preflightMessage);
        return false;
    }

    xq_CreateContourGroupRequest request;
    request.groupName =
        QStringLiteral("%1_contours")
            .arg(QString::fromStdString(pathNode->GetName()).trimmed())
            .toStdString();
    request.pathName = pathNode->GetName();

    const auto result =
        xq_SegmentationPipelineService::CreateContourGroup(
            context.DataStorage().GetPointer(),
            request);
    if (!result.ok)
    {
        SetMessage(message, FirstDiagnosticMessage(result));
        return false;
    }

    if (!CommitSegmentationResult(context, entryId, result, message))
        return false;

    QString selectionMessage;
    context.DataSelection()->SelectCatalogEntry(entryId, &selectionMessage);
    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());

    return true;
}

} // namespace

bool RegisterDynamicSegmentationWorkflowActionHandler(
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
                operations ? operations->SelectedOperationId(snapshot.WorkflowId)
                           : QString();
            if (operationId.trimmed().isEmpty())
            {
                SetMessage(taskMessage,
                           QStringLiteral(
                               "2D segmentation operation id is required."));
                return false;
            }

            if (snapshot.WorkflowId !=
                QString::fromLatin1(kSegmentation2DWorkflowId))
            {
                return RunUnsupportedSegmentationOperation(operations,
                                                           snapshot,
                                                           operationId,
                                                           taskMessage);
            }

            if (operationId !=
                QString::fromLatin1(kManualContourOperationId))
            {
                return RunUnsupportedSegmentationOperation(operations,
                                                           snapshot,
                                                           operationId,
                                                           taskMessage);
            }

            return RunManualContour(context,
                                    renderRefresh,
                                    snapshot,
                                    operationId,
                                    taskMessage);
        };

    bool registered = context.WorkflowActions()->RegisterHandler(
        QString::fromLatin1(kSegmentation2DWorkflowId),
        handler,
        message);
    registered = context.WorkflowActions()->RegisterHandler(
        QStringLiteral("segmentation-3d"),
        handler,
        message) && registered;
    return registered;
}

} // namespace xq::infrastructure
