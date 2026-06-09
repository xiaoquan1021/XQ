#include "xq_PathWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_RenderRefreshService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <xq_CenterlineInteractor.h>
#include <xq_PathPipeline.h>
#include <xq_PipelineDataUtils.h>
#include <xq_VesselCenterline.h>

#include <mitkImage.h>

#include <usModuleRegistry.h>

#include <QVariantList>
#include <QVariantMap>

#include <algorithm>

namespace xq::infrastructure
{

namespace
{

constexpr const char* kPathWorkflowId = "path";
constexpr const char* kCreateCenterlineOperationId = "create-centerline";
constexpr const char* kEditControlPointsOperationId = "edit-control-points";
constexpr const char* kSmoothPathOperationId = "smooth-path";
constexpr const char* kPathsFolderId = "paths";
constexpr const char* kPathsFolderTitle = "Paths";

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

bool RunUnsupportedPathOperation(
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
    return QStringLiteral("xq://generated/path/%1")
        .arg(catalogEntryId.trimmed());
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

bool ParseSeedPoints(const QVariant& value,
                     std::vector<mitk::Point3D>& seeds,
                     QString* message)
{
    seeds.clear();
    const QVariantList seedValues = value.toList();
    if (seedValues.size() < 2)
    {
        SetMessage(message,
                   QStringLiteral(
                       "Path creation requires at least two seed points."));
        return false;
    }

    for (const auto& seedValue : seedValues)
    {
        const QVariantList coordinates = seedValue.toList();
        if (coordinates.size() != 3)
        {
            SetMessage(message,
                       QStringLiteral(
                           "Path seed points must use x,y,z triplets."));
            return false;
        }

        mitk::Point3D seed;
        for (int i = 0; i < 3; ++i)
        {
            bool ok = false;
            const double coordinate = coordinates.at(i).toDouble(&ok);
            if (!ok)
            {
                SetMessage(message,
                           QStringLiteral(
                               "Path seed point coordinates must be numeric."));
                return false;
            }
            seed[i] = coordinate;
        }
        seeds.push_back(seed);
    }

    SetMessage(message, QString());
    return true;
}

QString FirstDiagnosticMessage(const xq::pipeline::OperationStatus& status)
{
    if (!status.diagnostics.empty())
        return QString::fromStdString(status.diagnostics.front().message);

    return QStringLiteral("Path creation failed.");
}

QString ResultDisplayName(const xq_PathPlanResult& pathResult)
{
    if (pathResult.node.IsNotNull())
    {
        const QString nodeName =
            QString::fromStdString(pathResult.node->GetName()).trimmed();
        if (!nodeName.isEmpty())
            return nodeName;
    }

    return QStringLiteral("Path result");
}

QString ResultDisplayName(const xq_PathExtractResult& pathResult)
{
    if (pathResult.node.IsNotNull())
    {
        const QString nodeName =
            QString::fromStdString(pathResult.node->GetName()).trimmed();
        if (!nodeName.isEmpty())
            return nodeName;
    }

    return QStringLiteral("Path result");
}

QString PreflightCommitTarget(xq::core::ApplicationContext& context,
                              const QString& entryId)
{
    if (!context.DataCatalog())
        return QStringLiteral("Path data catalog is required.");
    if (!context.DataHierarchy())
        return QStringLiteral("Path data hierarchy is required.");
    if (entryId.trimmed().isEmpty())
        return QStringLiteral("Path result catalog entry id is required.");
    if (context.DataCatalog()->FindById(entryId))
        return QStringLiteral("Duplicate data id.");

    const auto* pathsFolder =
        context.DataHierarchy()->FindNode(QString::fromLatin1(kPathsFolderId));
    if (pathsFolder &&
        pathsFolder->Kind != xq::core::DataHierarchyNodeKind::Folder)
    {
        return QStringLiteral("Duplicate hierarchy node id.");
    }

    if (context.DataHierarchy()->FindNode(HierarchyNodeId(entryId)))
        return QStringLiteral("Duplicate hierarchy node id.");

    return {};
}

bool CommitPathResult(xq::core::ApplicationContext& context,
                      const QString& entryId,
                      const xq_PathPlanResult& pathResult,
                      QString* message)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = entryId;
    entry.DisplayName = ResultDisplayName(pathResult);
    entry.SourcePath = VirtualSourcePath(entryId);
    entry.Modality = QStringLiteral("Generated");
    entry.WorkflowRole = xq::core::DataWorkflowRole::Path;

    if (!context.DataCatalog()->RegisterEntry(entry, message))
        return false;

    if (!context.DataHierarchy()->FindNode(QString::fromLatin1(kPathsFolderId)))
    {
        if (!context.DataHierarchy()->AddFolder(
                QString::fromLatin1(kPathsFolderId),
                context.DataHierarchy()->RootId(),
                QString::fromLatin1(kPathsFolderTitle),
                message))
        {
            return false;
        }
    }

    if (!context.DataHierarchy()->AddDataEntry(HierarchyNodeId(entryId),
                                               QString::fromLatin1(kPathsFolderId),
                                               entryId,
                                               entry.DisplayName,
                                               message))
    {
        return false;
    }

    if (context.DataNodes() &&
        !context.DataNodes()->BindNode(entryId, pathResult.node, message))
    {
        return false;
    }

    SetMessage(message,
               QStringLiteral("Registered path result catalog entry."));
    return true;
}

bool CommitPathResult(xq::core::ApplicationContext& context,
                      const QString& entryId,
                      const xq_PathExtractResult& pathResult,
                      QString* message)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = entryId;
    entry.DisplayName = ResultDisplayName(pathResult);
    entry.SourcePath = VirtualSourcePath(entryId);
    entry.Modality = QStringLiteral("Generated");
    entry.WorkflowRole = xq::core::DataWorkflowRole::Path;

