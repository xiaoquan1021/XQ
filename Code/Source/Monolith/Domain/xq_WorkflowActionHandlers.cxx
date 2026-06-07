#include "xq_WorkflowActionHandlers.h"

#include "xq_ImagePreprocessingWorkflowService.h"

#include "Core/xq_WorkflowActionService.h"

#include <QString>
#include <QStringList>

#include <memory>

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

xq::core::WorkflowActionService::WorkflowActionHandler
CreateImagePreprocessingHandler()
{
    auto service = std::make_shared<ImagePreprocessingWorkflowService>();
    return [service](const xq::core::WorkflowContextSnapshot& snapshot,
                     QString* message) {
        const auto result = service->Run(snapshot);
        if (message)
            *message = result.Message;
        return result.Succeeded;
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
    if (actions.RegisterHandler(QStringLiteral("image-preprocessing"),
                                CreateImagePreprocessingHandler()))
    {
        ++registered;
    }

    for (const auto& workflowId : dataDependentWorkflowIds)
    {
        if (workflowId == QStringLiteral("image-preprocessing"))
            continue;

        if (actions.RegisterHandler(workflowId, CreateDefaultHandler()))
            ++registered;
    }

    return registered;
}

} // namespace xq::domain
