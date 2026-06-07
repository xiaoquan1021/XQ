#include "Infrastructure/xq_ImagePreprocessingWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_TaskRunner.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowSelectionService.h"

#include <QCoreApplication>
#include <QVariantMap>

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
    image->SetDimensions(5, 5, 5);
    image->AllocateScalars(VTK_FLOAT, 1);

    for (int z = 0; z < 5; ++z)
    {
        for (int y = 0; y < 5; ++y)
        {
            for (int x = 0; x < 5; ++x)
            {
                auto* voxel =
                    static_cast<float*>(image->GetScalarPointer(x, y, z));
                *voxel = static_cast<float>(x + y + z);
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
    auto sourceVtk = MakeVtkImage();
    auto node = mitk::DataNode::New();
    node->SetName("CTA");
    node->SetData(MakeMitkImage(sourceVtk));
    return node;
}

QVariantMap CropParameters()
{
    QVariantMap parameters;
    parameters.insert(QStringLiteral("origin-x"), 1);
    parameters.insert(QStringLiteral("origin-y"), 1);
    parameters.insert(QStringLiteral("origin-z"), 0);
    parameters.insert(QStringLiteral("size-x"), 3);
    parameters.insert(QStringLiteral("size-y"), 2);
    parameters.insert(QStringLiteral("size-z"), 4);
    return parameters;
}

xq::infrastructure::ImagePreprocessingWorkflowActionOptions CropOptions()
{
    xq::infrastructure::ImagePreprocessingWorkflowActionOptions options;
    options.OperationId = QStringLiteral("crop");
    options.Parameters = CropParameters();
    options.ResultSuffix = QStringLiteral("cropped");
    return options;
}

xq::core::DataImportRequest MakeImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("image-001");
    request.SourcePath = QStringLiteral("C:/studies/image-001.nii");
    request.DisplayName = QStringLiteral("CTA Image");
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

bool PrepareImageWorkflow(xq::core::ApplicationContext& context)
{
    QString message;
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("image-preprocessing")))
    {
        return false;
    }

    const auto importResult =
        context.DataImports()->Import(MakeImport(), &message);
    return importResult.Succeeded;
}

int ChildCount(mitk::DataStorage::Pointer storage,
               mitk::DataNode::Pointer parent)
{
    auto children = storage->GetDerivations(parent);
    return children.IsNull() ? 0 : children->Size();
}

