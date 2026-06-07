#include "Infrastructure/xq_ImagePreprocessingCatalogCommitService.h"

#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"

#include <QCoreApplication>

#include <mitkDataNode.h>

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

xq::infrastructure::ImagePreprocessingStorageCommitResult
MakeStorageCommitResult()
{
    xq::infrastructure::ImagePreprocessingStorageCommitResult result;
    result.Succeeded = true;
    result.Message =
        QStringLiteral("Crop preprocessing operation completed CTA Image.");
    result.Execution.Succeeded = true;
    result.Execution.SourceCatalogEntryId = QStringLiteral("image-001");
    result.Execution.SelectedDataDisplayName = QStringLiteral("CTA Image");
    result.Execution.OperationId = QStringLiteral("crop");
    result.Execution.OperationTitle = QStringLiteral("Crop");
    result.ResultNode = mitk::DataNode::New();
    result.ResultNode->SetName("CTA_cropped");
    return result;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::infrastructure::ImagePreprocessingCatalogCommitService service;

    xq::core::DataCatalogService catalog;
    xq::core::DataHierarchyService hierarchy;

    xq::infrastructure::ImagePreprocessingCatalogCommitRequest request;
    request.ResultCatalogEntryId = QStringLiteral("image-001-crop");
    request.StorageCommit = MakeStorageCommitResult();
    request.DataHierarchy = &hierarchy;

    const auto missingCatalogResult = service.Run(request);
    if (Expect(!missingCatalogResult.Succeeded,
               "catalog commit should reject missing catalog service"))
        return 1;
    if (Expect(missingCatalogResult.Message ==
                   QStringLiteral("Image preprocessing data catalog is required."),
               "missing catalog should use service diagnostic"))
        return 1;

    request.DataCatalog = &catalog;
    request.DataHierarchy = nullptr;
    const auto missingHierarchyResult = service.Run(request);
    if (Expect(!missingHierarchyResult.Succeeded,
               "catalog commit should reject missing hierarchy service"))
        return 1;
    if (Expect(missingHierarchyResult.Message ==
                   QStringLiteral("Image preprocessing data hierarchy is required."),
               "missing hierarchy should use service diagnostic"))
        return 1;

    request.DataHierarchy = &hierarchy;
    request.StorageCommit.Succeeded = false;
    request.StorageCommit.Message = QStringLiteral("Storage commit failed.");
    const auto failedStorageResult = service.Run(request);
    if (Expect(!failedStorageResult.Succeeded,
               "catalog commit should reject failed storage commits"))
        return 1;
    if (Expect(failedStorageResult.Message ==
                   QStringLiteral("Storage commit failed."),
               "failed storage commit should preserve failure diagnostic"))
        return 1;

    request.StorageCommit = MakeStorageCommitResult();
    const auto commitResult = service.Run(request);
    if (Expect(commitResult.Succeeded,
               "catalog commit should register successful storage commits"))
        return 1;
    if (Expect(commitResult.CatalogEntryId ==
                   QStringLiteral("image-001-crop"),
               "catalog commit should return catalog entry id"))
        return 1;
    if (Expect(commitResult.Message ==
                   QStringLiteral("Registered preprocessing result catalog entry."),
               "catalog commit should report registration success"))
        return 1;

    const auto* entry = catalog.FindById(QStringLiteral("image-001-crop"));
    if (Expect(entry != nullptr,
               "catalog commit should create catalog entry"))
        return 1;
    if (Expect(entry->DisplayName == QStringLiteral("CTA_cropped"),
               "catalog entry should use result node display name"))
        return 1;
    if (Expect(entry->SourcePath ==
                   QStringLiteral("xq://generated/image-preprocessing/image-001-crop"),
               "catalog entry should use generated virtual source path"))
        return 1;
    if (Expect(entry->WorkflowRole == xq::core::DataWorkflowRole::Image,
               "catalog entry should be an image workflow role"))
        return 1;

    const auto* folder = hierarchy.FindNode(QStringLiteral("images"));
    if (Expect(folder != nullptr &&
                   folder->Kind == xq::core::DataHierarchyNodeKind::Folder,
               "catalog commit should ensure Images hierarchy folder"))
        return 1;
    const auto* node =
        hierarchy.FindNode(QStringLiteral("data-image-001-crop"));
    if (Expect(node != nullptr &&
                   node->Kind == xq::core::DataHierarchyNodeKind::DataEntry,
               "catalog commit should add hierarchy data node"))
        return 1;
    if (Expect(node->ParentId == QStringLiteral("images") &&
                   node->DataCatalogEntryId == QStringLiteral("image-001-crop") &&
                   node->DisplayName == QStringLiteral("CTA_cropped"),
               "hierarchy data node should point at the generated catalog entry"))
        return 1;

    return 0;
}
