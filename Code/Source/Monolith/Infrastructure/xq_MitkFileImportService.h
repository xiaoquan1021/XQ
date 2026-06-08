#ifndef XQ_INFRASTRUCTURE_MITKFILEIMPORTSERVICE_H
#define XQ_INFRASTRUCTURE_MITKFILEIMPORTSERVICE_H

#include "Core/xq_DataImportService.h"
#include "xq_DataNodeImportService.h"

#include <QString>

#include <mitkBaseData.h>
#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <vector>

namespace xq::core
{
class DataNodeRegistryService;
}

namespace xq::infrastructure
{

struct MitkFileReaderResult
{
    bool Succeeded = false;
    QString Message;
    std::vector<mitk::BaseData::Pointer> Data;
};

class MitkFileReader
{
public:
    virtual ~MitkFileReader() = default;

    virtual MitkFileReaderResult Load(const QString& sourcePath) const = 0;
};

class MitkIOFileReader : public MitkFileReader
{
public:
    MitkFileReaderResult Load(const QString& sourcePath) const override;
};

struct MitkFileImportRequest
{
    mitk::DataStorage::Pointer DataStorage;
    xq::core::DataImportService* DataImports = nullptr;
    xq::core::DataNodeRegistryService* DataNodes = nullptr;
    xq::core::DataImportRequest Import;
    const MitkFileReader* Reader = nullptr;
};

struct MitkFileImportResult
{
    bool Succeeded = false;
    QString CatalogEntryId;
    QString Message;
    DataNodeImportResult NodeImport;
    mitk::DataNode::Pointer Node;
};

class MitkFileImportService
{
public:
    MitkFileImportResult Import(const MitkFileImportRequest& request) const;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_MITKFILEIMPORTSERVICE_H
