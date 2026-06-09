#include "Infrastructure/xq_PathWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_RenderRefreshService.h"
#include "Core/xq_TaskRunner.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"

#include <xq_CenterlineSegment.h>
#include <xq_PipelineDataUtils.h>
#include <xq_VesselCenterline.h>

#include <QCoreApplication>
#include <QVariantList>

#include <mitkDataNode.h>
#include <mitkImage.h>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <iostream>
#include <memory>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

vtkSmartPointer<vtkImageData> MakeVtkImage()
{
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(25, 25, 1);
    image->SetSpacing(1.0, 1.0, 1.0);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_DOUBLE, 1);

    for (int y = 0; y < 25; ++y)
    {
        for (int x = 0; x < 25; ++x)
        {
            auto* voxel =
                static_cast<double*>(image->GetScalarPointer(x, y, 0));
            *voxel = 1.0;
            if ((x >= 2 && x <= 18 && y == 10) ||
                (x == 18 && y >= 10 && y <= 18))
            {
                *voxel = 100.0;
            }
        }
    }

    return image;
}

mitk::Image::Pointer MakeMitkImage(vtkImageData* source)
{
    auto image = mitk::Image::New();
    image->Initialize(source);
    auto* output = image->GetVtkImageData();
    if (output)
        output->DeepCopy(source);
    return image;
}

mitk::DataNode::Pointer MakeSourceNode()
{
    auto node = mitk::DataNode::New();
    node->SetName("Path CTA");
    node->SetData(MakeMitkImage(MakeVtkImage()));
    return node;
}

xq::core::DataImportRequest MakeImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("path-image");
    request.SourcePath = QStringLiteral("C:/studies/path-image.nii");
    request.DisplayName = QStringLiteral("Path CTA");
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

QVariantList Point(double x, double y, double z)
{
    return QVariantList{x, y, z};
}

mitk::Point3D PathPoint(double x, double y, double z)
{
    mitk::Point3D point;
    point[0] = x;
    point[1] = y;
    point[2] = z;
    return point;
}

QVariantList SeedPoints()
{
    return QVariantList{Point(2.0, 10.0, 0.0), Point(18.0, 18.0, 0.0)};
}

mitk::DataNode::Pointer MakePathNode(const std::string& name)
{
    auto* segment = new xq_CenterlineSegment();
    segment->ReplaceAnchors({PathPoint(0.0, 0.0, 0.0),
                             PathPoint(5.0, 3.0, 0.0),
                             PathPoint(10.0, 0.0, 0.0)});

    auto centerline = xq_VesselCenterline::New();
    centerline->SetSegment(segment);

    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(centerline);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Path);
    xq::pipeline::SetStringProperty(node,
                                    xq::pipeline::kSourceImageProperty,
                                    "Path CTA");
    return node;
}

bool PreparePathWorkflow(xq::core::ApplicationContext& context)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(QStringLiteral("path")))
        return false;

    const auto importResult = context.DataImports()->Import(MakeImport(),
                                                            &message);
    return importResult.Succeeded;
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

bool PrepareSmoothPathWorkflow(xq::core::ApplicationContext& context)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(QStringLiteral("path")))
        return false;

    const auto importResult = context.DataImports()->Import(MakePathImport(),
                                                            &message);
    if (!importResult.Succeeded)
        return false;

    if (!context.WorkflowOperations()->SelectOperation(
            QStringLiteral("path"),
            QStringLiteral("smooth-path"),
            &message))
    {
        return false;
    }

    if (!context.WorkflowOperations()->SetParameterValue(
            QStringLiteral("path"),
            QStringLiteral("smooth-path"),
            QStringLiteral("smoothing-factor"),
            0.75,
            &message))
    {
        return false;
    }

    return context.WorkflowOperations()->SetParameterValue(
        QStringLiteral("path"),
        QStringLiteral("smooth-path"),
        QStringLiteral("iteration-count"),
        16,
        &message);
}