    if (!context.DataCatalog()->RegisterEntry(entry, message))
        return false;

    if (!context.DataHierarchy()->FindNode(QString::fromLatin1(kPathsFolderId)))
    {
        if (!context.DataHierarchy()->AddFolder(
                QString::fromLatin1(kPathsFolderId),
                context.DataHierarchy()->RootId(),
                QString::fromLatin1(kPathsFolderTitle),
                message))
        {
            return false;
        }
    }

    if (!context.DataHierarchy()->AddDataEntry(HierarchyNodeId(entryId),
                                               QString::fromLatin1(kPathsFolderId),
                                               entryId,
                                               entry.DisplayName,
                                               message))
    {
        return false;
    }

    if (context.DataNodes() &&
        !context.DataNodes()->BindNode(entryId, pathResult.node, message))
    {
        return false;
    }

    SetMessage(message,
               QStringLiteral("Registered path result catalog entry."));
    return true;
}

bool RunCreateCenterline(xq::core::ApplicationContext& context,
                         xq::core::RenderRefreshService* renderRefresh,
                         const xq::core::WorkflowContextSnapshot& snapshot,
                         const QString& operationId,
                         QString* message)
{
    auto* operations = context.WorkflowOperations();
    if (!operations)
    {
        SetMessage(message, QStringLiteral("Path operation service is required."));
        return false;
    }

    const QVariantMap parameters =
        operations->ParameterValues(snapshot.WorkflowId, operationId);
    std::vector<mitk::Point3D> seeds;
    if (!ParseSeedPoints(parameters.value(QStringLiteral("seed-points")),
                         seeds,
                         message))
    {
        return false;
    }

    const auto sourceNode = ResolveSourceNode(context, snapshot);
    if (sourceNode.IsNull() ||
        dynamic_cast<mitk::Image*>(sourceNode->GetData()) == nullptr)
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active image node is required for path creation."));
        return false;
    }

    const QString entryId = ResultCatalogEntryId(snapshot, operationId);
    const QString preflightMessage = PreflightCommitTarget(context, entryId);
    if (!preflightMessage.isEmpty())
    {
        SetMessage(message, preflightMessage);
        return false;
    }

    const QString sourceName =
        QString::fromStdString(sourceNode->GetName()).trimmed();
    xq_PathPlanRequest request;
    request.pathName =
        QStringLiteral("%1_path").arg(sourceName).toStdString();
    request.imageNodeName = sourceName.toStdString();
    request.seeds = seeds;
    request.algorithm = "dijkstra";
    request.sampleCount =
        std::max(2,
                 parameters.value(QStringLiteral("control-point-count"))
                     .toInt());
    request.smoothCurve = true;

    const auto pathResult =
        xq_PathPipelineService::CreatePath(context.DataStorage().GetPointer(),
                                           request);
    if (!pathResult.ok)
    {
        SetMessage(message, FirstDiagnosticMessage(pathResult));
        return false;
    }

    if (!CommitPathResult(context, entryId, pathResult, message))
        return false;

    QString selectionMessage;
    context.DataSelection()->SelectCatalogEntry(entryId, &selectionMessage);
    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());

    return true;
}

