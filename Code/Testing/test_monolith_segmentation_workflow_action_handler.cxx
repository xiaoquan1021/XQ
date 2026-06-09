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
#include <xq_ContourGroup.h>
#include <xq_LumenSurface.h>
#include <xq_MitkSeg3D.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ProfileGroup.h>
#include <xq_VesselCenterline.h>

#include <QCoreApplication>

#include <mitkDataNode.h>
#include <mitkImage.h>

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSphereSource.h>
#include <vtkSmartPointer.h>

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

mitk::DataNode::Pointer MakeThresholdPathNode(const std::string& name)
{
    auto* segment = new xq_CenterlineSegment();
    segment->SetSampleDensity(9);
    segment->ReplaceAnchors({Point(0.0, 0.0, -5.0),
                             Point(0.0, 0.0, 5.0)});

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

vtkSmartPointer<vtkImageData> MakeRegionGrowingVtkImage()
{
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(5, 5, 5);
    image->SetSpacing(1.0, 1.0, 1.0);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_FLOAT, 1);

    for (int z = 0; z < 5; ++z)
    {
        for (int y = 0; y < 5; ++y)
        {
            for (int x = 0; x < 5; ++x)
            {
                auto* voxel =
                    static_cast<float*>(image->GetScalarPointer(x, y, z));
                const bool inside =
                    x >= 1 && x <= 3 &&
                    y >= 1 && y <= 3 &&
                    z >= 1 && z <= 3;
                *voxel = inside ? 100.0f : 0.0f;
            }
        }
    }

    return image;
}

mitk::Image::Pointer MakeThresholdContourMitkImage()
{
    auto source = vtkSmartPointer<vtkImageData>::New();
    source->SetDimensions(25, 25, 25);
    source->SetSpacing(1.0, 1.0, 1.0);
    source->SetOrigin(-12.0, -12.0, -12.0);
    source->AllocateScalars(VTK_DOUBLE, 1);

    for (int z = 0; z < 25; ++z)
    {
        for (int y = 0; y < 25; ++y)
        {
            for (int x = 0; x < 25; ++x)
            {
                const double worldX = -12.0 + static_cast<double>(x);
                const double worldY = -12.0 + static_cast<double>(y);
                const double worldZ = -12.0 + static_cast<double>(z);
                const double radiusSquared =
                    worldX * worldX + worldY * worldY + worldZ * worldZ;
                source->SetScalarComponentFromDouble(
                    x,
                    y,
                    z,
                    0,
                    radiusSquared <= 36.0 ? 100.0 : 0.0);
            }
        }
    }

    auto image = mitk::Image::New();
    image->Initialize(source);
    auto* output = image->GetVtkImageData();
    if (output)
        output->DeepCopy(source);
    return image;
}

mitk::Image::Pointer MakeRegionGrowingMitkImage()
{
    auto source = MakeRegionGrowingVtkImage();
    auto image = mitk::Image::New();
    image->Initialize(source);
    auto* output = image->GetVtkImageData();
    if (output)
        output->DeepCopy(source);
    return image;
}

mitk::DataNode::Pointer MakeImageNode(const std::string& name)
{
    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(MakeRegionGrowingMitkImage());
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Image);
    return node;
}

mitk::DataNode::Pointer MakeThresholdImageNode(const std::string& name)
{
    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(MakeThresholdContourMitkImage());
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Image);
    return node;
}

mitk::DataNode::Pointer MakeSegmentation3DNode(const std::string& name)
{
    auto sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetRadius(4.0);
    sphere->SetThetaResolution(16);
    sphere->SetPhiResolution(12);
    sphere->Update();

    auto surface = vtkSmartPointer<vtkPolyData>::New();
    surface->DeepCopy(sphere->GetOutput());

    auto segmentation = xq_MitkSeg3D::New();
    segmentation->SetMethod(xq_MitkSeg3D::Seg3DMethod::THRESHOLD);
    segmentation->SetLowerThreshold(50.0);
    segmentation->SetUpperThreshold(150.0);
    segmentation->SetSurfaceMesh(surface);

    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(segmentation);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Segmentation3D);
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

