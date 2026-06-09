#include "xq_RomSimulationWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_RenderRefreshService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <xq_MitkROMJob.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ROMJob.h>

#include <algorithm>
#include <memory>

namespace xq::infrastructure
{

namespace
{

constexpr const char* kRomSimulationWorkflowId = "rom-simulation";
constexpr const char* kBuild1DNetworkOperationId = "build-1d-network";
constexpr const char* kRomSimulationsFolderId = "rom-simulations";
constexpr const char* kRomSimulationsFolderTitle = "ROM Simulations";

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

bool RunUnsupportedRomSimulationOperation(
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
    return QStringLiteral("xq://generated/rom/%1")
        .arg(catalogEntryId.trimmed());
}

mitk::DataNode::Pointer ResolveSelectedNode(
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

mitk::DataNode::Pointer ResolveMeshNode(
    xq::core::ApplicationContext& context,
    const mitk::DataNode::Pointer& selectedNode)
{
    if (selectedNode.IsNull())
        return nullptr;

    if (xq::pipeline::HasStage(selectedNode.GetPointer(),
                               xq::pipeline::Stage::VolumeMesh))
    {
        return selectedNode;
    }

    if (xq::pipeline::HasStage(selectedNode.GetPointer(),
                               xq::pipeline::Stage::SimulationPrep))
    {
        return xq::pipeline::ResolveUpstreamNode(
            context.DataStorage().GetPointer(),
            selectedNode.GetPointer(),
            xq::pipeline::kSourceMeshProperty,
            xq::pipeline::Stage::VolumeMesh);
    }

    return nullptr;
}

QString PreflightCommitTarget(xq::core::ApplicationContext& context,
                              const QString& entryId)
{
    if (!context.DataCatalog())
        return QStringLiteral("ROM data catalog is required.");
    if (!context.DataHierarchy())
        return QStringLiteral("ROM data hierarchy is required.");
    if (entryId.trimmed().isEmpty())
        return QStringLiteral("ROM catalog entry id is required.");
    if (context.DataCatalog()->FindById(entryId))
        return QStringLiteral("Duplicate data id.");

    const auto* romFolder =
        context.DataHierarchy()->FindNode(
            QString::fromLatin1(kRomSimulationsFolderId));
    if (romFolder &&
        romFolder->Kind != xq::core::DataHierarchyNodeKind::Folder)
    {
        return QStringLiteral("Duplicate hierarchy node id.");
    }

    if (context.DataHierarchy()->FindNode(HierarchyNodeId(entryId)))
        return QStringLiteral("Duplicate hierarchy node id.");

    return {};
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

    return QStringLiteral("ROM network job");
}

void SetIntProperty(const mitk::DataNode::Pointer& node,
                    const char* key,
                    int value)
{
    if (node.IsNull() || key == nullptr)
        return;

    node->SetIntProperty(key, value);
}

std::unique_ptr<xq_ROMJob> CreateRomJob(const QString& meshName,
                                        const QVariantMap& parameters)
{
    auto job = std::make_unique<xq_ROMJob>();
    job->SetJobName(QStringLiteral("%1_rom").arg(meshName).toStdString());
    job->SetModelType("1D");

    const int branchCount =
        std::max(1,
                 parameters.value(QStringLiteral("branch-count"), 1)
                     .toInt());
    const int outletCount =
        std::max(1,
                 parameters.value(QStringLiteral("outlet-count"), 1)
                     .toInt());

    job->SetCapProp("inlet", "role", "inflow");
    job->SetCapProp("inlet", "branch_count", std::to_string(branchCount));
    for (int i = 0; i < outletCount; ++i)
    {
        const std::string capName =
            i == 0 ? "outlet" : "outlet_" + std::to_string(i + 1);
        job->SetCapProp(capName, "role", "outflow");
        job->SetRCR(capName, 100.0, 1.0e-5, 900.0);
    }

    job->SetProperty("branch_count", std::to_string(branchCount));
    job->SetProperty("outlet_count", std::to_string(outletCount));
    job->AddOutputField("pressure");
    job->AddOutputField("flow");
    return job;
}

mitk::DataNode::Pointer CreateRomNode(
    const mitk::DataNode::Pointer& meshNode,
    const mitk::DataNode::Pointer& selectedNode,
    const QVariantMap& parameters)
{
    const QString meshName =
        QString::fromStdString(meshNode->GetName()).trimmed();
    auto job = CreateRomJob(meshName, parameters);

    auto mitkJob = xq_MitkROMJob::New();
    mitkJob->SetROMJob(std::move(job));
    mitkJob->SetStatus("configured");

    auto node = mitk::DataNode::New();
    node->SetName(QStringLiteral("%1_rom").arg(meshName).toStdString());
    node->SetData(mitkJob);
    xq::pipeline::MarkGeneratedNode(node,
                                    xq::pipeline::Stage::ROMSimulation,
                                    "build-1d-network",
                                    "XQ Monolith",
                                    "1");
    xq::pipeline::SetStringProperty(
        node, xq::pipeline::kSourceMeshProperty, meshName.toStdString());
    if (selectedNode.IsNotNull() &&
        xq::pipeline::HasStage(selectedNode.GetPointer(),
                               xq::pipeline::Stage::SimulationPrep))
    {
        xq::pipeline::SetStringProperty(
            node,
            xq::pipeline::kSourceSimulationJobProperty,
            selectedNode->GetName());
    }
    xq::pipeline::SetStringProperty(node, "xq.rom.status", "configured");
    xq::pipeline::SetStringProperty(node, "xq.rom.model_type", "1D");
    SetIntProperty(node,
                   "xq.rom.branch_count",
                   std::max(1,
                            parameters
                                .value(QStringLiteral("branch-count"), 1)
                                .toInt()));
    SetIntProperty(node,
                   "xq.rom.outlet_count",
                   std::max(1,
                            parameters
                                .value(QStringLiteral("outlet-count"), 1)
                                .toInt()));
    node->Modified();
    return node;
}

bool CommitRomResult(xq::core::ApplicationContext& context,
                     const QString& entryId,
                     const mitk::DataNode::Pointer& sourceNode,
                     const mitk::DataNode::Pointer& resultNode,
                     QString* message)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = entryId;
    entry.DisplayName = ResultDisplayName(resultNode);
    entry.SourcePath = VirtualSourcePath(entryId);
    entry.Modality = QStringLiteral("ROMSimulation");
    entry.WorkflowRole = xq::core::DataWorkflowRole::ROMSimulation;

    if (!context.DataCatalog()->RegisterEntry(entry, message))
        return false;

    if (!context.DataHierarchy()->FindNode(
            QString::fromLatin1(kRomSimulationsFolderId)))
    {
        if (!context.DataHierarchy()->AddFolder(
                QString::fromLatin1(kRomSimulationsFolderId),
                context.DataHierarchy()->RootId(),
                QString::fromLatin1(kRomSimulationsFolderTitle),
                message))
        {
            return false;
        }
    }

    if (!context.DataHierarchy()->AddDataEntry(
            HierarchyNodeId(entryId),
            QString::fromLatin1(kRomSimulationsFolderId),
            entryId,
            entry.DisplayName,
            message))
    {
        return false;
    }

    if (context.DataStorage().IsNotNull() && resultNode.IsNotNull())
    {
        if (sourceNode.IsNotNull())
            context.DataStorage()->Add(resultNode, sourceNode);
        else
            context.DataStorage()->Add(resultNode);
    }

    if (context.DataNodes() &&
        !context.DataNodes()->BindNode(entryId, resultNode, message))
    {
        return false;
    }

    SetMessage(message,
               QStringLiteral("Registered ROM network catalog entry."));
    return true;
}

bool RunBuild1DNetwork(xq::core::ApplicationContext& context,
                       xq::core::RenderRefreshService* renderRefresh,
                       const xq::core::WorkflowContextSnapshot& snapshot,
                       const QString& operationId,
                       QString* message)
{
    const auto selectedNode = ResolveSelectedNode(context, snapshot);
    const auto meshNode = ResolveMeshNode(context, selectedNode);
    if (meshNode.IsNull())
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active mesh or simulation prep node is required for ROM network build."));
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

    auto resultNode = CreateRomNode(meshNode, selectedNode, parameters);
    auto* mitkJob = dynamic_cast<xq_MitkROMJob*>(resultNode->GetData());
    auto* job = mitkJob ? mitkJob->GetROMJob(0) : nullptr;
    const std::string validationMessage = job ? job->Validate()
                                              : "ROM job is missing.";
    if (!validationMessage.empty())
    {
        SetMessage(message, QString::fromStdString(validationMessage));
        return false;
    }

    if (!CommitRomResult(context,
                         entryId,
                         selectedNode,
                         resultNode,
                         message))
    {
        return false;
    }

    QString selectionMessage;
    context.DataSelection()->SelectCatalogEntry(entryId, &selectionMessage);
    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());

    return true;
}

} // namespace

bool RegisterDynamicRomSimulationWorkflowActionHandler(
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
                               "ROM simulation operation id is required."));
                return false;
            }

            if (operationId != QString::fromLatin1(kBuild1DNetworkOperationId))
            {
                return RunUnsupportedRomSimulationOperation(operations,
                                                            snapshot,
                                                            operationId,
                                                            taskMessage);
            }

            return RunBuild1DNetwork(context,
                                     renderRefresh,
                                     snapshot,
                                     operationId,
                                     taskMessage);
        };

    return context.WorkflowActions()->RegisterHandler(
        QString::fromLatin1(kRomSimulationWorkflowId),
        handler,
        message);
}

} // namespace xq::infrastructure
