#include "xq_DataNodeImportService.h"

#include "Core/xq_DataNodeRegistryService.h"

namespace xq::infrastructure
{

namespace
{

DataNodeImportResult FailedResult(const QString& message)
{
    DataNodeImportResult result;
    result.Message = message;
    return result;
}

} // namespace

DataNodeImportResult DataNodeImportService::Import(
    const DataNodeImportRequest& request) const
{
    if (request.DataStorage.IsNull())
    {
        return FailedResult(QStringLiteral(
            "Data node import storage is required."));
    }

    if (!request.DataImports)
        return FailedResult(QStringLiteral("Data import service is required."));

    if (!request.DataNodes)
        return FailedResult(QStringLiteral("Data node registry is required."));

    if (request.Node.IsNull())
        return FailedResult(QStringLiteral("Data node is required."));

    QString importMessage;
    const auto importResult =
        request.DataImports->Import(request.Import, &importMessage);
    if (!importResult.Succeeded)
        return FailedResult(importResult.Message);

    request.DataStorage->Add(request.Node);

    QString registryMessage;
    if (!request.DataNodes->BindNode(importResult.EntryId,
                                     request.Node,
                                     &registryMessage))
    {
        request.DataStorage->Remove(request.Node);
        return FailedResult(registryMessage);
    }

    DataNodeImportResult result;
    result.Succeeded = true;
    result.CatalogEntryId = importResult.EntryId;
    result.Message = importResult.Message;
    result.Node = request.Node;
    return result;
}

} // namespace xq::infrastructure
