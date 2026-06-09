#include "xq_MultiPhysicsWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_RenderRefreshService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <xq_MitkMultiPhysicsJob.h>
#include <xq_MultiPhysicsJob.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ResultImport.h>

#include <algorithm>
#include <memory>
#include <sstream>

namespace xq::infrastructure
{

namespace
{

constexpr const char* kMultiPhysicsWorkflowId = "multiphysics";
constexpr const char* kConfigureCouplingOperationId = "configure-coupling";
constexpr const char* kReviewCoupledResultsOperationId =
    "review-coupled-results";
constexpr const char* kMultiPhysicsFolderId = "multiphysics";
constexpr const char* kMultiPhysicsFolderTitle = "MultiPhysics";
constexpr const char* kSourceRomProperty = "xq.source.rom";

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

bool RunUnsupportedMultiPhysicsOperation(
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
    return QStringLiteral("xq://generated/multiphysics/%1")
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

mitk::DataNode::Pointer ResolveResultNode(
    xq::core::ApplicationContext& context,
    const xq::core::WorkflowContextSnapshot& snapshot)
{
    const auto selectedNode = ResolveSelectedNode(context, snapshot);
    if (selectedNode.IsNotNull() &&
        xq::pipeline::HasStage(selectedNode.GetPointer(),
                               xq::pipeline::Stage::Result))
    {
        return selectedNode;
    }

    return nullptr;
}

bool IsCouplingSourceNode(const mitk::DataNode::Pointer& node)
{
    return node.IsNotNull() &&
           (xq::pipeline::HasStage(node.GetPointer(),
                                   xq::pipeline::Stage::ROMSimulation) ||
            xq::pipeline::HasStage(node.GetPointer(),
                                   xq::pipeline::Stage::SimulationPrep));
}

QString PreflightCommitTarget(xq::core::ApplicationContext& context,
                              const QString& entryId)
{
    if (!context.DataCatalog())
        return QStringLiteral("MultiPhysics data catalog is required.");
    if (!context.DataHierarchy())
        return QStringLiteral("MultiPhysics data hierarchy is required.");
    if (entryId.trimmed().isEmpty())
        return QStringLiteral("MultiPhysics catalog entry id is required.");
    if (context.DataCatalog()->FindById(entryId))
        return QStringLiteral("Duplicate data id.");

    const auto* folder =
        context.DataHierarchy()->FindNode(
            QString::fromLatin1(kMultiPhysicsFolderId));
    if (folder &&
        folder->Kind != xq::core::DataHierarchyNodeKind::Folder)
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

    return QStringLiteral("MultiPhysics job");
}

void SetIntProperty(const mitk::DataNode::Pointer& node,
                    const char* key,
                    int value)
{
    if (node.IsNull() || key == nullptr)
        return;

    node->SetIntProperty(key, value);
}

void SetDoubleProperty(const mitk::DataNode::Pointer& node,
                       const char* key,
                       double value)
{
    if (node.IsNull() || key == nullptr)
        return;

    node->SetFloatProperty(key, static_cast<float>(value));
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

std::string PreferredCoupledReviewScalar(
    const mitk::DataNode::Pointer& node)
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
    if (auto displacement = findContaining("displacement");
        displacement != fields.end())
    {
        return *displacement;
    }
    if (auto velocity = findContaining("velocity");
        velocity != fields.end())
    {
        return *velocity;
    }
    if (!fields.empty())
        return fields.front();

    return {};
}

std::unique_ptr<xq_MultiPhysicsJob> CreateMultiPhysicsJob(
    const QString& sourceName,
    const QVariantMap& parameters)
{
    auto job = std::make_unique<xq_MultiPhysicsJob>();
    job->SetJobName(
        QStringLiteral("%1_multiphysics").arg(sourceName).toStdString());
    job->SetTimeStepSize(0.001);
    job->SetNumTimeSteps(
        std::max(1,
                 parameters.value(QStringLiteral("coupling-iterations"), 1)
                     .toInt()));

    xq_MultiPhysicsDomain fluid;
    fluid.name = "fluid";
    fluid.type = xq_MultiPhysicsDomainType::Fluid;
    fluid.material.density = 1.06;
    fluid.material.viscosity = 0.04;
    fluid.properties["source"] = sourceName.toStdString();
    job->AddDomain(fluid);

    xq_MultiPhysicsDomain wall;
    wall.name = "wall";
    wall.type = xq_MultiPhysicsDomainType::Solid;
    wall.material.density = 1.2;
    wall.material.elasticModulus = 4.0e6;
    wall.material.poissonRatio = 0.45;
    wall.properties["source"] = sourceName.toStdString();
    job->AddDomain(wall);

    xq_MultiPhysicsEquation fsi;
    fsi.name = "fsi-coupling";
    fsi.type = xq_MultiPhysicsEquationType::FSI;
    fsi.domainNames = {"fluid", "wall"};
    fsi.solverSettings.linearSolver = "gmres";
    fsi.solverSettings.tolerance = std::max(
        1.0e-12,
        parameters.value(QStringLiteral("relaxation-factor"), 1.0)
            .toDouble() *
            1.0e-6);
    fsi.solverSettings.maxIterations =
        std::max(1,
                 parameters.value(QStringLiteral("coupling-iterations"), 1)
                     .toInt());
    job->AddEquation(fsi);

    xq_MultiPhysicsBoundaryCondition inlet;
    inlet.faceName = "inlet";
    inlet.domainName = "fluid";
    inlet.bcType = xq_MultiPhysicsBCType::Dirichlet;
    inlet.parameters["value"] = "coupled-flow";
    job->AddBoundaryCondition(inlet);

    xq_MultiPhysicsBoundaryCondition outlet;
    outlet.faceName = "outlet";
    outlet.domainName = "fluid";
    outlet.bcType = xq_MultiPhysicsBCType::Resistance;
    outlet.parameters["value"] = "coupled-pressure";
    job->AddBoundaryCondition(outlet);

    job->SetProperty(
        "coupling_iterations",
        std::to_string(std::max(
            1,
            parameters.value(QStringLiteral("coupling-iterations"), 1)
                .toInt())));
    job->SetProperty(
        "relaxation_factor",
        std::to_string(
            parameters.value(QStringLiteral("relaxation-factor"), 1.0)
                .toDouble()));
    return job;
}

mitk::DataNode::Pointer CreateMultiPhysicsNode(
    const mitk::DataNode::Pointer& sourceNode,
    const QVariantMap& parameters)
{
    const QString sourceName =
        QString::fromStdString(sourceNode->GetName()).trimmed();
    auto job = CreateMultiPhysicsJob(sourceName, parameters);

    auto mitkJob = xq_MitkMultiPhysicsJob::New();
    mitkJob->SetJob(std::move(job));
    mitkJob->SetStatus("configured");

    auto node = mitk::DataNode::New();
    node->SetName(
        QStringLiteral("%1_multiphysics").arg(sourceName).toStdString());
    node->SetData(mitkJob);
    xq::pipeline::MarkGeneratedNode(node,
                                    xq::pipeline::Stage::MultiPhysics,
                                    "configure-coupling",
                                    "XQ Monolith",
                                    "1");
    if (xq::pipeline::HasStage(sourceNode.GetPointer(),
                               xq::pipeline::Stage::ROMSimulation))
    {
        xq::pipeline::SetStringProperty(node,
                                        kSourceRomProperty,
                                        sourceName.toStdString());
        const std::string sourceMesh = xq::pipeline::GetStringProperty(
            sourceNode.GetPointer(), xq::pipeline::kSourceMeshProperty);
        if (!sourceMesh.empty())
        {
            xq::pipeline::SetStringProperty(
                node, xq::pipeline::kSourceMeshProperty, sourceMesh);
        }
    }
    else if (xq::pipeline::HasStage(sourceNode.GetPointer(),
                                    xq::pipeline::Stage::SimulationPrep))
    {
        xq::pipeline::SetStringProperty(
            node,
            xq::pipeline::kSourceSimulationJobProperty,
            sourceNode->GetName());
        const std::string sourceMesh = xq::pipeline::GetStringProperty(
            sourceNode.GetPointer(), xq::pipeline::kSourceMeshProperty);
        if (!sourceMesh.empty())
        {
            xq::pipeline::SetStringProperty(
                node, xq::pipeline::kSourceMeshProperty, sourceMesh);
        }
    }

    xq::pipeline::SetStringProperty(
        node, "xq.multiphysics.status", "configured");
    SetIntProperty(node,
                   "xq.multiphysics.coupling_iterations",
                   std::max(1,
                            parameters
                                .value(QStringLiteral("coupling-iterations"),
                                       1)
                                .toInt()));
    SetDoubleProperty(node,
                      "xq.multiphysics.relaxation_factor",
                      parameters
                          .value(QStringLiteral("relaxation-factor"), 1.0)
                          .toDouble());
    node->Modified();
    return node;
}

bool CommitMultiPhysicsResult(xq::core::ApplicationContext& context,
                              const QString& entryId,
                              const mitk::DataNode::Pointer& sourceNode,
                              const mitk::DataNode::Pointer& resultNode,
                              QString* message)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = entryId;
    entry.DisplayName = ResultDisplayName(resultNode);
    entry.SourcePath = VirtualSourcePath(entryId);
    entry.Modality = QStringLiteral("MultiPhysics");
    entry.WorkflowRole = xq::core::DataWorkflowRole::MultiPhysics;

    if (!context.DataCatalog()->RegisterEntry(entry, message))
        return false;

    if (!context.DataHierarchy()->FindNode(
            QString::fromLatin1(kMultiPhysicsFolderId)))
    {
        if (!context.DataHierarchy()->AddFolder(
                QString::fromLatin1(kMultiPhysicsFolderId),
                context.DataHierarchy()->RootId(),
                QString::fromLatin1(kMultiPhysicsFolderTitle),
                message))
        {
            return false;
        }
    }

    if (!context.DataHierarchy()->AddDataEntry(
            HierarchyNodeId(entryId),
            QString::fromLatin1(kMultiPhysicsFolderId),
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
               QStringLiteral("Registered multiphysics catalog entry."));
    return true;
}

bool RunConfigureCoupling(xq::core::ApplicationContext& context,
                          xq::core::RenderRefreshService* renderRefresh,
                          const xq::core::WorkflowContextSnapshot& snapshot,
                          const QString& operationId,
                          QString* message)
{
    const auto sourceNode = ResolveSelectedNode(context, snapshot);
    if (!IsCouplingSourceNode(sourceNode))
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active ROM or simulation prep node is required for multiphysics coupling."));
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

    auto resultNode = CreateMultiPhysicsNode(sourceNode, parameters);
    auto* mitkJob =
        dynamic_cast<xq_MitkMultiPhysicsJob*>(resultNode->GetData());
    auto* job = mitkJob ? mitkJob->GetJob(0) : nullptr;
    const std::string validationMessage = job ? job->Validate()
                                              : "MultiPhysics job is missing.";
    if (!validationMessage.empty())
    {
        SetMessage(message, QString::fromStdString(validationMessage));
        return false;
    }

    if (!CommitMultiPhysicsResult(context,
                                  entryId,
                                  sourceNode,
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

bool RunReviewCoupledResults(
    xq::core::ApplicationContext& context,
    xq::core::RenderRefreshService* renderRefresh,
    const xq::core::WorkflowContextSnapshot& snapshot,
    QString* message)
{
    const auto resultNode = ResolveResultNode(context, snapshot);
    if (resultNode.IsNull())
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active coupled result node is required for multiphysics review."));
        return false;
    }

    const std::string scalar = PreferredCoupledReviewScalar(resultNode);
    if (scalar.empty())
    {
        SetMessage(message,
                   QStringLiteral(
                       "MultiPhysics result review requires named result fields."));
        return false;
    }

    if (!xq_ResultImport::SetActiveScalar(resultNode.GetPointer(), scalar))
    {
        SetMessage(message,
                   QStringLiteral(
                       "MultiPhysics result review could not activate %1.")
                       .arg(QString::fromStdString(scalar)));
        return false;
    }

    resultNode->SetVisibility(true);
    resultNode->SetBoolProperty("visible", true);
    resultNode->SetBoolProperty("scalar visibility", true);
    xq::pipeline::SetStringProperty(
        resultNode, "xq.review.multiphysics.active_scalar", scalar);
    xq::pipeline::SetStringProperty(
        resultNode, "xq.review.multiphysics.status", "ready");
    resultNode->Modified();

    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());

    SetMessage(message,
               QStringLiteral(
                   "Prepared multiphysics result review for %1.")
                   .arg(QString::fromStdString(scalar)));
    return true;
}

} // namespace

bool RegisterDynamicMultiPhysicsWorkflowActionHandler(
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
                               "MultiPhysics operation id is required."));
                return false;
            }

            if (operationId ==
                QString::fromLatin1(kConfigureCouplingOperationId))
            {
                return RunConfigureCoupling(context,
                                            renderRefresh,
                                            snapshot,
                                            operationId,
                                            taskMessage);
            }

            if (operationId ==
                QString::fromLatin1(kReviewCoupledResultsOperationId))
            {
                return RunReviewCoupledResults(context,
                                               renderRefresh,
                                               snapshot,
                                               taskMessage);
            }

            return RunUnsupportedMultiPhysicsOperation(operations,
                                                       snapshot,
                                                       operationId,
                                                       taskMessage);
        };

    return context.WorkflowActions()->RegisterHandler(
        QString::fromLatin1(kMultiPhysicsWorkflowId),
        handler,
        message);
}

} // namespace xq::infrastructure