xq::core::DataImportRequest MakeImageImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("image-001");
    request.SourcePath = QStringLiteral("C:/studies/image-001.nii");
    request.DisplayName = QStringLiteral("CTA Image");
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

xq::core::DataImportRequest MakeSegmentationImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("seg3d-001");
    request.SourcePath = QStringLiteral("C:/studies/seg3d-001.xqseg3d");
    request.DisplayName = QStringLiteral("CTA Threshold Segmentation");
    request.Modality = QStringLiteral("Segmentation");
    request.WorkflowRole = xq::core::DataWorkflowRole::Segmentation;
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

bool PrepareSegmentation3DWorkflow(xq::core::ApplicationContext& context,
                                   const QString& operationId)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("segmentation-3d")))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SelectOperation(
            QStringLiteral("segmentation-3d"),
            operationId,
            &message))
    {
        return false;
    }

    const auto importResult = context.DataImports()->Import(MakeImageImport(),
                                                            &message);
    return importResult.Succeeded;
}

bool PrepareSegmentation3DPreviewWorkflow(
    xq::core::ApplicationContext& context)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("segmentation-3d")))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SelectOperation(
            QStringLiteral("segmentation-3d"),
            QStringLiteral("surface-preview"),
            &message))
    {
        return false;
    }

    const auto importResult =
        context.DataImports()->Import(MakeSegmentationImport(), &message);
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

