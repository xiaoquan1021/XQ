#include "xq_MeshingWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_RenderRefreshService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <xq_MeshPipeline.h>
#include <xq_PipelineDataUtils.h>

#include <algorithm>

namespace xq::infrastructure
{

namespace
{

constexpr const char* kMeshingWorkflowId = "meshing";
constexpr const char* kGenerateVolumeMeshOperationId = "generate-volume-mesh";
constexpr const char* kMeshesFolderId = "meshes";
constexpr const char* kMeshesFolderTitle = "Meshes";

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

bool RunPlaceholderMeshingOperation(
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
               QStringLiteral("%1 meshing operation accepted %2.")
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
    return QStringLiteral("xq://generated/mesh/%1")
        .arg(catalogEntryId.trimmed());
}

mitk::DataNode::Pointer ResolveModelNode(
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

    return QStringLiteral("Meshing operation failed.");
}

QString ResultDisplayName(const xq_MeshGenerationResult& result)
{
    if (result.node.IsNotNull())
    {
        const QString nodeName =
            QString::fromStdString(result.node->GetName()).trimmed();
        if (!nodeName.isEmpty())
            return nodeName;
    }

    return QStringLiteral("Mesh result");
}

QString PreflightCommitTarget(xq::core::ApplicationContext& context,
                              const QString& entryId)
{
    if (!context.DataCatalog())
        return QStringLiteral("Mesh data catalog is required.");
    if (!context.DataHierarchy())
        return QStringLiteral("Mesh data hierarchy is required.");
    if (entryId.trimmed().isEmpty())
        return QStringLiteral("Mesh result catalog entry id is required.");
    if (context.DataCatalog()->FindById(entryId))
        return QStringLiteral("Duplicate data id.");

    const auto* meshesFolder =
        context.DataHierarchy()->FindNode(QString::fromLatin1(kMeshesFolderId));
    if (meshesFolder &&
        meshesFolder->Kind != xq::core::DataHierarchyNodeKind::Folder)
    {
        return QStringLiteral("Duplicate hierarchy node id.");
    }

    if (context.DataHierarchy()->FindNode(HierarchyNodeId(entryId)))
        return QStringLiteral("Duplicate hierarchy node id.");

    return {};
}

bool CommitMeshResult(xq::core::ApplicationContext& context,
                      const QString& entryId,
                      const xq_MeshGenerationResult& result,
                      QString* message)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = entryId;
    entry.DisplayName = ResultDisplayName(result);
    entry.SourcePath = VirtualSourcePath(entryId);
    entry.Modality = QStringLiteral("Generated");
    entry.WorkflowRole = xq::core::DataWorkflowRole::Mesh;

    if (!context.DataCatalog()->RegisterEntry(entry, message))
        return false;

    if (!context.DataHierarchy()->FindNode(QString::fromLatin1(kMeshesFolderId)))
    {
        if (!context.DataHierarchy()->AddFolder(
                QString::fromLatin1(kMeshesFolderId),
                context.DataHierarchy()->RootId(),
                QString::fromLatin1(kMeshesFolderTitle),
                message))
        {
            return false;
        }
    }

    if (!context.DataHierarchy()->AddDataEntry(HierarchyNodeId(entryId),
                                               QString::fromLatin1(kMeshesFolderId),
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
               QStringLiteral("Registered mesh result catalog entry."));
    return true;
}

bool RunGenerateVolumeMesh(xq::core::ApplicationContext& context,
                           xq::core::RenderRefreshService* renderRefresh,
                           const xq::core::WorkflowContextSnapshot& snapshot,
                           const QString& operationId,
                           QString* message)
{
    const auto modelNode = ResolveModelNode(context, snapshot);
    if (modelNode.IsNull() ||
        !xq::pipeline::HasStage(modelNode.GetPointer(),
                                xq::pipeline::Stage::Model))
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active model node is required for meshing."));
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

    xq_MeshGenerationRequest request;
    request.meshName =
        QStringLiteral("%1_mesh")
            .arg(QString::fromStdString(modelNode->GetName()).trimmed())
            .toStdString();
    request.globalEdgeSize =
        std::max(1.0e-6,
                 parameters.value(QStringLiteral("element-size"), 1.0)
                     .toDouble());
    const int optimizationSteps =
        parameters.value(QStringLiteral("optimization-steps"), 1).toInt();
    request.optimize = optimizationSteps > 0;

    const auto result =
        xq_MeshPipelineService::CreateVolumeMesh(
            context.DataStorage().GetPointer(),
            modelNode,
            request);
    if (!result.ok)
    {
        SetMessage(message, FirstDiagnosticMessage(result));
        return false;
    }

    if (!CommitMeshResult(context, entryId, result, message))
        return false;

    QString selectionMessage;
    context.DataSelection()->SelectCatalogEntry(entryId, &selectionMessage);
    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());

    return true;
}

} // namespace

bool RegisterDynamicMeshingWorkflowActionHandler(
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
                           QStringLiteral("Meshing operation id is required."));
                return false;
            }

            if (operationId !=
                QString::fromLatin1(kGenerateVolumeMeshOperationId))
            {
                return RunPlaceholderMeshingOperation(operations,
                                                      snapshot,
                                                      taskMessage);
            }

            return RunGenerateVolumeMesh(context,
                                         renderRefresh,
                                         snapshot,
                                         operationId,
                                         taskMessage);
        };

    return context.WorkflowActions()->RegisterHandler(
        QString::fromLatin1(kMeshingWorkflowId),
        handler,
        message);
}

} // namespace xq::infrastructure