bool PreparePathWorkflow(xq::core::ApplicationContext& context,
                         const QString& operationId)
{
    if (!PreparePathWorkflow(context))
        return false;

    QString message;
    return context.WorkflowOperations()->SelectOperation(
        QStringLiteral("path"),
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
        if (Expect(xq::infrastructure::RegisterDynamicPathWorkflowActionHandler(
                       *context,
                       nullptr,
                       &message),
                   "dynamic path handler should register"))
            return 1;
        if (Expect(context->WorkflowActions()->HasHandler(
                       QStringLiteral("path")),
                   "dynamic path handler should be discoverable"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PreparePathWorkflow(*context),
                   "missing seed fixture should prepare path workflow"))
            return 1;

        QString message;
        if (Expect(xq::infrastructure::RegisterDynamicPathWorkflowActionHandler(
                       *context,
                       nullptr,
                       &message),
                   "missing seed fixture should install handler"))
            return 1;

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "path handler should reject missing seed points"))
            return 1;
        if (Expect(message ==
                       QStringLiteral("Path creation requires at least two seed points."),
                   "missing seed diagnostic should be specific"))
            return 1;
        if (Expect(context->DataCatalog()->FindById(
                       QStringLiteral("path-image-create-centerline")) ==
                       nullptr,
                   "failed path handler should not register result catalog"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PreparePathWorkflow(*context),
                   "valid path fixture should prepare workflow"))
            return 1;

        auto sourceNode = MakeSourceNode();
        context->DataStorage()->Add(sourceNode);
        context->DataNodes()->BindNode(QStringLiteral("path-image"),
                                       sourceNode);

        QString message;
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("path"),
                       QStringLiteral("create-centerline"),
                       QStringLiteral("seed-points"),
                       SeedPoints(),
                       &message),
                   "valid path fixture should set seed points"))
            return 1;
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("path"),
                       QStringLiteral("create-centerline"),
                       QStringLiteral("control-point-count"),
                       12,
                       &message),
                   "valid path fixture should set sample count"))
            return 1;

        FakeRenderRefreshService refresh;
        if (Expect(xq::infrastructure::RegisterDynamicPathWorkflowActionHandler(
                       *context,
                       &refresh,
                       &message),
                   "valid path fixture should install handler"))
            return 1;

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "path handler should create centerline path"))
            return 1;
        if (Expect(message ==
                       QStringLiteral("Registered path result catalog entry."),
                   "path handler should report catalog commit success"))
            return 1;
        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("path-image-create-centerline"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::Path &&
                       entry->SourcePath ==
                           QStringLiteral("xq://generated/path/path-image-create-centerline"),
                   "path handler should register generated catalog entry"))
            return 1;
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-path-image-create-centerline"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("path-image-create-centerline"),
                   "path handler should register generated hierarchy entry"))
            return 1;
        if (Expect(context->DataNodes()->FindNode(
                       QStringLiteral("path-image-create-centerline"))
                       .IsNotNull(),
                   "path handler should bind generated node"))
            return 1;
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("path-image-create-centerline"),
                   "path handler should select generated path"))
            return 1;
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "path handler should refresh rendering after success"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PreparePathWorkflow(*context,
                                       QStringLiteral("edit-control-points")),
                   "edit path fixture should prepare workflow"))
            return 1;

        auto pathNode = MakePathNode("Path CTA");
        context->DataStorage()->Add(pathNode);
        context->DataNodes()->BindNode(QStringLiteral("path-image"),
                                       pathNode);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(xq::infrastructure::RegisterDynamicPathWorkflowActionHandler(
                       *context,
                       &refresh,
                       &message),
                   "edit path fixture should install handler"))
            return 1;

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "edit control points should enable path editing"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message == QStringLiteral(
                                  "Path control point editing enabled."),
                   "edit control points should report editing enabled"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }

        bool editable = false;
        bool showControlPoints = false;
        bool editingEnabled = false;
        std::string operation;
        if (Expect(pathNode->GetBoolProperty("xq.path.editable",
                                             editable) &&
                       editable &&
                       pathNode->GetBoolProperty(
                           "path.show.control.points",
                           showControlPoints) &&
                       showControlPoints &&
                       pathNode->GetBoolProperty(
                           "xq.path.editing.enabled",
                           editingEnabled) &&
                       editingEnabled &&
                       pathNode->GetStringProperty("xq.path.operation",
                                                   operation) &&
                       operation == "edit-control-points",
                   "edit control points should mark path node editable"))
            return 1;
        if (Expect(pathNode->GetDataInteractor().IsNotNull(),
                   "edit control points should attach a data interactor"))
            return 1;
        if (Expect(context->DataCatalog()->FindById(
                       QStringLiteral("path-image-edit-control-points")) ==
                       nullptr,
                   "edit control points should not register generated catalog entry"))
            return 1;
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("path-image"),
                   "edit control points should keep selected path"))
            return 1;
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "edit control points should refresh rendering after success"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PreparePathWorkflow(
                       *context,
                       QStringLiteral("edit-control-points")),
                   "missing edit path fixture should prepare workflow"))
            return 1;

        QString message;
        if (Expect(xq::infrastructure::RegisterDynamicPathWorkflowActionHandler(
                       *context,
                       nullptr,
                       &message),
                   "missing edit path fixture should install handler"))
            return 1;

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "edit control points should reject missing path node"))
            return 1;
        if (Expect(message == QStringLiteral(
                                  "Active path node is required for control point editing."),
                   "missing edit path node diagnostic should be specific"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareSmoothPathWorkflow(*context),
                   "smooth path fixture should prepare workflow"))
            return 1;

        auto pathNode = MakePathNode("Main Path");
        context->DataStorage()->Add(pathNode);
        context->DataNodes()->BindNode(QStringLiteral("path-001"),
                                       pathNode);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(xq::infrastructure::RegisterDynamicPathWorkflowActionHandler(
                       *context,
                       &refresh,
                       &message),
                   "smooth path fixture should install handler"))
            return 1;

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "smooth path should create generated path"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral("Registered path result catalog entry."),
                   "smooth path should report catalog commit success"))
            return 1;

        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("path-001-smooth-path"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::Path &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/path/path-001-smooth-path"),
                   "smooth path should register generated catalog entry"))
            return 1;

        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-path-001-smooth-path"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("path-001-smooth-path"),
                   "smooth path should register generated hierarchy entry"))
            return 1;

        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("path-001-smooth-path"));
        auto* path = resultNode.IsNotNull()
                         ? dynamic_cast<xq_VesselCenterline*>(
                               resultNode->GetData())
                         : nullptr;
        auto* segment = path ? path->GetSegment() : nullptr;
        if (Expect(path != nullptr &&
                       segment != nullptr &&
                       segment->GetTraceVertexCount() >= 2,
                   "smooth path should bind generated centerline geometry"))
            return 1;

        std::string smoothing;
        std::string operation;
        int sampleCount = 0;
        if (Expect(resultNode.IsNotNull() &&
                       resultNode->GetStringProperty(
                           "xq.params.path.smoothing",
                           smoothing) &&
                       smoothing == "spline" &&
                       resultNode->GetStringProperty("xq.path.operation",
                                                     operation) &&
                       operation == "smooth-path" &&
                       resultNode->GetIntProperty(
                           "xq.path.calculation_number",
                           sampleCount) &&
                       sampleCount == 16,
                   "smooth path should record smoothing metadata"))
            return 1;

        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("path-001-smooth-path"),
                   "smooth path should select generated path"))
            return 1;
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "smooth path should refresh rendering after success"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareSmoothPathWorkflow(*context),
                   "missing smooth path node fixture should prepare workflow"))
            return 1;

        QString message;
        if (Expect(xq::infrastructure::RegisterDynamicPathWorkflowActionHandler(
                       *context,
                       nullptr,
                       &message),
                   "missing smooth path node fixture should install handler"))
            return 1;

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "smooth path should reject missing path node"))
            return 1;
        if (Expect(message == QStringLiteral(
                                  "Active path node is required for path smoothing."),
                   "missing smooth path node diagnostic should be specific"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    return 0;
}
