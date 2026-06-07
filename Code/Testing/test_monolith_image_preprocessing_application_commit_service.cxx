#include "Infrastructure/xq_ImagePreprocessingApplicationCommitService.h"

#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"

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

xq::infrastructure::ImagePreprocessingApplicationCommitRequest
MakeRequest(mitk::DataStorage::Pointer storage,
            xq::core::DataCatalogService& catalog,
            xq::core::DataHierarchyService& hierarchy,
            mitk::DataNode::Pointer sourceNode,
            const QString& resultCatalogEntryId)
{
    xq::infrastructure::ImagePreprocessingApplicationCommitRequest request;
    request.DataStorage = storage;
    request.DataCatalog = &catalog;
    request.DataHierarchy = &hierarchy;
    request.SourceNode = sourceNode;
    request.Context = MakeSnapshot();
    request.OperationId = QStringLiteral("crop");
    request.Parameters = CropParameters();
    request.ResultCatalogEntryId = resultCatalogEntryId;
    request.ResultSuffix = QStringLiteral("cropped");
    return request;
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

bool RegisterDuplicateCatalogEntry(xq::core::DataCatalogService& catalog)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = QStringLiteral("image-001-crop");
    entry.DisplayName = QStringLiteral("Existing Crop");
    entry.SourcePath = QStringLiteral("C:/studies/existing-crop.nii");
    entry.Modality = QStringLiteral("CT");
    entry.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return catalog.RegisterEntry(entry);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::infrastructure::ImagePreprocessingApplicationCommitService service;

    {
        auto storage = mitk::StandaloneDataStorage::New();
        auto sourceNode = MakeSourceNode();
        storage->Add(sourceNode);
        xq::core::DataCatalogService catalog;
        xq::core::DataHierarchyService hierarchy;
        if (Expect(RegisterDuplicateCatalogEntry(catalog),
                   "duplicate fixture catalog entry should register"))
            return 1;

        auto request = MakeRequest(storage,
                                   catalog,
                                   hierarchy,
                                   sourceNode,
                                   QStringLiteral("image-001-crop"));
        const int childrenBeforeRun = ChildCount(storage, sourceNode);
        const auto duplicateResult = service.Run(request);
        if (Expect(!duplicateResult.Succeeded,
                   "application commit should reject duplicate catalog targets"))
            return 1;
        if (Expect(duplicateResult.Message ==
                       QStringLiteral("Duplicate data id."),
                   "duplicate catalog target should use catalog diagnostic"))
            return 1;
        if (Expect(ChildCount(storage, sourceNode) == childrenBeforeRun,
                   "duplicate catalog target should not mutate DataStorage"))
            return 1;
        if (Expect(hierarchy.FindNode(
                       QStringLiteral("data-image-001-crop")) == nullptr,
                   "duplicate catalog target should not mutate hierarchy"))
            return 1;
    }

    {
        auto storage = mitk::StandaloneDataStorage::New();
        auto sourceNode = MakeSourceNode();
        storage->Add(sourceNode);
        xq::core::DataCatalogService catalog;
        xq::core::DataHierarchyService hierarchy;
        QString errorMessage;
        hierarchy.AddFolder(QStringLiteral("images"),
                            hierarchy.RootId(),
                            QStringLiteral("Images"),
                            &errorMessage);
        hierarchy.AddDataEntry(QStringLiteral("data-image-001-crop"),
                               QStringLiteral("images"),
                               QStringLiteral("other-image"),
                               QStringLiteral("Existing Crop"),
                               &errorMessage);

        auto request = MakeRequest(storage,
                                   catalog,
                                   hierarchy,
                                   sourceNode,
                                   QStringLiteral("image-001-crop"));
        const int childrenBeforeRun = ChildCount(storage, sourceNode);
        const auto duplicateHierarchyResult = service.Run(request);
        if (Expect(!duplicateHierarchyResult.Succeeded,
                   "application commit should reject duplicate hierarchy targets"))
            return 1;
        if (Expect(duplicateHierarchyResult.Message ==
                       QStringLiteral("Duplicate hierarchy node id."),
                   "duplicate hierarchy target should use hierarchy diagnostic"))
            return 1;
        if (Expect(ChildCount(storage, sourceNode) == childrenBeforeRun,
                   "duplicate hierarchy target should not mutate DataStorage"))
            return 1;
        if (Expect(catalog.Entries().isEmpty(),
                   "duplicate hierarchy target should not mutate catalog"))
            return 1;
    }

    {
        xq::core::DataCatalogService catalog;
        xq::core::DataHierarchyService hierarchy;
        auto sourceNode = MakeSourceNode();
        auto request = MakeRequest(nullptr,
                                   catalog,
                                   hierarchy,
                                   sourceNode,
                                   QStringLiteral("image-002-crop"));

        const auto missingStorageResult = service.Run(request);
        if (Expect(!missingStorageResult.Succeeded,
                   "application commit should reject storage failures"))
            return 1;
        if (Expect(missingStorageResult.Message ==
                       QStringLiteral("Image preprocessing data storage is required."),
                   "storage failure should preserve storage diagnostic"))
            return 1;
        if (Expect(catalog.Entries().isEmpty(),
                   "storage failure should not register catalog entries"))
            return 1;
        if (Expect(hierarchy.FindNode(
                       QStringLiteral("data-image-002-crop")) == nullptr,
                   "storage failure should not register hierarchy entries"))
            return 1;
    }

    {
        auto storage = mitk::StandaloneDataStorage::New();
        auto sourceNode = MakeSourceNode();
        storage->Add(sourceNode);
        xq::core::DataCatalogService catalog;
        xq::core::DataHierarchyService hierarchy;
        auto request = MakeRequest(storage,
                                   catalog,
                                   hierarchy,
                                   sourceNode,
                                   QStringLiteral("image-001-crop"));

        const auto commitResult = service.Run(request);
        if (Expect(commitResult.Succeeded,
                   "application commit should run valid crop requests"))
            return 1;
        if (Expect(commitResult.StorageCommit.Succeeded,
                   "application commit should expose storage success"))
            return 1;
        if (Expect(commitResult.CatalogCommit.Succeeded,
                   "application commit should expose catalog success"))
            return 1;
        if (Expect(commitResult.ResultNode.IsNotNull(),
                   "application commit should expose result node"))
            return 1;
        if (Expect(commitResult.CatalogEntryId ==
                       QStringLiteral("image-001-crop"),
                   "application commit should expose catalog entry id"))
            return 1;

        auto child = FindChild(storage, sourceNode, "CTA_cropped");
        if (Expect(child.IsNotNull(),
                   "application commit should add result node under source"))
            return 1;
        if (Expect(child.GetPointer() == commitResult.ResultNode.GetPointer(),
                   "application commit child should match returned result node"))
            return 1;

        const auto* entry = catalog.FindById(QStringLiteral("image-001-crop"));
        if (Expect(entry != nullptr,
                   "application commit should register catalog entry"))
            return 1;
        if (Expect(entry->DisplayName == QStringLiteral("CTA_cropped"),
                   "application commit catalog entry should use result name"))
            return 1;
        if (Expect(entry->SourcePath ==
                       QStringLiteral("xq://generated/image-preprocessing/image-001-crop"),
                   "application commit catalog entry should use virtual source"))
            return 1;

        const auto* node =
            hierarchy.FindNode(QStringLiteral("data-image-001-crop"));
        if (Expect(node != nullptr &&
                       node->DataCatalogEntryId ==
                           QStringLiteral("image-001-crop"),
                   "application commit should register hierarchy entry"))
            return 1;
    }

    return 0;
}
