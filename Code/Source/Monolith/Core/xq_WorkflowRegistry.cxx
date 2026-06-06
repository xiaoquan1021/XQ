#include "xq_WorkflowRegistry.h"

#include <algorithm>

namespace xq::core
{

const std::vector<WorkflowDescriptor>& DefaultWorkflowRegistry()
{
    static const std::vector<WorkflowDescriptor> workflows = {
        {QStringLiteral("project"), QStringLiteral("Project"),
         WorkflowCategory::Project},
        {QStringLiteral("data"), QStringLiteral("Data"),
         WorkflowCategory::Data},
        {QStringLiteral("image-preprocessing"), QStringLiteral("Image Preprocessing"),
         WorkflowCategory::Image},
        {QStringLiteral("path"), QStringLiteral("Path"),
         WorkflowCategory::Path},
        {QStringLiteral("segmentation-2d"), QStringLiteral("2D Segmentation"),
         WorkflowCategory::Segmentation},
        {QStringLiteral("segmentation-3d"), QStringLiteral("3D Segmentation"),
         WorkflowCategory::Segmentation},
        {QStringLiteral("modeling"), QStringLiteral("Modeling"),
         WorkflowCategory::Modeling},
        {QStringLiteral("meshing"), QStringLiteral("Meshing"),
         WorkflowCategory::Modeling},
        {QStringLiteral("flow-simulation"), QStringLiteral("Flow Simulation"),
         WorkflowCategory::Simulation},
        {QStringLiteral("rom-simulation"), QStringLiteral("ROM Simulation"),
         WorkflowCategory::Simulation},
        {QStringLiteral("multiphysics"), QStringLiteral("Multi-Physics"),
         WorkflowCategory::Simulation},
        {QStringLiteral("python-api"), QStringLiteral("Python API"),
         WorkflowCategory::Automation},
    };

    return workflows;
}

const WorkflowDescriptor* FindWorkflowById(const QString& id)
{
    const auto& workflows = DefaultWorkflowRegistry();
    const auto it = std::find_if(
        workflows.begin(), workflows.end(),
        [&id](const WorkflowDescriptor& workflow) {
            return workflow.Id == id;
        });

    if (it == workflows.end())
        return nullptr;

    return &(*it);
}

} // namespace xq::core
