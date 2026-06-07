#ifndef XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGRESULTNODEFACTORY_H
#define XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGRESULTNODEFACTORY_H

#include "xq_ImagePreprocessingExecutionService.h"

#include <QString>

#include <mitkDataNode.h>

namespace xq::infrastructure
{

struct ImagePreprocessingResultNodeCreationResult
{
    bool Succeeded = false;
    QString Message;
    mitk::DataNode::Pointer Node;
};

class ImagePreprocessingResultNodeFactory
{
public:
    ImagePreprocessingResultNodeCreationResult CreateImageResultNode(
        mitk::DataNode* sourceNode,
        const ImagePreprocessingExecutionResult& executionResult,
        const QString& suffix) const;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_IMAGEPREPROCESSINGRESULTNODEFACTORY_H
