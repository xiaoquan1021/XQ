#ifndef XQ_WORKFLOWREGISTRY_H
#define XQ_WORKFLOWREGISTRY_H

#include <QString>

#include <vector>

namespace xq::core
{

enum class WorkflowCategory
{
    Project,
    Data,
    Image,
    Path,
    Segmentation,
    Modeling,
    Simulation,
    Automation
};

struct WorkflowDescriptor
{
    QString Id;
    QString Title;
    WorkflowCategory Category;
};

const std::vector<WorkflowDescriptor>& DefaultWorkflowRegistry();
const WorkflowDescriptor* FindWorkflowById(const QString& id);

} // namespace xq::core

#endif // XQ_WORKFLOWREGISTRY_H
