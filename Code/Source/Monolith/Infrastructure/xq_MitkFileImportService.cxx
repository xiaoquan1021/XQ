#include "xq_MitkFileImportService.h"

#include <QFileInfo>

#include <mitkDataNode.h>
#include <mitkException.h>
#include <mitkIOUtil.h>

#include <exception>

namespace xq::infrastructure
{

namespace
{

MitkFileReaderResult FailedReaderResult(const QString& message)
{
    MitkFileReaderResult result;
    result.Message = message;
    return result;
}

MitkFileImportResult FailedImportResult(const QString& message)
{
    MitkFileImportResult result;
    result.Message = message;
    return result;
}

MitkFileImportResult FailedNodeImportResult(
    const DataNodeImportResult& nodeImport)
{
    MitkFileImportResult result;
    result.NodeImport = nodeImport;
    result.Node = nodeImport.Node;
    result.CatalogEntryId = nodeImport.CatalogEntryId;
    result.Message = nodeImport.Message;
    return result;
}

QString NodeName(const xq::core::DataImportRequest& import)
{
    QString name = import.DisplayName.trimmed();
    if (!name.isEmpty())
        return name;

    name = QFileInfo(import.SourcePath).fileName().trimmed();
    if (!name.isEmpty())
        return name;

    return import.RequestedId.trimmed();
}

} // namespace

MitkFileReaderResult MitkIOFileReader::Load(const QString& sourcePath) const
{
    try
    {
        MitkFileReaderResult result;
        result.Data = mitk::IOUtil::Load(sourcePath.toStdString());
        result.Succeeded = true;
        return result;
    }
    catch (const mitk::Exception& error)
    {
        return FailedReaderResult(QString::fromStdString(
            std::string(error.GetDescription())));
    }
    catch (const std::exception& error)
    {
        return FailedReaderResult(QString::fromLocal8Bit(error.what()));
    }
    catch (...)
    {
        return FailedReaderResult(QStringLiteral(
            "MITK file import failed with an unknown error."));
    }
}

MitkFileImportResult MitkFileImportService::Import(
    const MitkFileImportRequest& request) const
{
    if (request.DataStorage.IsNull())
    {
        return FailedImportResult(QStringLiteral(
            "MITK file import storage is required."));
    }

    if (!request.DataImports)
        return FailedImportResult(QStringLiteral("Data import service is required."));

    if (!request.DataNodes)
        return FailedImportResult(QStringLiteral("Data node registry is required."));

    const QString sourcePath = request.Import.SourcePath.trimmed();
    if (sourcePath.isEmpty())
    {
        return FailedImportResult(QStringLiteral(
            "MITK file import source path is required."));
    }

    MitkIOFileReader defaultReader;
    const MitkFileReader* reader = request.Reader ? request.Reader
                                                  : &defaultReader;
    const auto readerResult = reader->Load(sourcePath);
    if (!readerResult.Succeeded)
        return FailedImportResult(readerResult.Message);

    if (readerResult.Data.empty())
    {
        return FailedImportResult(QStringLiteral(
            "MITK file import produced no data."));
    }

    if (readerResult.Data.size() != 1)
    {
        return FailedImportResult(QStringLiteral(
            "MITK file import expects one data object."));
    }

    auto node = mitk::DataNode::New();
    node->SetData(readerResult.Data.front());
    const QString nodeName = NodeName(request.Import);
    if (!nodeName.isEmpty())
        node->SetName(nodeName.toStdString());

    DataNodeImportRequest nodeRequest;
    nodeRequest.DataStorage = request.DataStorage;
    nodeRequest.DataImports = request.DataImports;
    nodeRequest.DataNodes = request.DataNodes;
    nodeRequest.Import = request.Import;
    nodeRequest.Node = node;

    DataNodeImportService nodeImportService;
    const auto nodeImport = nodeImportService.Import(nodeRequest);
    if (!nodeImport.Succeeded)
        return FailedNodeImportResult(nodeImport);

    MitkFileImportResult result;
    result.Succeeded = true;
    result.CatalogEntryId = nodeImport.CatalogEntryId;
    result.Message = nodeImport.Message;
    result.NodeImport = nodeImport;
    result.Node = nodeImport.Node;
    return result;
}

} // namespace xq::infrastructure
