#include "Infrastructure/xq_ImagePreprocessingStorageCommitService.h"

#include <QCoreApplication>
#include <QVariantMap>

#include <mitkDataNode.h>
#include <mitkImage.h>
#include <mitkStandaloneDataStorage.h>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <iostream>

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

xq::core::WorkflowContextSnapshot MakeSnapshot()
{
    xq::core::WorkflowContextSnapshot snapshot;
    snapshot.WorkflowId = QStringLiteral("image-preprocessing");
    snapshot.WorkflowTitle = QStringLiteral("Image Preprocessing");
    snapshot.RequiresSelectedData = true;
    snapshot.HasSelectedData = true;
    snapshot.HasCompatibleSelection = true;
    snapshot.SelectedCatalogEntryId = QStringLiteral("image-001");
    snapshot.SelectedDataDisplayName = QStringLiteral("CTA Image");
    snapshot.SelectedDataRole = xq::core::DataWorkflowRole::Image;
    return snapshot;
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

mitk::DataNode::Pointer FindChild(
    mitk::DataStorage::Pointer storage,
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

    xq::infrastructure::ImagePreprocessingStorageCommitService service;

    xq::infrastructure::ImagePreprocessingStorageCommitRequest request;
    request.Context = MakeSnapshot();
    request.OperationId = QStringLiteral("crop");
    request.Parameters = CropParameters();
    request.ResultSuffix = QStringLiteral("cropped");
    request.SourceNode = MakeSourceNode();

    const auto missingStorageResult = service.Run(request);
    if (Expect(!missingStorageResult.Succeeded,
               "storage commit should reject missing storage"))
        return 1;
    if (Expect(missingStorageResult.Message ==
                   QStringLiteral("Image preprocessing data storage is required."),
               "missing storage should use service diagnostic"))
        return 1;

    auto storage = mitk::StandaloneDataStorage::New();
    request.DataStorage = storage;
    request.SourceNode = mitk::DataNode::New();

    const auto unusableSourceResult = service.Run(request);
    if (Expect(!unusableSourceResult.Succeeded,
               "storage commit should reject unusable source nodes"))
        return 1;
    if (Expect(unusableSourceResult.Message ==
                   QStringLiteral("Selected node does not contain a usable image."),
               "unusable source should use MITK image adapter diagnostic"))
        return 1;

    auto sourceNode = MakeSourceNode();
    storage->Add(sourceNode);
    request.SourceNode = sourceNode;

    const auto commitResult = service.Run(request);
    if (Expect(commitResult.Succeeded,
               "storage commit should run valid crop requests"))
        return 1;
    if (Expect(commitResult.ResultNode.IsNotNull(),
               "storage commit should return result node"))
        return 1;
    if (Expect(QString::fromStdString(commitResult.ResultNode->GetName()) ==
                   QStringLiteral("CTA_cropped"),
               "storage commit result node should be named by source and suffix"))
        return 1;
    if (Expect(dynamic_cast<mitk::Image*>(
                   commitResult.ResultNode->GetData()) != nullptr,
               "storage commit result node should contain image data"))
        return 1;
    if (Expect(commitResult.Message ==
                   QStringLiteral("Crop preprocessing operation completed CTA Image."),
               "storage commit should preserve execution message"))
        return 1;

    auto child = FindChild(storage, sourceNode, "CTA_cropped");
    if (Expect(child.IsNotNull(),
               "storage commit should add result node under source node"))
        return 1;
    if (Expect(child.GetPointer() == commitResult.ResultNode.GetPointer(),
               "storage child should be the returned result node"))
        return 1;

    const int* dimensions =
        dynamic_cast<mitk::Image*>(child->GetData())
            ->GetVtkImageData()
            ->GetDimensions();
    if (Expect(dimensions[0] == 3 &&
                   dimensions[1] == 2 &&
                   dimensions[2] == 4,
               "storage commit should preserve crop output dimensions"))
        return 1;

    return 0;
}
