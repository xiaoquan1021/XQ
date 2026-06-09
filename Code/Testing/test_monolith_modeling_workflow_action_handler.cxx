#include "Infrastructure/xq_ModelingWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_RenderRefreshService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"

#include <xq_CircularProfile.h>
#include <xq_Model.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ProfileGroup.h>
#include <xq_SegmentationUtils.h>

#include <QCoreApplication>

#include <mitkDataNode.h>

#include <vtkPolyData.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

xq_ProfilePlacementFrame MakeAxialFrame(int pathPosIndex, double z)
{
    xq_ProfilePlacementFrame frame;
    frame.pathPosIndex = pathPosIndex;
    frame.position[0] = 0.0;
    frame.position[1] = 0.0;
    frame.position[2] = z;
    frame.tangent.Fill(0.0);
    frame.tangent[2] = 1.0;
    frame.rotation.Fill(0.0);
    frame.rotation[0] = 1.0;
    return frame;
}

xq_CircularProfile* MakeCircle(double radius)
{
    auto* profile = new xq_CircularProfile();
    profile->SetMethod("manual");
    profile->SetRadius(radius);
    mitk::Point3D origin;
    origin.Fill(0.0);
    profile->SetProfileCenter(origin);
    return profile;
}

mitk::DataNode::Pointer MakeSegmentationNode(const std::string& name)
{
    auto group = xq_ProfileGroup::New();
    auto* lower = MakeCircle(3.0);
    auto* upper = MakeCircle(3.0);
    group->AppendProfile(lower, 0);
    group->AppendProfile(upper, 1);
    xq_SegmentationUtils::ApplyPlacementFrame(
        group->GetProfileAtPathPos(0),
        MakeAxialFrame(0, -4.0));
    xq_SegmentationUtils::ApplyPlacementFrame(
        group->GetProfileAtPathPos(1),
        MakeAxialFrame(1, 4.0));
    group->SetAttribute("path_name", "Main Path");
    group->SetLoftedMesh(xq_SegmentationUtils::LoftProfileGroup(group));

    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(group);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::ContourGroup);
    xq::pipeline::SetStringProperty(
        node,
        xq::pipeline::kSourcePathProperty,
        "Main Path");
    return node;
}

xq::core::DataImportRequest MakeSegmentationImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("seg-001");
    request.SourcePath = QStringLiteral("C:/studies/seg-001.xqseg");
    request.DisplayName = QStringLiteral("Main Segmentation");
    request.Modality = QStringLiteral("Segmentation");
    request.WorkflowRole = xq::core::DataWorkflowRole::Segmentation;
    return request;
}

bool PrepareModelingWorkflow(xq::core::ApplicationContext& context)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("modeling")))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SelectOperation(
            QStringLiteral("modeling"),
            QStringLiteral("build-solid-model"),
            &message))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SetParameterValue(
            QStringLiteral("modeling"),
            QStringLiteral("build-solid-model"),
            QStringLiteral("blend-radius"),
            0.0,
            &message))
    {
        return false;
    }

    const auto importResult =
        context.DataImports()->Import(MakeSegmentationImport(), &message);
    return importResult.Succeeded;
}

bool PrepareModelingWorkflow(xq::core::ApplicationContext& context,
                             const QString& operationId)
{
    if (!PrepareModelingWorkflow(context))
        return false;

    QString message;
    return context.WorkflowOperations()->SelectOperation(
        QStringLiteral("modeling"),
        operationId,
        &message);
}

class FakeRenderRefreshService : public xq::core::RenderRefreshService
{
public:
    int Calls = 0;
    mitk::DataStorage::Pointer LastDataStorage;

