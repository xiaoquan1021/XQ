#include "xq_FlowSimulationWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_RenderRefreshService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <xq_PipelineDataUtils.h>
#include <xq_SimulationPrepPipeline.h>

#include <algorithm>

namespace xq::infrastructure
{

namespace
{

constexpr const char* kFlowSimulationWorkflowId = "flow-simulation";
constexpr const char* kConfigureCfdJobOperationId = "configure-cfd-job";
constexpr const char* kSimulationsFolderId = "simulations";
constexpr const char* kSimulationsFolderTitle = "Simulations";

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

bool RunPlaceholderFlowSimulationOperation(
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
               QStringLiteral("%1 flow simulation operation accepted %2.")
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
    return QStringLiteral("xq://generated/simulation-prep/%1")
        .arg(catalogEntryId.trimmed());
}

mitk::DataNode::Pointer ResolveMeshNode(
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

mitk::DataNode::Pointer ResolveModelNodeForMesh(
    xq::core::ApplicationContext& context,
    const mitk::DataNode::Pointer& meshNode)
{
    return xq::pipeline::ResolveUpstreamNode(
        context.DataStorage().GetPointer(),
        meshNode.GetPointer(),
        xq::pipeline::kSourceModelProperty,
        xq::pipeline::Stage::Model);
}

QString FirstDiagnosticMessage(const xq::pipeline::OperationStatus& status)
{
    if (!status.diagnostics.empty())
        return QString::fromStdString(status.diagnostics.front().message);

    return QStringLiteral("Flow simulation operation failed.");
}

QString ResultDisplayName(const xq_SimulationPrepResult& result)
{
    if (result.node.IsNotNull())
    {
        const QString nodeName =
            QString::fromStdString(result.node->GetName()).trimmed();
        if (!nodeName.isEmpty())
            return nodeName;
    }

    return QStringLiteral("Simulation prep job");
}

QString PreflightCommitTarget(xq::core::ApplicationContext& context,
                              const QString& entryId)
{
    if (!context.DataCatalog())
        return QStringLiteral("Simulation data catalog is required.");
    if (!context.DataHierarchy())
        return QStringLiteral("Simulation data hierarchy is required.");
    if (entryId.trimmed().isEmpty())
        return QStringLiteral("Simulation prep catalog entry id is required.");
    if (context.DataCatalog()->FindById(entryId))
        return QStringLiteral("Duplicate data id.");

    const auto* simulationsFolder =
        context.DataHierarchy()->FindNode(
            QString::fromLatin1(kSimulationsFolderId));
    if (simulationsFolder &&
        simulationsFolder->Kind != xq::core::DataHierarchyNodeKind::Folder)
    {
        return QStringLiteral("Duplicate hierarchy node id.");
    }

    if (context.DataHierarchy()->FindNode(HierarchyNodeId(entryId)))
        return QStringLiteral("Duplicate hierarchy node id.");

    return {};
}

bool CommitSimulationPrepResult(xq::core::ApplicationContext& context,
                                const QString& entryId,
                                const xq_SimulationPrepResult& result,
                                QString* message)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = entryId;
    entry.DisplayName = ResultDisplayName(result);
    entry.SourcePath = VirtualSourcePath(entryId);
    entry.Modality = QStringLiteral("SimulationPrep");
    entry.WorkflowRole = xq::core::DataWorkflowRole::SimulationPrep;

    if (!context.DataCatalog()->RegisterEntry(entry, message))
        return false;

    if (!context.DataHierarchy()->FindNode(
            QString::fromLatin1(kSimulationsFolderId)))
    {
        if (!context.DataHierarchy()->AddFolder(
                QString::fromLatin1(kSimulationsFolderId),
                context.DataHierarchy()->RootId(),
                QString::fromLatin1(kSimulationsFolderTitle),
                message))
        {
            return false;
        }
    }

    if (!context.DataHierarchy()->AddDataEntry(
            HierarchyNodeId(entryId),
            QString::fromLatin1(kSimulationsFolderId),
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
               QStringLiteral("Registered simulation prep catalog entry."));
    return true;
}

std::string SolverTypeForProfile(const QVariantMap& parameters)
{
    const QString profile =
        parameters.value(QStringLiteral("solver-profile"),
                         QStringLiteral("steady"))
            .toString()
            .trimmed()
            .toLower();
    if (profile == QStringLiteral("steady") ||
        profile == QStringLiteral("pulsatile") ||
        profile == QStringLiteral("transient"))
    {
        return "xq_export_only";
    }

    return "xq_export_only";
}

bool RunConfigureCfdJob(xq::core::ApplicationContext& context,
                        xq::core::RenderRefreshService* renderRefresh,
                        const xq::core::WorkflowContextSnapshot& snapshot,
                        const QString& operationId,
                        QString* message)
{
    const auto meshNode = ResolveMeshNode(context, snapshot);
    if (meshNode.IsNull() ||
        !xq::pipeline::HasStage(meshNode.GetPointer(),
                                xq::pipeline::Stage::VolumeMesh))
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active mesh node is required for flow simulation."));
        return false;
    }

    const auto modelNode = ResolveModelNodeForMesh(context, meshNode);
    if (modelNode.IsNull())
    {
        SetMessage(message,
                   QStringLiteral(
                       "Upstream model node is required for flow simulation."));
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

    xq_SimulationPrepRequest request;
    request.jobName =
        QStringLiteral("%1_cfd")
            .arg(QString::fromStdString(meshNode->GetName()).trimmed())
            .toStdString();
    request.solverType = SolverTypeForProfile(parameters);
    request.numLinearIterations =
        std::max(1,
                 parameters.value(QStringLiteral("inlet-count"), 0).toInt() +
                     5);
    request.numNonlinearIterations =
        std::max(1,
                 parameters.value(QStringLiteral("outlet-count"), 0).toInt() +
                     25);

    const auto result =
        xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep(
            context.DataStorage().GetPointer(),
            modelNode,
            meshNode,
            request);
    if (!result.ok)
    {
        SetMessage(message, FirstDiagnosticMessage(result));
        return false;
    }

    if (!CommitSimulationPrepResult(context, entryId, result, message))
        return false;

    QString selectionMessage;
    context.DataSelection()->SelectCatalogEntry(entryId, &selectionMessage);
    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());

    return true;
}

} // namespace

bool RegisterDynamicFlowSimulationWorkflowActionHandler(
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
                               "Flow simulation operation id is required."));
                return false;
            }

            if (operationId !=
                QString::fromLatin1(kConfigureCfdJobOperationId))
            {
                return RunPlaceholderFlowSimulationOperation(operations,
                                                             snapshot,
                                                             taskMessage);
            }

            return RunConfigureCfdJob(context,
                                      renderRefresh,
                                      snapshot,
                                      operationId,
                                      taskMessage);
        };

    return context.WorkflowActions()->RegisterHandler(
        QString::fromLatin1(kFlowSimulationWorkflowId),
        handler,
        message);
}

} // namespace xq::infrastructure
