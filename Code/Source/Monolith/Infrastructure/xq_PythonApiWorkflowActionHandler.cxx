#include "xq_PythonApiWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <xq_PythonApiService.h>

namespace xq::infrastructure
{

namespace
{

constexpr const char* kPythonApiWorkflowId = "python-api";
constexpr const char* kOpenPythonConsoleOperationId = "open-python-console";

void SetMessage(QString* message, const QString& value)
{
    if (message)
        *message = value;
}

QString OperationTitle(xq::core::WorkflowOperationService* operations,
                       const QString& workflowId,
                       const QString& operationId)
{
    if (!operations)
        return {};

    for (const auto& operation :
         operations->OperationsForWorkflow(workflowId))
    {
        if (operation.Id == operationId)
            return operation.Title;
    }

    return {};
}

bool RunPlaceholderPythonApiOperation(
    xq::core::WorkflowOperationService* operations,
    const xq::core::WorkflowContextSnapshot& snapshot,
    QString* message)
{
    const QString operationId =
        operations ? operations->SelectedOperationId(snapshot.WorkflowId)
                   : QString();
    const QString operationTitle =
        OperationTitle(operations, snapshot.WorkflowId, operationId);
    if (operationTitle.trimmed().isEmpty())
    {
        SetMessage(message,
                   QStringLiteral("%1 domain workflow accepted %2.")
                       .arg(snapshot.WorkflowTitle,
                            snapshot.SelectedDataDisplayName));
        return true;
    }

    SetMessage(message,
               QStringLiteral("%1 python api operation accepted.")
                   .arg(operationTitle));
    return true;
}

bool RunOpenPythonConsole(xq::core::ApplicationContext& context,
                          QString* message)
{
    xq_PythonApiService service(context.DataStorage().GetPointer());
    const auto version = service.Version();
    const QString versionText =
        version.ok ? QString::fromStdString(version.value)
                   : QStringLiteral("Python API version unavailable.");
    const QString availability =
        QString::fromStdString(service.GetAvailabilityDiagnostic());

    SetMessage(message,
               QStringLiteral("%1 | %2").arg(versionText, availability));
    return true;
}

} // namespace

bool RegisterDynamicPythonApiWorkflowActionHandler(
    xq::core::ApplicationContext& context,
    QString* message)
{
    const auto handler =
        [&context](const xq::core::WorkflowContextSnapshot& snapshot,
                   QString* taskMessage) {
            auto* operations = context.WorkflowOperations();
            const QString operationId =
                operations ? operations->SelectedOperationId(snapshot.WorkflowId)
                           : QString();
            if (operationId.trimmed().isEmpty())
            {
                SetMessage(taskMessage,
                           QStringLiteral(
                               "Python API operation id is required."));
                return false;
            }

            if (operationId !=
                QString::fromLatin1(kOpenPythonConsoleOperationId))
            {
                return RunPlaceholderPythonApiOperation(operations,
                                                        snapshot,
                                                        taskMessage);
            }

            return RunOpenPythonConsole(context, taskMessage);
        };

    return context.WorkflowActions()->RegisterHandler(
        QString::fromLatin1(kPythonApiWorkflowId),
        handler,
        message);
}

} // namespace xq::infrastructure
