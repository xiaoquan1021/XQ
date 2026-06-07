#ifndef XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGSTORAGECOMMITSERVICE_H
#define XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGSTORAGECOMMITSERVICE_H

#include "Core/xq_WorkflowContextService.h"
#include "xq_ImagePreprocessingExecutionService.h"

#include <QString>
#include <QVariantMap>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

namespace xq::infrastructure
{

struct ImagePreprocessingStorageCommitRequest
{
    mitk::DataStorage::Pointer DataStorage;
    mitk::DataNode::Pointer SourceNode;
    xq::core::WorkflowContextSnapshot Context;
    QString OperationId;
    QVariantMap Parameters;
    QString ResultSuffix;
};

struct ImagePreprocessingStorageCommitResult
{
    bool Succeeded = false;
    QString Message;
    ImagePreprocessingExecutionResult Execution;
    mitk::DataNode::Pointer ResultNode;
};

class ImagePreprocessingStorageCommitService
{
public:
    ImagePreprocessingStorageCommitResult Run(
        const ImagePreprocessingStorageCommitRequest& request) const;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGSTORAGECOMMITSERVICE_H
