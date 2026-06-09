#include "Core/xq_ApplicationContext.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Infrastructure/xq_PythonApiWorkflowActionHandler.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include <iostream>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

QComboBox* FindSelector(xq::presentation::MainWindow& window)
{
    return window.findChild<QComboBox*>(
        QStringLiteral("xqWorkflowOperationSelector_python-api"));
}

QPushButton* FindActionButton(xq::presentation::MainWindow& window,
                              const QString& workflowId)
{
    return window.findChild<QPushButton*>(
        QStringLiteral("xqWorkflowPrimaryAction_%1").arg(workflowId));
}

QSpinBox* FindIntegerParameter(xq::presentation::MainWindow& window,
                               const QString& parameterId)
{
    return window.findChild<QSpinBox*>(
        QStringLiteral("xqWorkflowParameter_%1").arg(parameterId));
}

QLabel* FindStatusLabel(xq::presentation::MainWindow& window,
                        const QString& workflowId)
{
    return window.findChild<QLabel*>(
        QStringLiteral("xqWorkflowContextStatus_%1").arg(workflowId));
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context->WorkflowActions(),
        context->WorkflowOperations());
    xq::infrastructure::RegisterDynamicPythonApiWorkflowActionHandler(
        *context);
    xq::presentation::MainWindow window(*context);

    if (Expect(FindActionButton(window, QStringLiteral("project")) == nullptr,
               "project page should not expose a primary action button"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindActionButton(window, QStringLiteral("data")) == nullptr,
               "data page should not expose a primary action button"))
    {
        delete context;
        return 1;
    }

    auto* selector = FindSelector(window);
    if (Expect(selector != nullptr,
               "Python API page should expose an operation selector"))
    {
        delete context;
        return 1;
    }
    if (Expect(selector->count() == 3,
               "Python API selector should expose domain operations"))
    {
        delete context;
        return 1;
    }
    if (Expect(selector->itemData(1).toString() ==
                       QStringLiteral("run-project-script") &&
                   selector->itemText(1) ==
                       QStringLiteral("Project Script Runner"),
               "Python API selector should preserve operation order"))
    {
        delete context;
        return 1;
    }
    if (Expect(!selector->isVisible(),
               "Python API page should keep the generic operation selector hidden behind Workbench panels"))
    {
        delete context;
        return 1;
    }

    auto* runtimeGroup = window.findChild<QGroupBox*>(
        QStringLiteral("xqPythonApiRuntimeGroup"));
    auto* snippetGroup = window.findChild<QGroupBox*>(
        QStringLiteral("xqPythonApiSnippetGroup"));
    auto* scriptGroup = window.findChild<QGroupBox*>(
        QStringLiteral("xqPythonApiScriptGroup"));
    auto* runtimeStatus = window.findChild<QLabel*>(
        QStringLiteral("xqPythonApiRuntimeStatusLabel"));
    auto* scriptNotice = window.findChild<QLabel*>(
        QStringLiteral("xqPythonApiScriptUnavailableNotice"));
    auto* consoleButton = window.findChild<QPushButton*>(
        QStringLiteral("xqPythonApiConsoleButton"));
    auto* snippetButton = window.findChild<QPushButton*>(
        QStringLiteral("xqPythonApiSnippetButton"));
    auto* scriptButton = window.findChild<QPushButton*>(
        QStringLiteral("xqPythonApiScriptButton"));

    if (Expect(runtimeGroup != nullptr &&
                   runtimeGroup->title() == QStringLiteral("Runtime Status"),
               "Python API page should restore a runtime status panel"))
    {
        delete context;
        return 1;
    }
    if (Expect(snippetGroup != nullptr &&
                   snippetGroup->title() == QStringLiteral("Snippet Catalog"),
               "Python API page should restore a snippet catalog panel"))
    {
        delete context;
        return 1;
    }
    if (Expect(scriptGroup != nullptr &&
                   scriptGroup->title() ==
                       QStringLiteral("Project Script Runner"),
               "Python API page should restore a project script panel"))
    {
        delete context;
        return 1;
    }
    if (Expect(runtimeStatus != nullptr &&
                   runtimeStatus->text().contains(
                       QStringLiteral("Python API unavailable")),
               "Python API runtime panel should state runtime availability honestly"))
    {
        delete context;
        return 1;
    }
    if (Expect(scriptNotice != nullptr &&
                   scriptNotice->text().contains(
                       QStringLiteral("runtime is unavailable")),
               "Python API script panel should show the v1 runtime guard"))
    {
        delete context;
        return 1;
    }
    if (Expect(consoleButton != nullptr &&
                   consoleButton->text() ==
                       QStringLiteral("Check Runtime") &&
                   snippetButton != nullptr &&
                   snippetButton->text() ==
                       QStringLiteral("Export Snippets") &&
                   scriptButton != nullptr &&
                   scriptButton->text() ==
                       QStringLiteral("Run Project Script"),
               "Python API page should expose concrete Workbench command buttons"))
    {
        delete context;
        return 1;
    }

    selector->setCurrentIndex(
        selector->findData(QStringLiteral("run-project-script")));
    app.processEvents();
    auto* actionButton = FindActionButton(window, QStringLiteral("python-api"));
    if (Expect(actionButton != nullptr &&
                   actionButton->text() ==
                       QStringLiteral("Run Project Script Runner"),
               "Python API action should include selected operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindIntegerParameter(
                   window,
                   QStringLiteral("script-timeout-seconds")) != nullptr &&
                   FindIntegerParameter(
                       window,
                       QStringLiteral("max-output-lines")) != nullptr,
               "Project Script Runner should expose Python API parameters"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("python-api")),
               "Python API workflow should be selectable before run"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(actionButton->isEnabled(),
               "Python API action should be enabled without selected data"))
    {
        delete context;
        return 1;
    }
    auto* statusLabel = FindStatusLabel(window, QStringLiteral("python-api"));
    if (Expect(statusLabel != nullptr &&
                   statusLabel->text() == QStringLiteral("Ready."),
               "Python API status should not ask for selected data"))
    {
        delete context;
        return 1;
    }

    QStringList diagnostics;
    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&diagnostics](const QString& message) {
                         diagnostics.append(message);
                     });
    actionButton->click();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Run Python API failed: Python project script runtime is unavailable. Python API unavailable in this build: the xq Python extension module was not built because pybind11 and a matching Python 3.11 ABI are not linked. The C++ inspection service remains available for tests and internal callers.")),
               "Python API script action should report unavailable runtime"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
