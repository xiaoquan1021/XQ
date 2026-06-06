#include "Core/xq_WorkflowRegistry.h"

#include <iostream>
#include <set>
#include <vector>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main()
{
    const auto& workflows = xq::core::DefaultWorkflowRegistry();

    const std::vector<QString> expectedIds = {
        QStringLiteral("project"),
        QStringLiteral("data"),
        QStringLiteral("image-preprocessing"),
        QStringLiteral("path"),
        QStringLiteral("segmentation-2d"),
        QStringLiteral("segmentation-3d"),
        QStringLiteral("modeling"),
        QStringLiteral("meshing"),
        QStringLiteral("flow-simulation"),
        QStringLiteral("rom-simulation"),
        QStringLiteral("multiphysics"),
        QStringLiteral("python-api"),
    };

    if (Expect(workflows.size() == expectedIds.size(),
               "default workflow registry has an unexpected size"))
        return 1;

    std::set<QString> seenIds;
    for (std::size_t i = 0; i < expectedIds.size(); ++i)
    {
        const auto& workflow = workflows.at(i);
        if (Expect(workflow.Id == expectedIds.at(i),
                   "default workflow registry order or id is wrong"))
            return 1;
        if (Expect(!workflow.Title.trimmed().isEmpty(),
                   "workflow title must not be empty"))
            return 1;
        if (Expect(seenIds.find(workflow.Id) == seenIds.end(),
                   "workflow ids must be unique"))
            return 1;
        seenIds.insert(workflow.Id);
    }

    const auto* flow =
        xq::core::FindWorkflowById(QStringLiteral("flow-simulation"));
    if (Expect(flow != nullptr, "flow simulation workflow must be discoverable"))
        return 1;
    if (Expect(flow->Title == QStringLiteral("Flow Simulation"),
               "flow simulation workflow has the wrong title"))
        return 1;
    if (Expect(flow->Category == xq::core::WorkflowCategory::Simulation,
               "flow simulation workflow has the wrong category"))
        return 1;

    const auto* segment3d =
        xq::core::FindWorkflowById(QStringLiteral("segmentation-3d"));
    if (Expect(segment3d != nullptr,
               "3D segmentation workflow must be discoverable"))
        return 1;
    if (Expect(segment3d->Category == xq::core::WorkflowCategory::Segmentation,
               "3D segmentation workflow has the wrong category"))
        return 1;

    if (Expect(xq::core::FindWorkflowById(QStringLiteral("missing")) == nullptr,
               "unknown workflow ids must not resolve"))
        return 1;

    return 0;
}