mitk::DataNode::Pointer FindChild(mitk::DataStorage::Pointer storage,
                                  mitk::DataNode::Pointer parent,
                                  const std::string& name)
{
    auto children = storage->GetDerivations(parent);
    if (children.IsNull())
        return nullptr;

    for (auto it = children->Begin(); it != children->End(); ++it)
    {
        auto node = it->Value();
        if (node->GetName() == name)
            return node;
    }

    return nullptr;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        xq::infrastructure::ImagePreprocessingWorkflowActionOptions options;
        QString message;
        if (Expect(!xq::infrastructure::
                       RegisterImagePreprocessingWorkflowActionHandler(
                           *context,
                           options,
                           &message),
                   "workflow action registrar should reject missing operation ids"))
            return 1;
        if (Expect(message ==
                       QStringLiteral("Image preprocessing operation id is required."),
                   "missing operation id should use registrar diagnostic"))
            return 1;
        if (Expect(!context->WorkflowActions()->HasHandler(
                       QStringLiteral("image-preprocessing")),
                   "failed registration should not install handler"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareImageWorkflow(*context),
                   "missing active node fixture should prepare workflow"))
            return 1;

        QString message;
        if (Expect(xq::infrastructure::
                       RegisterImagePreprocessingWorkflowActionHandler(
                           *context,
                           CropOptions(),
                           &message),
                   "workflow action registrar should install crop handler"))
            return 1;
        if (Expect(context->WorkflowActions()->HasHandler(
                       QStringLiteral("image-preprocessing")),
                   "registered crop handler should be discoverable"))
            return 1;

        const int historyBeforeRun = context->Tasks()->History().size();
        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "crop handler should fail without active MITK node"))
            return 1;
        if (Expect(message ==
                       QStringLiteral("Active image node is required for image preprocessing."),
                   "missing active node should use handler diagnostic"))
            return 1;
        const auto historyAfterRun = context->Tasks()->History();
        if (Expect(historyAfterRun.size() == historyBeforeRun + 1,
                   "failed crop handler should still record a task"))
            return 1;
        if (Expect(!historyAfterRun.back().Succeeded &&
                       historyAfterRun.back().Message == message,
                   "failed crop task should preserve handler diagnostic"))
            return 1;
        if (Expect(context->DataCatalog()->FindById(
                       QStringLiteral("image-001-crop")) == nullptr,
                   "failed crop handler should not register result catalog"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareImageWorkflow(*context),
                   "registry crop fixture should prepare workflow"))
            return 1;

        auto sourceNode = MakeSourceNode();
        context->DataStorage()->Add(sourceNode);

        QString message;
        if (Expect(context->DataNodes()->BindNode(QStringLiteral("image-001"),
                                                  sourceNode,
                                                  &message),
                   "registry crop fixture should bind selected data node"))
            return 1;
        if (Expect(xq::infrastructure::
                       RegisterImagePreprocessingWorkflowActionHandler(
                           *context,
                           CropOptions(),
                           &message),
                   "registry crop fixture should install handler"))
            return 1;

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "crop handler should resolve source node from registry"))
            return 1;
        if (Expect(message ==
                       QStringLiteral("Registered preprocessing result catalog entry."),
                   "registry crop handler should report application commit success"))
            return 1;

        auto child = FindChild(context->DataStorage(), sourceNode, "CTA_cropped");
        if (Expect(child.IsNotNull(),
                   "registry crop handler should add named result node"))
            return 1;
        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("image-001-crop"));
        if (Expect(entry != nullptr,
                   "registry crop handler should register generated catalog entry"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareImageWorkflow(*context),
                   "valid crop fixture should prepare workflow"))
            return 1;

        auto sourceNode = MakeSourceNode();
        context->DataStorage()->Add(sourceNode);
        context->SetActiveNode(sourceNode);

        QString message;
        if (Expect(xq::infrastructure::
                       RegisterImagePreprocessingWorkflowActionHandler(
                           *context,
                           CropOptions(),
                           &message),
                   "valid crop fixture should install handler"))
            return 1;

        const int historyBeforeRun = context->Tasks()->History().size();
        const int childrenBeforeRun =
            ChildCount(context->DataStorage(), sourceNode);
        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "crop handler should execute through workflow actions"))
            return 1;
        if (Expect(message ==
                       QStringLiteral("Registered preprocessing result catalog entry."),
                   "crop handler should report application commit success"))
            return 1;

        const auto historyAfterRun = context->Tasks()->History();
        if (Expect(historyAfterRun.size() == historyBeforeRun + 1 &&
                       historyAfterRun.back().Succeeded,
                   "successful crop handler should record successful task"))
            return 1;
        if (Expect(ChildCount(context->DataStorage(), sourceNode) ==
                       childrenBeforeRun + 1,
                   "crop handler should add one storage child"))
            return 1;
        auto child = FindChild(context->DataStorage(), sourceNode, "CTA_cropped");
        if (Expect(child.IsNotNull(),
                   "crop handler should add named result node"))
            return 1;

        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("image-001-crop"));
        if (Expect(entry != nullptr &&
                       entry->SourcePath ==
                           QStringLiteral("xq://generated/image-preprocessing/image-001-crop"),
                   "crop handler should register generated catalog entry"))
            return 1;
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-image-001-crop"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("image-001-crop"),
                   "crop handler should register generated hierarchy entry"))
            return 1;
    }

    return 0;
}
