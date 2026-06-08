#include "Infrastructure/xq_PythonApiWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_TaskRunner.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"

#include <QCoreApplication>

#include <iostream>
#include <memory>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

bool PreparePythonApiWorkflow(xq::core::ApplicationContext& context,
                              const QString& operationId)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("python-api")))
    {
        return false;
    }
    return context.WorkflowOperations()->SelectOperation(
        QStringLiteral("python-api"),
        operationId,
        &message);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicPythonApiWorkflowActionHandler(*context,
                                                                  &message),
                "dynamic Python API handler should register"))
        {
            return 1;
        }
        if (Expect(context->WorkflowActions()->HasHandler(
                       QStringLiteral("python-api")),
                   "dynamic Python API handler should be discoverable"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PreparePythonApiWorkflow(
                       *context,
                       QStringLiteral("open-python-console")),
                   "Python API fixture should prepare console operation"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicPythonApiWorkflowActionHandler(*context,
                                                                  &message),
                "Python API fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "Python API handler should return availability diagnostic"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }

        if (Expect(message.contains(QStringLiteral(
                       "XQ 1.0.0 (Python API skeleton, pybind11 not linked)")) &&
                       message.contains(QStringLiteral(
                           "Python API unavailable in this build")) &&
                       message.contains(QStringLiteral(
                           "C++ inspection service remains available")),
                   "Python API diagnostic should come from xq_PythonApiService"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(context->Tasks()->History().back().Succeeded,
                   "Python API diagnostic task should succeed"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PreparePythonApiWorkflow(
                       *context,
                       QStringLiteral("export-api-snippet")),
                   "Python API fixture should prepare snippet operation"))
        {
            return 1;
        }

        QString message;
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("python-api"),
                       QStringLiteral("export-api-snippet"),
                       QStringLiteral("snippet-count"),
                       3,
                       &message),
                   "Python API snippet count should be configurable"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicPythonApiWorkflowActionHandler(*context,
                                                                  &message),
                "Python API snippet fixture should install handler"))
        {
            return 1;
        }
        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "Python API snippet export should succeed"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message.contains(QStringLiteral(
                       "Python API snippets (3)")) &&
                       message.contains(QStringLiteral("xq.version()")) &&
                       message.contains(QStringLiteral("xq.list_nodes()")) &&
                       message.contains(QStringLiteral("xq.find_node(name)")),
                   "snippet export should report deterministic API snippets"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(!message.contains(QStringLiteral("xq.resolve_upstream")),
                   "snippet-count should limit generated snippets"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PreparePythonApiWorkflow(
                       *context,
                       QStringLiteral("run-project-script")),
                   "Python API fixture should prepare script operation"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicPythonApiWorkflowActionHandler(*context,
                                                                  &message),
                "Python API placeholder fixture should install handler"))
        {
            return 1;
        }
        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "unsupported Python API operation should preserve placeholder"))
        {
            return 1;
        }
        if (Expect(message == QStringLiteral(
                                  "Project Script Runner python api operation accepted."),
                   "unsupported Python API operation should report placeholder"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    return 0;
}
