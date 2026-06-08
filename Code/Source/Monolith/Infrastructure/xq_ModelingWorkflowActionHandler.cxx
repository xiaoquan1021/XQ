#include "xq_ModelingWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_RenderRefreshService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <xq_ModelPipeline.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ProfileGroup.h>

#include <algorithm>

namespace xq::infrastructure
{

namespace
{

constexpr const char* kModelingWorkflowId = "modeling";
constexpr const char* kBuildSolidModelOperationId = "build-solid-model";
constexpr const char* kModelsFolderId = "models";
constexpr const char* kModelsFolderTitle = "Models";

void SetMessage(QString* message, const QString& value)
{
    if (message)
        *message = value;
}

QString SelectedDataLabel(
    const xq::core::WorkflowContextSnapshot& snapshot)
{
    const QString displayName = snapshot.SelectedDataDisplayName.trimmed();
    if (!displayName.isEmpty())
        return displayName;

    return snapshot.SelectedCatalogEntryId;
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

bool RunPlaceholderModelingOperation(
    xq::core::WorkflowOperationService* operations,
    const xq::core::WorkflowContextSnapshot& snapshot,
    QString* message)
{
    const QString operationId =
        operations ? operations->SelectedOperationId(snapshot.WorkflowId)
                   : QString();
    const QString operationTitle =
        OperationTitle(operations, snapshot.WorkflowId, operationId);
    if (operationTitle.trimmed().isEmpty())
    {
        SetMessage(message,
                   QStringLiteral("%1 domain workflow accepted %2.")
                       .arg(snapshot.WorkflowTitle,
                            SelectedDataLabel(snapshot)));
        return true;
    }

    SetMessage(message,
               QStringLiteral("%1 modeling operation accepted %2.")
                   .arg(operationTitle,
                        SelectedDataLabel(snapshot)));
    return true;
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
    return QStringLiteral("xq://generated/model/%1")
        .arg(catalogEntryId.trimmed());
}

mitk::DataNode::Pointer ResolveSegmentationNode(
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

    return QStringLiteral("Modeling operation failed.");
}

QString ResultDisplayName(const xq_CreateModelResult& result)
{
    if (result.node.IsNotNull())
    {
        const QString nodeName =
            QString::fromStdString(result.node->GetName()).trimmed();
        if (!nodeName.isEmpty())
            return nodeName;
    }

    return QStringLiteral("Model result");
}

QString PreflightCommitTarget(xq::core::ApplicationContext& context,
                              const QString& entryId)
{
    if (!context.DataCatalog())
        return QStringLiteral("Model data catalog is required.");
    if (!context.DataHierarchy())
        return QStringLiteral("Model data hierarchy is required.");
    if (entryId.trimmed().isEmpty())
        return QStringLiteral("Model result catalog entry id is required.");
    if (context.DataCatalog()->FindById(entryId))
        return QStringLiteral("Duplicate data id.");

    const auto* modelsFolder =
        context.DataHierarchy()->FindNode(QString::fromLatin1(kModelsFolderId));
    if (modelsFolder &&
        modelsFolder->Kind != xq::core::DataHierarchyNodeKind::Folder)
    {
        return QStringLiteral("Duplicate hierarchy node id.");
    }

    if (context.DataHierarchy()->FindNode(HierarchyNodeId(entryId)))
        return QStringLiteral("Duplicate hierarchy node id.");

    return {};
}

bool CommitModelResult(xq::core::ApplicationContext& context,
                       const QString& entryId,
                       const xq_CreateModelResult& result,
                       QString* message)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = entryId;
    entry.DisplayName = ResultDisplayName(result);
    entry.SourcePath = VirtualSourcePath(entryId);
    entry.Modality = QStringLiteral("Generated");
    entry.WorkflowRole = xq::core::DataWorkflowRole::Model;

    if (!context.DataCatalog()->RegisterEntry(entry, message))
        return false;

    if (!context.DataHierarchy()->FindNode(QString::fromLatin1(kModelsFolderId)))
    {
        if (!context.DataHierarchy()->AddFolder(
                QString::fromLatin1(kModelsFolderId),
                context.DataHierarchy()->RootId(),
                QString::fromLatin1(kModelsFolderTitle),
                message))
        {
            return false;
        }
    }

    if (!context.DataHierarchy()->AddDataEntry(HierarchyNodeId(entryId),
                                               QString::fromLatin1(kModelsFolderId),
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
               QStringLiteral("Registered model result catalog entry."));
    return true;
}

bool RunBuildSolidModel(xq::core::ApplicationContext& context,
                        xq::core::RenderRefreshService* renderRefresh,
                        const xq::core::WorkflowContextSnapshot& snapshot,
                        const QString& operationId,
                        QString* message)
{
    const auto segmentationNode = ResolveSegmentationNode(context, snapshot);
    if (segmentationNode.IsNull() ||
        !xq::pipeline::HasStage(segmentationNode.GetPointer(),
                                xq::pipeline::Stage::ContourGroup))
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active segmentation node is required for modeling."));
        return false;
    }

    const QString entryId = ResultCatalogEntryId(snapshot, operationId);
    const QString preflightMessage = PreflightCommitTarget(context, entryId);
    if (!preflightMessage.isEmpty())
    {
        SetMessage(message, preflightMessage);
        return false;
    }

    const QVariantMap parameters =
        context.WorkflowOperations()
            ? context.WorkflowOperations()->ParameterValues(snapshot.WorkflowId,
                                                            operationId)
            : QVariantMap();

    xq_CreateModelRequest request;
    request.modelName =
        QStringLiteral("%1_model")
            .arg(QString::fromStdString(segmentationNode->GetName()).trimmed())
            .toStdString();
    request.modelType = "PolyData";
    request.numSampling =
        std::max(1,
                 parameters.value(QStringLiteral("sample-count"), 60).toInt());
    request.sourceContourGroupNames = {segmentationNode->GetName()};
    request.blendRadius =
        std::max(0.0,
                 parameters.value(QStringLiteral("blend-radius"), 0.0)
                     .toDouble());

    const auto result =
        xq_ModelPipelineService::CreateModel(context.DataStorage().GetPointer(),
                                             request);
    if (!result.ok)
    {
        SetMessage(message, FirstDiagnosticMessage(result));
        return false;
    }

    if (!CommitModelResult(context, entryId, result, message))
        return false;

    QString selectionMessage;
    context.DataSelection()->SelectCatalogEntry(entryId, &selectionMessage);
    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());

    return true;
}

} // namespace

bool RegisterDynamicModelingWorkflowActionHandler(
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
                               "Modeling operation id is required."));
                return false;
            }

            if (operationId !=
                QString::fromLatin1(kBuildSolidModelOperationId))
            {
                return RunPlaceholderModelingOperation(operations,
                                                       snapshot,
                                                       taskMessage);
            }

            return RunBuildSolidModel(context,
                                      renderRefresh,
                                      snapshot,
                                      operationId,
                                      taskMessage);
        };

    return context.WorkflowActions()->RegisterHandler(
        QString::fromLatin1(kModelingWorkflowId),
        handler,
        message);
}

} // namespace xq::infrastructure