    void RefreshDataStorage(mitk::DataStorage::Pointer dataStorage) override
    {
        ++Calls;
        LastDataStorage = dataStorage;
    }
};

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicModelingWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "dynamic modeling handler should register"))
        {
            return 1;
        }
        if (Expect(context->WorkflowActions()->HasHandler(
                       QStringLiteral("modeling")),
                   "dynamic modeling handler should be discoverable"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareModelingWorkflow(*context),
                   "missing segmentation fixture should prepare modeling workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicModelingWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "missing segmentation fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "modeling handler should reject missing segmentation node"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Active segmentation node is required for modeling."),
                   "missing segmentation diagnostic should be specific"))
        {
            return 1;
        }
        if (Expect(context->DataCatalog()->FindById(
                       QStringLiteral("seg-001-build-solid-model")) == nullptr,
                   "failed modeling handler should not register result catalog"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareModelingWorkflow(*context),
                   "valid modeling fixture should prepare workflow"))
        {
            return 1;
        }

        auto segmentationNode = MakeSegmentationNode("Main Segmentation");
        context->DataStorage()->Add(segmentationNode);
        context->DataNodes()->BindNode(QStringLiteral("seg-001"),
                                       segmentationNode);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicModelingWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "valid modeling fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "modeling handler should create solid model"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered model result catalog entry."),
                   "modeling handler should report catalog commit success"))
        {
            return 1;
        }
        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("seg-001-build-solid-model"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::Model &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/model/seg-001-build-solid-model"),
                   "modeling handler should register generated catalog entry"))
        {
            return 1;
        }
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-seg-001-build-solid-model"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("seg-001-build-solid-model"),
                   "modeling handler should register generated hierarchy entry"))
        {
            return 1;
        }
        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("seg-001-build-solid-model"));
        auto* model = resultNode.IsNotNull()
                          ? dynamic_cast<xq_Model*>(resultNode->GetData())
                          : nullptr;
        auto* element = model ? model->GetModelElement(0) : nullptr;
        auto surface = element ? element->GetWholeVtkPolyData() : nullptr;
        if (Expect(model != nullptr &&
                       surface != nullptr &&
                       surface->GetNumberOfCells() > 0,
                   "modeling handler should bind generated model geometry"))
        {
            return 1;
        }
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("seg-001-build-solid-model"),
                   "modeling handler should select generated model"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "modeling handler should refresh rendering after success"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareModelingWorkflow(*context,
                                           QStringLiteral("loft-surface")),
                   "loft fixture should prepare modeling workflow"))
        {
            return 1;
        }

        auto segmentationNode = MakeSegmentationNode("Main Segmentation");
        context->DataStorage()->Add(segmentationNode);
        context->DataNodes()->BindNode(QStringLiteral("seg-001"),
                                       segmentationNode);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicModelingWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "loft fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "loft surface should create generated model"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered model result catalog entry."),
                   "loft surface should report catalog commit success"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }

        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("seg-001-loft-surface"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::Model &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/model/seg-001-loft-surface"),
                   "loft surface should register generated model catalog entry"))
        {
            return 1;
        }

        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-seg-001-loft-surface"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("seg-001-loft-surface"),
                   "loft surface should register generated hierarchy entry"))
        {
            return 1;
        }

        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("seg-001-loft-surface"));
        auto* model = resultNode.IsNotNull()
                          ? dynamic_cast<xq_Model*>(resultNode->GetData())
                          : nullptr;
        auto* element = model ? model->GetModelElement(0) : nullptr;
        auto surface = element ? element->GetWholeVtkPolyData() : nullptr;
        if (Expect(model != nullptr &&
                       surface != nullptr &&
                       surface->GetNumberOfCells() > 0,
                   "loft surface should bind generated model geometry"))
        {
            return 1;
        }

        std::string operation;
        bool surfaceOnly = false;
        if (Expect(resultNode.IsNotNull() &&
                       resultNode->GetStringProperty("xq.model.operation",
                                                     operation) &&
                       operation == "loft-surface" &&
                       resultNode->GetBoolProperty("xq.model.surface_only",
                                                   surfaceOnly) &&
                       surfaceOnly,
                   "loft surface should record honest surface metadata"))
        {
            return 1;
        }

        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("seg-001-loft-surface"),
                   "loft surface should select generated model"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "loft surface should refresh rendering after success"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareModelingWorkflow(*context,
                                           QStringLiteral("loft-surface")),
                   "missing loft segmentation fixture should prepare modeling workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicModelingWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "missing loft segmentation fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "loft surface should reject missing segmentation node"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Active segmentation node is required for modeling."),
                   "missing loft segmentation diagnostic should be specific"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareModelingWorkflow(*context,
                                           QStringLiteral("trim-branches")),
                   "trim fixture should prepare modeling workflow"))
        {
            return 1;
        }

        auto segmentationNode = MakeSegmentationNode("Main Segmentation");
        context->DataStorage()->Add(segmentationNode);
        context->DataNodes()->BindNode(QStringLiteral("seg-001"),
                                       segmentationNode);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicModelingWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "trim fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "trim branches should create filtered model"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered model result catalog entry."),
                   "trim branches should report catalog commit success"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }

        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("seg-001-trim-branches"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::Model &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/model/seg-001-trim-branches"),
                   "trim branches should register generated model catalog entry"))
        {
            return 1;
        }

        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("seg-001-trim-branches"));
        auto* model = resultNode.IsNotNull()
                          ? dynamic_cast<xq_Model*>(resultNode->GetData())
                          : nullptr;
        auto* element = model ? model->GetModelElement(0) : nullptr;
        auto surface = element ? element->GetWholeVtkPolyData() : nullptr;
        if (Expect(model != nullptr &&
                       surface != nullptr &&
                       surface->GetNumberOfCells() > 0,
                   "trim branches should bind generated model geometry"))
        {
            return 1;
        }

        std::string operation;
        std::string trimPath;
        bool filterOnly = false;
        if (Expect(resultNode.IsNotNull() &&
                       resultNode->GetStringProperty("xq.model.operation",
                                                     operation) &&
                       operation == "trim-branches" &&
                       resultNode->GetStringProperty("xq.model.trim.path",
                                                     trimPath) &&
                       trimPath == "Main Path" &&
                       resultNode->GetBoolProperty("xq.model.trim.filter_only",
                                                   filterOnly) &&
                       filterOnly,
                   "trim branches should record honest trim filter metadata"))
        {
            return 1;
        }

        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("seg-001-trim-branches"),
                   "trim branches should select generated model"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "trim branches should refresh rendering after success"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareModelingWorkflow(*context,
                                           QStringLiteral("trim-branches")),
                   "missing trim segmentation fixture should prepare modeling workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicModelingWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "missing trim segmentation fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "trim branches should reject missing segmentation node"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Active segmentation node is required for modeling."),
                   "missing trim segmentation diagnostic should be specific"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    return 0;
}
