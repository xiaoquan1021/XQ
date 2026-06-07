#ifndef XQ_DOMAIN_IMAGEPREPROCESSINGWORKFLOWSERVICE_H
#define XQ_DOMAIN_IMAGEPREPROCESSINGWORKFLOWSERVICE_H

#include "Core/xq_WorkflowContextService.h"

#include <QString>

namespace xq::domain
{

struct ImagePreprocessingWorkflowResult
{
    bool Succeeded = false;
    QString SourceCatalogEntryId;
    QString SelectedDataDisplayName;
    QString Message;
};

class ImagePreprocessingWorkflowService
{
public:
    ImagePreprocessingWorkflowResult Run(
        const xq::core::WorkflowContextSnapshot& snapshot) const;
};

} // namespace xq::domain

#endif // XQ_DOMAIN_IMAGEPREPROCESSINGWORKFLOWSERVICE_H
