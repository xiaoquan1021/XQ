#include "xq_ImagePreprocessingApplicationCommitService.h"

#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"

namespace xq::infrastructure
{

namespace
{

constexpr const char* kImagesFolderId = "images";

ImagePreprocessingApplicationCommitResult FailedResult(const QString& message)
{
    ImagePreprocessingApplicationCommitResult result;
    result.Message = message;
    return result;
}

ImagePreprocessingApplicationCommitResult FailedStorageResult(
    const ImagePreprocessingStorageCommitResult& storageCommit)
{
    ImagePreprocessingApplicationCommitResult result;
    result.StorageCommit = storageCommit;
    result.ResultNode = storageCommit.ResultNode;
    result.Message = storageCommit.Message;
    return result;
}

ImagePreprocessingApplicationCommitResult FailedCatalogResult(
    const ImagePreprocessingStorageCommitResult& storageCommit,
    const ImagePreprocessingCatalogCommitResult& catalogCommit)
{
    ImagePreprocessingApplicationCommitResult result;
    result.StorageCommit = storageCommit;
    result.CatalogCommit = catalogCommit;
    result.ResultNode = storageCommit.ResultNode;
    result.CatalogEntryId = catalogCommit.CatalogEntryId;
    result.Message = catalogCommit.Message;
    return result;
}

QString HierarchyNodeId(const QString& catalogEntryId)
{
    return QStringLiteral("data-%1").arg(catalogEntryId.trimmed());
}

QString PreflightCommitTarget(
    const ImagePreprocessingApplicationCommitRequest& request)
{
    if (!request.DataCatalog)
    {
        return QStringLiteral(
            "Image preprocessing data catalog is required.");
    }

    if (!request.DataHierarchy)
    {
        return QStringLiteral(
            "Image preprocessing data hierarchy is required.");
    }

    const QString entryId = request.ResultCatalogEntryId.trimmed();
    if (entryId.isEmpty())
    {
        return QStringLiteral(
            "Image preprocessing result catalog entry id is required.");
    }

    if (request.DataCatalog->FindById(entryId))
        return QStringLiteral("Duplicate data id.");

    const auto* imagesFolder =
        request.DataHierarchy->FindNode(QString::fromLatin1(kImagesFolderId));
    if (imagesFolder &&
        imagesFolder->Kind != xq::core::DataHierarchyNodeKind::Folder)
    {
        return QStringLiteral("Duplicate hierarchy node id.");
    }

    if (request.DataHierarchy->FindNode(HierarchyNodeId(entryId)))
        return QStringLiteral("Duplicate hierarchy node id.");

    return {};
}

} // namespace

ImagePreprocessingApplicationCommitResult
ImagePreprocessingApplicationCommitService::Run(
    const ImagePreprocessingApplicationCommitRequest& request) const
{
    const QString preflightMessage = PreflightCommitTarget(request);
    if (!preflightMessage.isEmpty())
        return FailedResult(preflightMessage);

    ImagePreprocessingStorageCommitRequest storageRequest;
    storageRequest.DataStorage = request.DataStorage;
    storageRequest.SourceNode = request.SourceNode;
    storageRequest.Context = request.Context;
    storageRequest.OperationId = request.OperationId;
    storageRequest.Parameters = request.Parameters;
    storageRequest.ResultSuffix = request.ResultSuffix;

    ImagePreprocessingStorageCommitService storageService;
    const auto storageCommit = storageService.Run(storageRequest);
    if (!storageCommit.Succeeded)
        return FailedStorageResult(storageCommit);

    ImagePreprocessingCatalogCommitRequest catalogRequest;
    catalogRequest.DataCatalog = request.DataCatalog;
    catalogRequest.DataHierarchy = request.DataHierarchy;
    catalogRequest.ResultCatalogEntryId = request.ResultCatalogEntryId;
    catalogRequest.StorageCommit = storageCommit;

    ImagePreprocessingCatalogCommitService catalogService;
    const auto catalogCommit = catalogService.Run(catalogRequest);
    if (!catalogCommit.Succeeded)
        return FailedCatalogResult(storageCommit, catalogCommit);

    ImagePreprocessingApplicationCommitResult result;
    result.Succeeded = true;
    result.CatalogEntryId = catalogCommit.CatalogEntryId;
    result.Message = catalogCommit.Message;
    result.StorageCommit = storageCommit;
    result.CatalogCommit = catalogCommit;
    result.ResultNode = storageCommit.ResultNode;
    return result;
}

} // namespace xq::infrastructure
