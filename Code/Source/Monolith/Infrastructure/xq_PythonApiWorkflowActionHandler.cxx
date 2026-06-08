#include "xq_PythonApiWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"

#include <xq_PythonApiService.h>

#include <QStringList>
#include <QVariantMap>

#include <algorithm>

namespace xq::infrastructure
{

namespace
{

constexpr const char* kPythonApiWorkflowId = "python-api";
constexpr const char* kOpenPythonConsoleOperationId = "open-python-console";
constexpr const char* kExportApiSnippetOperationId = "export-api-snippet";

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

QStringList PythonApiSnippetCatalog()
{
    return {
        QStringLiteral("xq.version()"),
        QStringLiteral("xq.list_nodes()"),
        QStringLiteral("xq.find_node(name)"),
        QStringLiteral("xq.resolve_upstream(node_name, stage)"),
        QStringLiteral("xq.read_model(name)"),
        QStringLiteral("xq.project.save(path)"),
    };
}

int RequestedSnippetCount(xq::core::WorkflowOperationService* operations,
                          const QString& workflowId,
                          const QString& operationId)
{
    if (!operations)
        return PythonApiSnippetCatalog().size();

    const QVariantMap parameters =
        operations->ParameterValues(workflowId, operationId);
    const int requested =
        parameters.value(QStringLiteral("snippet-count")).toInt();
    if (requested <= 0)
        return PythonApiSnippetCatalog().size();

    return requested;
}

bool RunExportApiSnippet(xq::core::WorkflowOperationService* operations,
                         const xq::core::WorkflowContextSnapshot& snapshot,
                         QString* message)
{
    const QStringList snippets = PythonApiSnippetCatalog();
    const int count = std::min(
        RequestedSnippetCount(operations,
                              snapshot.WorkflowId,
                              QString::fromLatin1(kExportApiSnippetOperationId)),
        static_cast<int>(snippets.size()));
    QStringList selected;
    for (int i = 0; i < count; ++i)
        selected.append(snippets.at(i));

    SetMessage(
        message,
        QStringLiteral("Python API snippets (%1): %2")
            .arg(count)
            .arg(selected.join(QStringLiteral(" | "))));
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
                if (operationId ==
                    QString::fromLatin1(kExportApiSnippetOperationId))
                {
                    return RunExportApiSnippet(operations,
                                               snapshot,
                                               taskMessage);
                }

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
