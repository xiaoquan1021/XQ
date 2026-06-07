#ifndef XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGAPPLICATIONCOMMITSERVICE_H
#define XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGAPPLICATIONCOMMITSERVICE_H

#include "xq_ImagePreprocessingCatalogCommitService.h"
#include "xq_ImagePreprocessingStorageCommitService.h"

namespace xq::infrastructure
{

struct ImagePreprocessingApplicationCommitRequest
{
    mitk::DataStorage::Pointer DataStorage;
    xq::core::DataCatalogService* DataCatalog = nullptr;
    xq::core::DataHierarchyService* DataHierarchy = nullptr;
    mitk::DataNode::Pointer SourceNode;
    xq::core::WorkflowContextSnapshot Context;
    QString OperationId;
    QVariantMap Parameters;
    QString ResultCatalogEntryId;
    QString ResultSuffix;
};

struct ImagePreprocessingApplicationCommitResult
{
    bool Succeeded = false;
    QString CatalogEntryId;
    QString Message;
    ImagePreprocessingStorageCommitResult StorageCommit;
    ImagePreprocessingCatalogCommitResult CatalogCommit;
    mitk::DataNode::Pointer ResultNode;
};

class ImagePreprocessingApplicationCommitService
{
public:
    ImagePreprocessingApplicationCommitResult Run(
        const ImagePreprocessingApplicationCommitRequest& request) const;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGAPPLICATIONCOMMITSERVICE_H
