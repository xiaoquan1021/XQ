#include "xq_WorkflowActionHandlers.h"

#include "Core/xq_WorkflowActionService.h"

#include <QString>
#include <QStringList>

namespace xq::domain
{

namespace
{

QString SelectedDataLabel(
    const xq::core::WorkflowContextSnapshot& snapshot)
{
    const QString displayName = snapshot.SelectedDataDisplayName.trimmed();
    if (!displayName.isEmpty())
        return displayName;

    return snapshot.SelectedCatalogEntryId;
}

xq::core::WorkflowActionService::WorkflowActionHandler
CreateDefaultHandler()
{
    return [](const xq::core::WorkflowContextSnapshot& snapshot,
              QString* message) {
        if (message)
        {
            *message = QStringLiteral("%1 domain workflow accepted %2.")
                           .arg(snapshot.WorkflowTitle,
                                SelectedDataLabel(snapshot));
        }
        return true;
    };
}

} // namespace

int RegisterDefaultWorkflowActionHandlers(
    xq::core::WorkflowActionService& actions)
{
    const QStringList dataDependentWorkflowIds = {
        QStringLiteral("image-preprocessing"),
        QStringLiteral("path"),
        QStringLiteral("segmentation-2d"),
        QStringLiteral("segmentation-3d"),
        QStringLiteral("modeling"),
        QStringLiteral("meshing"),
        QStringLiteral("flow-simulation"),
        QStringLiteral("rom-simulation"),
        QStringLiteral("multiphysics"),
    };

    int registered = 0;
    for (const auto& workflowId : dataDependentWorkflowIds)
    {
        if (actions.RegisterHandler(workflowId, CreateDefaultHandler()))
            ++registered;
    }

    return registered;
}

} // namespace xq::domain
