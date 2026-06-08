#include "Infrastructure/xq_SegmentationWorkflowActionHandler.h"

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

#include <xq_CenterlineSegment.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ProfileGroup.h>
#include <xq_VesselCenterline.h>

#include <QCoreApplication>

#include <mitkDataNode.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

mitk::Point3D Point(double x, double y, double z)
{
    mitk::Point3D point;
    point[0] = x;
    point[1] = y;
    point[2] = z;
    return point;
}

mitk::DataNode::Pointer MakePathNode(const std::string& name)
{
    auto* segment = new xq_CenterlineSegment();
    segment->ReplaceAnchors({Point(0.0, 0.0, 0.0),
                             Point(5.0, 0.0, 0.0),
                             Point(10.0, 0.0, 0.0)});

    auto centerline = xq_VesselCenterline::New();
    centerline->SetSegment(segment);

    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(centerline);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Path);
    xq::pipeline::SetStringProperty(
        node,
        xq::pipeline::kSourceImageProperty,
        "CTA Image");
    return node;
}

xq::core::DataImportRequest MakePathImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("path-001");
    request.SourcePath = QStringLiteral("C:/studies/path-001.xqpth");
    request.DisplayName = QStringLiteral("Main Path");
    request.Modality = QStringLiteral("Path");
    request.WorkflowRole = xq::core::DataWorkflowRole::Path;
    return request;
}

bool PrepareSegmentationWorkflow(xq::core::ApplicationContext& context)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("segmentation-2d")))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SelectOperation(
            QStringLiteral("segmentation-2d"),
            QStringLiteral("manual-contour"),
            &message))
    {
        return false;
    }

    const auto importResult = context.DataImports()->Import(MakePathImport(),
                                                            &message);
    return importResult.Succeeded;
}

bool PrepareSegmentationWorkflow(xq::core::ApplicationContext& context,
                                 const QString& workflowId,
                                 const QString& operationId)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(workflowId))
        return false;
    if (!context.WorkflowOperations()->SelectOperation(workflowId,
                                                       operationId,
                                                       &message))
    {
        return false;
    }

    const auto importResult = context.DataImports()->Import(MakePathImport(),
                                                            &message);
    return importResult.Succeeded;
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
                    RegisterDynamicSegmentationWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "dynamic segmentation handler should register"))
        {
            return 1;
        }
        if (Expect(context->WorkflowActions()->HasHandler(
                       QStringLiteral("segmentation-2d")),
                   "dynamic segmentation handler should be discoverable"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareSegmentationWorkflow(*context),
                   "missing path fixture should prepare segmentation workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicSegmentationWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "missing path fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "segmentation handler should reject missing path node"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Active path node is required for 2D segmentation."),
                   "missing path diagnostic should be specific"))
        {
            return 1;
        }
        if (Expect(context->DataCatalog()->FindById(
                       QStringLiteral("path-001-manual-contour")) == nullptr,
                   "failed segmentation handler should not register result catalog"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareSegmentationWorkflow(*context),
                   "valid segmentation fixture should prepare workflow"))
        {
            return 1;
        }

        auto pathNode = MakePathNode("Main Path");
        context->DataStorage()->Add(pathNode);
        context->DataNodes()->BindNode(QStringLiteral("path-001"), pathNode);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicSegmentationWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "valid segmentation fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "segmentation handler should create contour group"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered segmentation result catalog entry."),
                   "segmentation handler should report catalog commit success"))
        {
            return 1;
        }
        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("path-001-manual-contour"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::Segmentation &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/segmentation/path-001-manual-contour"),
                   "segmentation handler should register generated catalog entry"))
        {
            return 1;
        }
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-path-001-manual-contour"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("path-001-manual-contour"),
                   "segmentation handler should register generated hierarchy entry"))
        {
            return 1;
        }
        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("path-001-manual-contour"));
        if (Expect(resultNode.IsNotNull() &&
                       dynamic_cast<xq_ProfileGroup*>(
                           resultNode->GetData()) != nullptr,
                   "segmentation handler should bind generated profile group"))
        {
            return 1;
        }
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("path-001-manual-contour"),
                   "segmentation handler should select generated segmentation"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "segmentation handler should refresh rendering after success"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareSegmentationWorkflow(
                       *context,
                       QStringLiteral("segmentation-2d"),
                       QStringLiteral("threshold-contour")),
                   "unsupported 2D segmentation fixture should prepare workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicSegmentationWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "unsupported 2D segmentation fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "unsupported 2D segmentation should fail"))
        {
            return 1;
        }
        if (Expect(message == QStringLiteral(
                                  "Threshold Contour is not wired to a native 2D Segmentation runtime yet."),
                   "unsupported 2D segmentation diagnostic should name operation"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareSegmentationWorkflow(
                       *context,
                       QStringLiteral("segmentation-3d"),
                       QStringLiteral("region-growing")),
                   "unsupported 3D segmentation fixture should prepare workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicSegmentationWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "unsupported 3D segmentation fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "unsupported 3D segmentation should fail"))
        {
            return 1;
        }
        if (Expect(message == QStringLiteral(
                                  "Region Growing is not wired to a native 3D Segmentation runtime yet."),
                   "unsupported 3D segmentation diagnostic should name operation"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    return 0;
}
