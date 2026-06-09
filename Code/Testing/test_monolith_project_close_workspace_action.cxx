#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_ProjectService.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTreeWidget>

#include <mitkDataNode.h>

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

xq::core::DataImportRequest MakeImageImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("image-001");
    request.SourcePath = QStringLiteral("C:/studies/image-001");
    request.DisplayName = QStringLiteral("CTA A");
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

QAction* FindAction(xq::presentation::MainWindow& window,
                    const QString& objectName)
{
    return window.findChild<QAction*>(objectName);
}

QLabel* FindLabel(xq::presentation::MainWindow& window,
                  const QString& objectName)
{
    return window.findChild<QLabel*>(objectName);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    window.show();
    app.processEvents();

    auto* closeAction =
        FindAction(window, QStringLiteral("xqCloseProjectAction"));
    auto* saveAction =
        FindAction(window, QStringLiteral("xqSaveProjectAction"));
    auto* projectName =
        FindLabel(window, QStringLiteral("xqProjectPageName"));
    auto* projectPath =
        FindLabel(window, QStringLiteral("xqProjectPagePath"));
    auto* dataCount =
        FindLabel(window, QStringLiteral("xqProjectPageDataCount"));
    auto* projectTree =
        window.findChild<QTreeWidget*>(QStringLiteral("xqProjectStructureTree"));
    if (Expect(closeAction != nullptr && saveAction != nullptr,
               "Close Workspace and Save actions should exist"))
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

    closeAction->trigger();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Close Workspace skipped: no active project.")),
               "Close Workspace without a project should report a no-op diagnostic"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const QString projectFilePath =
        tempDir.filePath(QStringLiteral("CloseStudy.xqproj"));
    if (Expect(context->Projects()->CreateProject(QStringLiteral("CloseStudy"),
                                                  projectFilePath,
                                                  &errorMessage),
               "creating a project should succeed"))
    {
        delete context;
        return 1;
    }
    const auto importResult =
        context->DataImports()->Import(MakeImageImport(), &errorMessage);
    if (Expect(importResult.Succeeded, "test data import should succeed"))
    {
        delete context;
        return 1;
    }
    auto node = mitk::DataNode::New();
    node->SetName("CTA A");
    context->DataStorage()->Add(node);
    if (Expect(context->DataNodes()->BindNode(QStringLiteral("image-001"),
                                              node,
                                              &errorMessage),
               "test should bind imported data to a node"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectCatalogEntry(
                   QStringLiteral("image-001"),
                   &errorMessage),
               "test should select imported data"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(context->Projects()->HasActiveProject() &&
                   !context->DataCatalog()->Entries().isEmpty() &&
                   !context->DataNodes()->CatalogEntryIds().isEmpty() &&
                   !context->DataStorage()->GetAll()->empty() &&
                   context->DataSelection()->HasSelection(),
               "test setup should have active project, data, node, and selection"))
    {
        delete context;
        return 1;
    }

    closeAction->trigger();
    app.processEvents();

    if (Expect(!context->Projects()->HasActiveProject(),
               "Close Workspace should clear the active project"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataCatalog()->Entries().isEmpty(),
               "Close Workspace should clear the data catalog"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataHierarchy()->ChildrenOf(
                   context->DataHierarchy()->RootId()).isEmpty(),
               "Close Workspace should clear hierarchy children"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataNodes()->CatalogEntryIds().isEmpty(),
               "Close Workspace should clear data node bindings"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataStorage()->GetAll()->empty(),
               "Close Workspace should clear MITK DataStorage"))
    {
        delete context;
        return 1;
    }
    if (Expect(!context->DataSelection()->HasSelection(),
               "Close Workspace should clear data selection"))
    {
        delete context;
        return 1;
    }
    if (Expect(!saveAction->isEnabled(),
               "Close Workspace should disable Save"))
    {
        delete context;
        return 1;
    }
    if (Expect(projectName->text() == QStringLiteral("No project loaded") &&
                   projectPath->text() == QStringLiteral("No project file") &&
                   dataCount->text() == QStringLiteral("Data items: 0"),
               "Close Workspace should reset project labels"))
    {
        delete context;
        return 1;
    }
    if (Expect(projectTree->topLevelItemCount() == 1 &&
                   projectTree->topLevelItem(0)->text(0) ==
                       QStringLiteral("(No project loaded)"),
               "Close Workspace should reset the project tree"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.windowTitle() == QStringLiteral("XQ") &&
                   window.statusBar()->currentMessage() ==
                       QStringLiteral("No project"),
               "Close Workspace should reset window title and status"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
