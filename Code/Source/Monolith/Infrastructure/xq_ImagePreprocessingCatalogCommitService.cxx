#include "xq_ImagePreprocessingCatalogCommitService.h"

#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"

namespace xq::infrastructure
{

namespace
{

constexpr const char* kImagesFolderId = "images";
constexpr const char* kImagesFolderTitle = "Images";

ImagePreprocessingCatalogCommitResult FailedResult(const QString& message)
{
    ImagePreprocessingCatalogCommitResult result;
    result.Message = message;
    return result;
}

QString HierarchyNodeId(const QString& catalogEntryId)
{
    return QStringLiteral("data-%1").arg(catalogEntryId.trimmed());
}

QString VirtualSourcePath(const QString& catalogEntryId)
{
    return QStringLiteral("xq://generated/image-preprocessing/%1")
        .arg(catalogEntryId.trimmed());
}

QString ResultDisplayName(
    const ImagePreprocessingStorageCommitResult& storageCommit)
{
    if (storageCommit.ResultNode.IsNotNull())
    {
        const QString nodeName =
            QString::fromStdString(storageCommit.ResultNode->GetName()).trimmed();
        if (!nodeName.isEmpty())
            return nodeName;
    }

    return storageCommit.Execution.SelectedDataDisplayName.trimmed().isEmpty()
        ? QStringLiteral("Image preprocessing result")
        : storageCommit.Execution.SelectedDataDisplayName;
}

} // namespace

ImagePreprocessingCatalogCommitResult
ImagePreprocessingCatalogCommitService::Run(
    const ImagePreprocessingCatalogCommitRequest& request) const
{
    if (!request.DataCatalog)
    {
        return FailedResult(QStringLiteral(
            "Image preprocessing data catalog is required."));
    }

    if (!request.DataHierarchy)
    {
        return FailedResult(QStringLiteral(
            "Image preprocessing data hierarchy is required."));
    }

    if (!request.StorageCommit.Succeeded)
        return FailedResult(request.StorageCommit.Message);

    const QString entryId = request.ResultCatalogEntryId.trimmed();
    if (entryId.isEmpty())
    {
        return FailedResult(QStringLiteral(
            "Image preprocessing result catalog entry id is required."));
    }

    const QString displayName = ResultDisplayName(request.StorageCommit);

    xq::core::DataCatalogEntry entry;
    entry.Id = entryId;
    entry.DisplayName = displayName;
    entry.SourcePath = VirtualSourcePath(entryId);
    entry.Modality = QStringLiteral("Generated");
    entry.WorkflowRole = xq::core::DataWorkflowRole::Image;

    QString errorMessage;
    if (!request.DataCatalog->RegisterEntry(entry, &errorMessage))
        return FailedResult(errorMessage);

    if (!request.DataHierarchy->FindNode(QString::fromLatin1(kImagesFolderId)))
    {
        if (!request.DataHierarchy->AddFolder(
                QString::fromLatin1(kImagesFolderId),
                request.DataHierarchy->RootId(),
                QString::fromLatin1(kImagesFolderTitle),
                &errorMessage))
        {
            return FailedResult(errorMessage);
        }
    }

    if (!request.DataHierarchy->AddDataEntry(
            HierarchyNodeId(entryId),
            QString::fromLatin1(kImagesFolderId),
            entryId,
            displayName,
            &errorMessage))
    {
        return FailedResult(errorMessage);
    }

    ImagePreprocessingCatalogCommitResult result;
    result.Succeeded = true;
    result.CatalogEntryId = entryId;
    result.Message =
        QStringLiteral("Registered preprocessing result catalog entry.");
    return result;
}

} // namespace xq::infrastructure