bool PrepareThresholdContourWorkflow(xq::core::ApplicationContext& context)
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
            QStringLiteral("threshold-contour"),
            &message))
    {
        return false;
    }

    auto pathImport = MakePathImport();
    pathImport.DisplayName = QStringLiteral("Threshold Path");
    const auto pathResult = context.DataImports()->Import(pathImport,
                                                          &message);
    if (!pathResult.Succeeded)
        return false;

    const auto imageResult = context.DataImports()->Import(MakeImageImport(),
                                                           &message);
    if (!imageResult.Succeeded)
        return false;

    return context.DataSelection()->SelectCatalogEntry(pathResult.EntryId,
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
                   "missing threshold source image fixture should prepare workflow"))
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
                "missing threshold source image fixture should install handler"))
        {
            return 1;
        }

        auto pathNode = MakePathNode("Main Path");
        pathNode->SetStringProperty(xq::pipeline::kSourceImageProperty, "");
        context->DataStorage()->Add(pathNode);
        context->DataNodes()->BindNode(QStringLiteral("path-001"), pathNode);

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "threshold contour should reject missing source image metadata"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Active path node with source image metadata is required for threshold contour segmentation."),
                   "missing threshold source image diagnostic should be specific"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareThresholdContourWorkflow(*context),
                   "threshold contour fixture should prepare workflow"))
        {
            return 1;
        }
        QString message;
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("segmentation-2d"),
                       QStringLiteral("threshold-contour"),
                       QStringLiteral("threshold-lower"),
                       25.0,
                       &message),
                   "threshold contour fixture should set lower threshold"))
        {
            return 1;
        }
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("segmentation-2d"),
                       QStringLiteral("threshold-contour"),
                       QStringLiteral("threshold-upper"),
                       50.0,
                       &message),
                   "threshold contour fixture should set upper threshold"))
        {
            return 1;
        }

        auto pathNode = MakeThresholdPathNode("Threshold Path");
        context->DataStorage()->Add(pathNode);
        context->DataNodes()->BindNode(QStringLiteral("path-001"), pathNode);

        auto imageNode = MakeThresholdImageNode("CTA Image");
        context->DataStorage()->Add(imageNode);
        context->DataNodes()->BindNode(QStringLiteral("image-001"), imageNode);

        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicSegmentationWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "threshold contour fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "threshold contour should create contour group result"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered segmentation result catalog entry."),
                   "threshold contour should report catalog commit success"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }

        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("path-001-threshold-contour"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::Segmentation &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/segmentation/path-001-threshold-contour"),
                   "threshold contour should register generated catalog entry"))
        {
            return 1;
        }

        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("path-001-threshold-contour"));
        auto* contourGroup = resultNode.IsNotNull()
                                 ? dynamic_cast<xq_ContourGroup*>(
                                       resultNode->GetData())
                                 : nullptr;
        if (Expect(contourGroup != nullptr &&
                       contourGroup->GetContourCount() > 0,
                   "threshold contour should bind non-empty contour group"))
        {
            return 1;
        }

        double lowerThreshold = 0.0;
        double upperThreshold = 0.0;
        bool rangeCollapsed = false;
        std::string algorithm;
        std::string sourceImage;
        std::string sourcePath;
        if (Expect(
                xq::pipeline::HasStage(resultNode,
                                       xq::pipeline::Stage::ContourGroup) &&
                    xq::pipeline::GetStringProperty(
                        resultNode.GetPointer(),
                        xq::pipeline::kAlgorithmProperty) == "threshold" &&
                    xq::pipeline::GetStringProperty(
                        resultNode.GetPointer(),
                        xq::pipeline::kSourceImageProperty) == "CTA Image" &&
                    xq::pipeline::GetStringProperty(
                        resultNode.GetPointer(),
                        xq::pipeline::kSourcePathProperty) ==
                        "Threshold Path" &&
                    resultNode->GetStringProperty("xq.segmentation.method",
                                                  algorithm) &&
                    algorithm == "threshold-contour" &&
                    resultNode->GetDoubleProperty(
                        "xq.segmentation.threshold.lower",
                        lowerThreshold) &&
                    lowerThreshold == 25.0 &&
                    resultNode->GetDoubleProperty(
                        "xq.segmentation.threshold.upper",
                        upperThreshold) &&
                    upperThreshold == 50.0 &&
                    resultNode->GetBoolProperty(
                        "xq.segmentation.threshold.range_collapsed",
                        rangeCollapsed) &&
                    rangeCollapsed,
                "threshold contour should record source and threshold metadata"))
        {
            return 1;
        }

        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("path-001-threshold-contour"),
                   "threshold contour should select generated segmentation"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "threshold contour should refresh rendering after success"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareSegmentation3DWorkflow(
                       *context,
                       QStringLiteral("threshold-region")),
                   "3D threshold region fixture should prepare workflow"))
        {
            return 1;
        }
        QString message;
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("segmentation-3d"),
                       QStringLiteral("threshold-region"),
                       QStringLiteral("threshold-lower"),
                       50.0,
                       &message),
                   "3D threshold region fixture should set lower threshold"))
        {
            return 1;
        }
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("segmentation-3d"),
                       QStringLiteral("threshold-region"),
                       QStringLiteral("threshold-upper"),
                       150.0,
                       &message),
                   "3D threshold region fixture should set upper threshold"))
        {
            return 1;
        }

        auto imageNode = MakeImageNode("CTA Image");
        context->DataStorage()->Add(imageNode);
        context->DataNodes()->BindNode(QStringLiteral("image-001"), imageNode);

        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicSegmentationWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "3D threshold region fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "3D threshold region should create segmentation result"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered 3D segmentation result catalog entry."),
                   "3D threshold region should report catalog commit success"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("image-001-threshold-region"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::Segmentation &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/segmentation/image-001-threshold-region"),
                   "3D threshold region should register generated catalog entry"))
        {
            return 1;
        }
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-image-001-threshold-region"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("image-001-threshold-region"),
                   "3D threshold region should register hierarchy entry"))
        {
            return 1;
        }
        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("image-001-threshold-region"));
        auto* seg3d = resultNode.IsNotNull()
                          ? dynamic_cast<xq_MitkSeg3D*>(resultNode->GetData())
                          : nullptr;
        if (Expect(seg3d != nullptr,
                   "3D threshold region should bind xq_MitkSeg3D data"))
        {
            return 1;
        }
        if (Expect(seg3d->GetMethod() ==
                       xq_MitkSeg3D::Seg3DMethod::THRESHOLD &&
                       seg3d->GetSeedPoints().empty() &&
                       seg3d->GetLowerThreshold() == 50.0 &&
                       seg3d->GetUpperThreshold() == 150.0,
                   "3D threshold region should preserve method and thresholds"))
        {
            return 1;
        }
        if (Expect(seg3d->GetSurfaceMesh() != nullptr &&
                       seg3d->GetSurfaceMesh()->GetNumberOfPoints() > 0,
                   "3D threshold region should attach extracted surface mesh"))
        {
            return 1;
        }
        if (Expect(xq::pipeline::HasStage(resultNode,
                                          xq::pipeline::Stage::Segmentation3D) &&
                       xq::pipeline::GetStringProperty(
                           resultNode.GetPointer(),
                           xq::pipeline::kAlgorithmProperty) ==
                           "threshold-region" &&
                       xq::pipeline::GetStringProperty(
                           resultNode.GetPointer(),
                           xq::pipeline::kSourceImageProperty) ==
                           "CTA Image",
                   "3D threshold region should mark pipeline metadata"))
        {
            return 1;
        }
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("image-001-threshold-region"),
                   "3D threshold region should select generated segmentation"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "3D threshold region should refresh rendering after success"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareSegmentation3DWorkflow(
                       *context,
                       QStringLiteral("region-growing")),
                   "3D region growing fixture should prepare workflow"))
        {
            return 1;
        }
        QString message;
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("segmentation-3d"),
                       QStringLiteral("region-growing"),
                       QStringLiteral("seed-x"),
                       2,
                       &message),
                   "3D region growing fixture should set seed-x"))
        {
            return 1;
        }
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("segmentation-3d"),
                       QStringLiteral("region-growing"),
                       QStringLiteral("seed-y"),
                       2,
                       &message),
                   "3D region growing fixture should set seed-y"))
        {
            return 1;
        }
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("segmentation-3d"),
                       QStringLiteral("region-growing"),
                       QStringLiteral("threshold-upper"),
                       125.0,
                       &message),
                   "3D region growing fixture should set threshold-upper"))
        {
            return 1;
        }

        auto imageNode = MakeImageNode("CTA Image");
        context->DataStorage()->Add(imageNode);
        context->DataNodes()->BindNode(QStringLiteral("image-001"), imageNode);

        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicSegmentationWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "3D region growing fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "3D region growing should create segmentation result"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered 3D segmentation result catalog entry."),
                   "3D region growing should report catalog commit success"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("image-001-region-growing"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::Segmentation &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/segmentation/image-001-region-growing"),
                   "3D region growing should register generated catalog entry"))
        {
            return 1;
        }
        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("image-001-region-growing"));
        auto* seg3d = resultNode.IsNotNull()
                          ? dynamic_cast<xq_MitkSeg3D*>(resultNode->GetData())
                          : nullptr;
        if (Expect(seg3d != nullptr,
                   "3D region growing should bind xq_MitkSeg3D data"))
        {
            return 1;
        }
        const auto seeds = seg3d->GetSeedPoints();
        if (Expect(seg3d->GetMethod() ==
                       xq_MitkSeg3D::Seg3DMethod::REGION_GROWING &&
                       seeds.size() == 1 &&
                       seeds.front()[0] == 2.0 &&
                       seeds.front()[1] == 2.0 &&
                       seeds.front()[2] == 2.0 &&
                       seg3d->GetLowerThreshold() == 100.0 &&
                       seg3d->GetUpperThreshold() == 125.0,
                   "3D region growing should preserve method, seed, and thresholds"))
        {
            return 1;
        }
        if (Expect(seg3d->GetSurfaceMesh() != nullptr &&
                       seg3d->GetSurfaceMesh()->GetNumberOfPoints() > 0,
                   "3D region growing should attach extracted surface mesh"))
        {
            return 1;
        }
        if (Expect(xq::pipeline::HasStage(resultNode,
                                          xq::pipeline::Stage::Segmentation3D) &&
                       xq::pipeline::GetStringProperty(
                           resultNode.GetPointer(),
                           xq::pipeline::kAlgorithmProperty) ==
                           "region-growing" &&
                       xq::pipeline::GetStringProperty(
                           resultNode.GetPointer(),
                           xq::pipeline::kSourceImageProperty) ==
                           "CTA Image",
                   "3D region growing should mark pipeline metadata"))
        {
            return 1;
        }
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("image-001-region-growing"),
                   "3D region growing should select generated segmentation"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "3D region growing should refresh rendering after success"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareSegmentation3DPreviewWorkflow(*context),
                   "3D surface preview fixture should prepare workflow"))
        {
            return 1;
        }
        QString message;
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("segmentation-3d"),
                       QStringLiteral("surface-preview"),
                       QStringLiteral("smoothing-iterations"),
                       8,
                       &message),
                   "3D surface preview fixture should set smoothing iterations"))
        {
            return 1;
        }

        auto segmentationNode =
            MakeSegmentation3DNode("CTA Threshold Segmentation");
        context->DataStorage()->Add(segmentationNode);
        context->DataNodes()->BindNode(QStringLiteral("seg3d-001"),
                                       segmentationNode);

        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicSegmentationWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "3D surface preview fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "3D surface preview should create preview result"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered 3D segmentation result catalog entry."),
                   "3D surface preview should report catalog commit success"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("seg3d-001-surface-preview"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::Segmentation &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/segmentation/seg3d-001-surface-preview"),
                   "3D surface preview should register generated catalog entry"))
        {
            return 1;
        }
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-seg3d-001-surface-preview"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("seg3d-001-surface-preview"),
                   "3D surface preview should register hierarchy entry"))
        {
            return 1;
        }
        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("seg3d-001-surface-preview"));
        auto* preview = resultNode.IsNotNull()
                            ? dynamic_cast<xq_LumenSurface*>(
                                  resultNode->GetData())
                            : nullptr;
        if (Expect(preview != nullptr,
                   "3D surface preview should bind xq_LumenSurface data"))
        {
            return 1;
        }
        auto previewSurface = preview->GetSurfaceMesh();
        if (Expect(previewSurface != nullptr &&
                       previewSurface->GetNumberOfPoints() > 0 &&
                       previewSurface->GetNumberOfCells() > 0,
                   "3D surface preview should attach processed surface mesh"))
        {
            return 1;
        }
        if (Expect(xq::pipeline::HasStage(resultNode,
                                          xq::pipeline::Stage::Segmentation3D) &&
                       xq::pipeline::GetStringProperty(
                           resultNode.GetPointer(),
                           xq::pipeline::kAlgorithmProperty) ==
                           "surface-preview" &&
                       xq::pipeline::GetStringProperty(
                           resultNode.GetPointer(),
                           xq::pipeline::kSourceImageProperty) ==
                           "CTA Image",
                   "3D surface preview should mark pipeline metadata"))
        {
            return 1;
        }
        int smoothingIterations = 0;
        if (Expect(resultNode->GetIntProperty(
                       "xq.segmentation.surface_preview.smoothing_iterations",
                       smoothingIterations) &&
                       smoothingIterations == 8,
                   "3D surface preview should record smoothing metadata"))
        {
            return 1;
        }
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("seg3d-001-surface-preview"),
                   "3D surface preview should select generated preview"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "3D surface preview should refresh rendering after success"))
        {
            return 1;
        }
    }

    return 0;
}
