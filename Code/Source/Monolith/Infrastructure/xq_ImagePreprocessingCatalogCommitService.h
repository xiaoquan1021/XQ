#ifndef XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGCATALOGCOMMITSERVICE_H
#define XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGCATALOGCOMMITSERVICE_H

#include "xq_ImagePreprocessingStorageCommitService.h"

#include <QString>

namespace xq::core
{
class DataCatalogService;
class DataHierarchyService;
}

namespace xq::infrastructure
{

struct ImagePreprocessingCatalogCommitRequest
{
    xq::core::DataCatalogService* DataCatalog = nullptr;
    xq::core::DataHierarchyService* DataHierarchy = nullptr;
    QString ResultCatalogEntryId;
    ImagePreprocessingStorageCommitResult StorageCommit;
};

struct ImagePreprocessingCatalogCommitResult
{
    bool Succeeded = false;
    QString CatalogEntryId;
    QString Message;
};

class ImagePreprocessingCatalogCommitService
{
public:
    ImagePreprocessingCatalogCommitResult Run(
        const ImagePreprocessingCatalogCommitRequest& request) const;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGCATALOGCOMMITSERVICE_H
