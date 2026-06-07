#ifndef XQ_INFRASTRUCTURE_DATANODEIMPORTSERVICE_H
#define XQ_INFRASTRUCTURE_DATANODEIMPORTSERVICE_H

#include "Core/xq_DataImportService.h"

#include <QString>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

namespace xq::core
{
class DataNodeRegistryService;
}

namespace xq::infrastructure
{

struct DataNodeImportRequest
{
    mitk::DataStorage::Pointer DataStorage;
    xq::core::DataImportService* DataImports = nullptr;
    xq::core::DataNodeRegistryService* DataNodes = nullptr;
    xq::core::DataImportRequest Import;
    mitk::DataNode::Pointer Node;
};

struct DataNodeImportResult
{
    bool Succeeded = false;
    QString CatalogEntryId;
    QString Message;
    mitk::DataNode::Pointer Node;
};

class DataNodeImportService
{
public:
    DataNodeImportResult Import(const DataNodeImportRequest& request) const;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_DATANODEIMPORTSERVICE_H