bool RunSmoothPath(xq::core::ApplicationContext& context,
                   xq::core::RenderRefreshService* renderRefresh,
                   const xq::core::WorkflowContextSnapshot& snapshot,
                   const QString& operationId,
                   QString* message)
{
    auto* operations = context.WorkflowOperations();
    if (!operations)
    {
        SetMessage(message, QStringLiteral("Path operation service is required."));
        return false;
    }

    const auto sourceNode = ResolveSourceNode(context, snapshot);
    if (sourceNode.IsNull() ||
        !xq::pipeline::HasStage(sourceNode.GetPointer(),
                                xq::pipeline::Stage::Path) ||
        dynamic_cast<xq_VesselCenterline*>(sourceNode->GetData()) == nullptr)
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active path node is required for path smoothing."));
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
        operations->ParameterValues(snapshot.WorkflowId, operationId);
    const int sampleCount =
        std::max(2,
                 parameters.value(QStringLiteral("iteration-count"), 64)
                     .toInt());

    const QString sourceName =
        QString::fromStdString(sourceNode->GetName()).trimmed();
    xq_PathExtractRequest request;
    request.pathName =
        QStringLiteral("%1_smoothed").arg(sourceName).toStdString();
    request.centerlineNodeName = sourceName.toStdString();
    request.sampleCount = sampleCount;
    request.smoothCurve = true;

    auto pathResult =
        xq_PathPipelineService::ExtractPathFromCenterline(
            context.DataStorage().GetPointer(),
            request);
    if (!pathResult.ok)
    {
        SetMessage(message, FirstDiagnosticMessage(pathResult));
        return false;
    }

    if (pathResult.node.IsNotNull())
    {
        pathResult.node->SetStringProperty("xq.path.operation",
                                           "smooth-path");
        pathResult.node->SetBoolProperty("xq.path.source_preserved", true);
        pathResult.node->SetStringProperty(
            "xq.path.capability.diagnostic",
            "Smooth Path generated a new spline-smoothed Path from the "
            "selected centerline. The source path node was not mutated.");
    }

    if (!CommitPathResult(context, entryId, pathResult, message))
        return false;

    QString selectionMessage;
    context.DataSelection()->SelectCatalogEntry(entryId, &selectionMessage);
    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());

    return true;
}

bool RunEditControlPoints(xq::core::ApplicationContext& context,
                          xq::core::RenderRefreshService* renderRefresh,
                          const xq::core::WorkflowContextSnapshot& snapshot,
                          QString* message)
{
    const auto sourceNode = ResolveSourceNode(context, snapshot);
    auto* path = sourceNode.IsNotNull()
                     ? dynamic_cast<xq_VesselCenterline*>(sourceNode->GetData())
                     : nullptr;
    auto* segment = path ? path->GetSegment(0) : nullptr;
    if (sourceNode.IsNull() ||
        !xq::pipeline::HasStage(sourceNode.GetPointer(),
                                xq::pipeline::Stage::Path) ||
        !path ||
        !segment)
    {
        SetMessage(message,
                   QStringLiteral(
                       "Active path node is required for control point editing."));
        return false;
    }

    if (sourceNode->GetDataInteractor().IsNull())
    {
        auto interactor = xq_CenterlineInteractor::New();
        auto* module = us::ModuleRegistry::GetModule("xqModulePath");
        interactor->LoadStateMachine("xq_PathInteraction.xml", module);
        interactor->SetEventConfig("xq_PathConfig.xml", module);
        interactor->SetDataNode(sourceNode);
    }

    sourceNode->SetBoolProperty("xq.path.editable", true);
    sourceNode->SetBoolProperty("path.show.control.points", true);
    sourceNode->SetBoolProperty("xq.path.editing.enabled", true);
    sourceNode->SetStringProperty("xq.path.operation",
                                  "edit-control-points");
    sourceNode->SetStringProperty(
        "xq.path.capability.diagnostic",
        "Edit Control Points enabled the Path interactor on the selected "
        "node. Anchor insert, move, and delete are handled by interaction "
        "events; no generated data entry was created.");

    QString selectionMessage;
    context.DataSelection()->SelectCatalogEntry(snapshot.SelectedCatalogEntryId,
                                                &selectionMessage);
    if (renderRefresh)
        renderRefresh->RefreshDataStorage(context.DataStorage());

    SetMessage(message, QStringLiteral("Path control point editing enabled."));
    return true;
}

} // namespace

bool RegisterDynamicPathWorkflowActionHandler(
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
                           QStringLiteral("Path operation id is required."));
                return false;
            }

            if (operationId !=
                    QString::fromLatin1(kCreateCenterlineOperationId) &&
                operationId !=
                    QString::fromLatin1(kEditControlPointsOperationId) &&
                operationId != QString::fromLatin1(kSmoothPathOperationId))
            {
                return RunUnsupportedPathOperation(operations,
                                                   snapshot,
                                                   operationId,
                                                   taskMessage);
            }

            if (operationId ==
                QString::fromLatin1(kEditControlPointsOperationId))
            {
                return RunEditControlPoints(context,
                                            renderRefresh,
                                            snapshot,
                                            taskMessage);
            }

            if (operationId == QString::fromLatin1(kSmoothPathOperationId))
            {
                return RunSmoothPath(context,
                                     renderRefresh,
                                     snapshot,
                                     operationId,
                                     taskMessage);
            }

            return RunCreateCenterline(context,
                                       renderRefresh,
                                       snapshot,
                                       operationId,
                                       taskMessage);
        };

    return context.WorkflowActions()->RegisterHandler(
        QString::fromLatin1(kPathWorkflowId),
        handler,
        message);
}

} // namespace xq::infrastructure
