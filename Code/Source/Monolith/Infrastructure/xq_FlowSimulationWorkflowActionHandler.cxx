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
#include <xq_ResultImport.h>
#include <xq_SimulationPrepPipeline.h>

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace xq::infrastructure
{

namespace
{

constexpr const char* kFlowSimulationWorkflowId = "flow-simulation";
constexpr const char* kConfigureCfdJobOperationId = "configure-cfd-job";
constexpr const char* kRunSteadyFlowOperationId = "run-steady-flow";
constexpr const char* kReviewFlowResultsOperationId = "review-flow-results";
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

QString VirtualResultSourcePath(const QString& catalogEntryId)
{
    return QStringLiteral("xq://generated/simulation-result/%1")
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

mitk::DataNode::Pointer ResolveSimulationPrepNode(
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

mitk::DataNode::Pointer ResolveSimulationResultNode(
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

QString ResultDisplayName(const mitk::DataNode::Pointer& node)
{
    if (node.IsNotNull())
    {
        const QString nodeName =
            QString::fromStdString(node->GetName()).trimmed();
        if (!nodeName.isEmpty())
            return nodeName;
    }

    return QStringLiteral("Steady flow result");
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

QString ResultSourcePath(const mitk::DataNode::Pointer& node,
                         const QString& entryId)
{
    const QString filePath = QString::fromStdString(
        xq::pipeline::GetStringProperty(node.GetPointer(),
                                        "xq.result.file_path"))
                                 .trimmed();
    if (!filePath.isEmpty())
        return filePath;

    return VirtualResultSourcePath(entryId);
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

bool CommitSteadyFlowResults(xq::core::ApplicationContext& context,
                             const QString& baseEntryId,
                             const xq_SimulationRunResult& result,
                             QString* firstEntryId,
                             QString* message)
{
    if (result.resultNodes.empty())
    {
        SetMessage(message,
                   QStringLiteral(
                       "Steady flow solver did not import result nodes."));
        return false;
    }

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

    for (size_t i = 0; i < result.resultNodes.size(); ++i)
    {
        const QString entryId =
            QStringLiteral("%1-result-%2")
                .arg(baseEntryId.trimmed())
                .arg(static_cast<int>(i + 1));
        const QString preflightMessage = PreflightCommitTarget(context,
                                                               entryId);
        if (!preflightMessage.isEmpty())
        {
            SetMessage(message, preflightMessage);
            return false;
        }

        const auto& node = result.resultNodes[i];
        xq::core::DataCatalogEntry entry;
        entry.Id = entryId;
        entry.DisplayName = ResultDisplayName(node);
        entry.SourcePath = ResultSourcePath(node, entryId);
        entry.Modality = QStringLiteral("SimulationResult");
        entry.WorkflowRole = xq::core::DataWorkflowRole::SimulationResult;

        if (!context.DataCatalog()->RegisterEntry(entry, message))
            return false;
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
            !context.DataNodes()->BindNode(entryId, node, message))
        {
            return false;
        }

        if (i == 0 && firstEntryId)
            *firstEntryId = entryId;
    }

    SetMessage(message,
               QStringLiteral(
                   "Registered steady flow result catalog entries."));
    return true;
}

std::vector<std::string> SplitCsv(std::string text)
{
    std::vector<std::string> values;
    std::string token;
    std::istringstream input(text);
    while (std::getline(input, token, ','))
    {
        if (!token.empty())
            values.push_back(token);
    }
    return values;
}

std::string PreferredReviewScalar(const mitk::DataNode::Pointer& node)
{
    std::vector<std::string> fields =
        xq_ResultImport::GetFieldNames(node.GetPointer());
    if (fields.empty())
    {
        fields = SplitCsv(xq::pipeline::GetStringProperty(
            node.GetPointer(), "xq.result.field_names"));
    }

    const auto findContaining = [&fields](const std::string& text) {
        return std::find_if(fields.begin(),
                            fields.end(),
                            [&text](const std::string& field) {
                                return field.find(text) != std::string::npos;
                            });
    };

    if (auto pressure = findContaining("pressure");
        pressure != fields.end())
    {
        return *pressure;
    }
    if (auto velocity = findContaining("velocity");
        velocity != fields.end())
    {
        return *velocity;
    }
    if (auto shear = findContaining("shear");
        shear != fields.end())
    {
        return *shear;
    }
    if (!fields.empty())
        return fields.front();

    return {};
}

std::filesystem::path SteadyFlowCaseDir()
{
    return std::filesystem::temp_directory_path() /
           "xq_monolith_run_steady_flow_case";
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
        if (profile == QStringLiteral("steady"))
            return "xq_simple_flow";
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

bool RunSteadyFlow(xq::core::ApplicationContext& context,
                   xq::core::RenderRefreshService* renderRefresh,
                   const xq::core::WorkflowContextSnapshot& snapshot,
                   const QString& operationId,
                   QString* message)
{
    const auto simPrepNode = ResolveSimulationPrepNode(context, snapshot);
    if (simPrepNode.IsNull() ||
        !xq::pipeline::HasStage(simPrepNode.GetPointer(),
                                xq::pipeline::Stage::SimulationPrep))
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active simulation prep node is required for steady flow solve."));
        return false;
    }

    const QString baseEntryId = ResultCatalogEntryId(snapshot, operationId);
    const QString firstResultEntryId =
        QStringLiteral("%1-result-1").arg(baseEntryId);
    const QString preflightMessage = PreflightCommitTarget(context,
                                                           firstResultEntryId);
    if (!preflightMessage.isEmpty())
    {
        SetMessage(message, preflightMessage);
        return false;
    }

    xq_SimulationRunRequest request;
    request.caseDir = SteadyFlowCaseDir().string();
    request.numProcessors = 1;
    request.mpiPath = "mpiexec";

    const auto result =
        xq_SimulationPrepPipelineService::RunSolverAndImportResults(
            context.DataStorage().GetPointer(),
            simPrepNode,
            request);
    if (!result.ok)
    {
        SetMessage(message, FirstDiagnosticMessage(result));
        return false;
    }

    QString selectedEntryId;
    if (!CommitSteadyFlowResults(context,
                                 baseEntryId,
                                 result,
                                 &selectedEntryId,
                                 message))
    {
        return false;
    }

    if (!selectedEntryId.trimmed().isEmpty())
    {
        QString selectionMessage;
        context.DataSelection()->SelectCatalogEntry(selectedEntryId,
                                                    &selectionMessage);
    }
    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());

    return true;
}

bool RunReviewFlowResults(xq::core::ApplicationContext& context,
                          xq::core::RenderRefreshService* renderRefresh,
                          const xq::core::WorkflowContextSnapshot& snapshot,
                          QString* message)
{
    const auto resultNode = ResolveSimulationResultNode(context, snapshot);
    if (resultNode.IsNull() ||
        !xq::pipeline::HasStage(resultNode.GetPointer(),
                                xq::pipeline::Stage::Result))
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active simulation result node is required for flow result review."));
        return false;
    }

    const std::string scalar = PreferredReviewScalar(resultNode);
    if (scalar.empty())
    {
        SetMessage(message,
                   QStringLiteral(
                       "Flow result review requires named result fields."));
        return false;
    }

    if (!xq_ResultImport::SetActiveScalar(resultNode.GetPointer(), scalar))
    {
        SetMessage(message,
                   QStringLiteral(
                       "Flow result review could not activate %1.")
                       .arg(QString::fromStdString(scalar)));
        return false;
    }

    resultNode->SetVisibility(true);
    resultNode->SetBoolProperty("visible", true);
    resultNode->SetBoolProperty("scalar visibility", true);
    xq::pipeline::SetStringProperty(
        resultNode, "xq.review.flow.active_scalar", scalar);
    xq::pipeline::SetStringProperty(
        resultNode, "xq.review.flow.status", "ready");
    resultNode->Modified();

    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());

    SetMessage(message,
               QStringLiteral("Prepared flow result review for %1.")
                   .arg(QString::fromStdString(scalar)));
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

            if (operationId ==
                QString::fromLatin1(kConfigureCfdJobOperationId))
            {
                return RunConfigureCfdJob(context,
                                          renderRefresh,
                                          snapshot,
                                          operationId,
                                          taskMessage);
            }

            if (operationId ==
                QString::fromLatin1(kRunSteadyFlowOperationId))
            {
                return RunSteadyFlow(context,
                                     renderRefresh,
                                     snapshot,
                                     operationId,
                                     taskMessage);
            }

            if (operationId ==
                QString::fromLatin1(kReviewFlowResultsOperationId))
            {
                return RunReviewFlowResults(context,
                                            renderRefresh,
                                            snapshot,
                                            taskMessage);
            }

            return RunPlaceholderFlowSimulationOperation(operations,
                                                         snapshot,
                                                         taskMessage);
        };

    return context.WorkflowActions()->RegisterHandler(
        QString::fromLatin1(kFlowSimulationWorkflowId),
        handler,
        message);
}

} // namespace xq::infrastructure
