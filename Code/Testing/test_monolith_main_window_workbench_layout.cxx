#include "Core/xq_ApplicationContext.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QStackedWidget>
#include <QToolBar>
#include <QTreeView>
#include <QWidget>

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

QDockWidget* FindDock(xq::presentation::MainWindow& window,
                      const QString& objectName)
{
    return window.findChild<QDockWidget*>(objectName);
}

int ExpectDockToggle(xq::presentation::MainWindow& window,
                     QApplication& app,
                     const QString& actionName,
                     QDockWidget* dock)
{
    auto* action = window.findChild<QAction*>(actionName);
    if (Expect(action != nullptr,
               "View menu should expose a dock toggle action"))
        return 1;
    if (Expect(action->isCheckable(),
               "Dock toggle action should be checkable"))
        return 1;
    if (Expect(action->isChecked() == dock->isVisible(),
               "Dock toggle action should mirror dock visibility"))
        return 1;

    action->trigger();
    app.processEvents();
    if (Expect(!dock->isVisible(),
               "Dock toggle action should hide the dock"))
        return 1;
    if (Expect(!action->isChecked(),
               "Dock toggle action should uncheck after hiding"))
        return 1;

    action->trigger();
    app.processEvents();
    if (Expect(dock->isVisible(),
               "Dock toggle action should show the dock again"))
        return 1;
    if (Expect(action->isChecked(),
               "Dock toggle action should check after showing"))
        return 1;

    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    window.show();
    app.processEvents();

    auto* renderHost =
        window.findChild<QWidget*>(QStringLiteral("xqRenderHostContainer"));
    if (Expect(renderHost != nullptr,
               "Workbench layout should expose the MITK render host container"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.centralWidget() == renderHost,
               "MITK render host should be the central workbench area"))
    {
        delete context;
        return 1;
    }

    auto* dataManagerDock =
        FindDock(window, QStringLiteral("xqDataManagerDock"));
    if (Expect(dataManagerDock != nullptr,
               "Workbench layout should expose a left Data Manager dock"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.dockWidgetArea(dataManagerDock) ==
                   Qt::LeftDockWidgetArea,
               "Data Manager dock should be on the left like the original XQ perspective"))
    {
        delete context;
        return 1;
    }
    if (Expect(dataManagerDock->widget() ==
                   window.findChild<QTreeView*>(
                       QStringLiteral("xqDataHierarchyView")),
               "Data Manager dock should own the data hierarchy tree"))
    {
        delete context;
        return 1;
    }

    auto* imageNavigatorDock =
        FindDock(window, QStringLiteral("xqImageNavigatorDock"));
    if (Expect(imageNavigatorDock != nullptr,
               "Workbench layout should expose an Image Navigator dock"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.dockWidgetArea(imageNavigatorDock) ==
                   Qt::LeftDockWidgetArea,
               "Image Navigator should live below the Data Manager in the left workbench column"))
    {
        delete context;
        return 1;
    }
    if (Expect(imageNavigatorDock->widget() != nullptr,
               "Image Navigator dock should contain a widget placeholder in monolith v1"))
    {
        delete context;
        return 1;
    }

    auto* injectedNavigator = new QWidget;
    injectedNavigator->setObjectName(
        QStringLiteral("xqInjectedImageNavigator"));
    window.SetImageNavigatorWidget(injectedNavigator);
    app.processEvents();
    if (Expect(imageNavigatorDock->widget() == injectedNavigator,
               "Image Navigator dock should allow the monolith app to install the real MITK navigator widget"))
    {
        delete context;
        return 1;
    }

    auto* workflowDock =
        FindDock(window, QStringLiteral("xqWorkflowToolsDock"));
    if (Expect(workflowDock != nullptr,
               "Workbench layout should expose workflow tools as a right-side dock"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.dockWidgetArea(workflowDock) == Qt::RightDockWidgetArea,
               "Workflow tools should be on the right instead of replacing the left workbench column"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowDock->findChild<QListWidget*>(
                   QStringLiteral("xqWorkflowNavigation")) != nullptr,
               "Workflow dock should keep discoverable workflow navigation"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowDock->findChild<QStackedWidget*>(
                   QStringLiteral("xqWorkflowPages")) != nullptr,
               "Workflow dock should keep discoverable workflow pages"))
    {
        delete context;
        return 1;
    }

    auto* fileMenu = window.findChild<QMenu*>(QStringLiteral("FileMenu"));
    if (Expect(fileMenu != nullptr,
               "Workbench layout should restore the original File menu entry point"))
    {
        delete context;
        return 1;
    }
    auto* viewMenu = window.findChild<QMenu*>(QStringLiteral("ViewMenu"));
    if (Expect(viewMenu != nullptr,
               "Workbench layout should restore the original View menu entry point"))
    {
        delete context;
        return 1;
    }
    auto* mainToolbar =
        window.findChild<QToolBar*>(QStringLiteral("mainActionsToolBar"));
    if (Expect(mainToolbar != nullptr,
               "Workbench layout should restore the original main actions toolbar name"))
    {
        delete context;
        return 1;
    }

    if (ExpectDockToggle(window,
                         app,
                         QStringLiteral("xqToggleDataManagerDockAction"),
                         dataManagerDock))
    {
        delete context;
        return 1;
    }
    if (ExpectDockToggle(window,
                         app,
                         QStringLiteral("xqToggleImageNavigatorDockAction"),
                         imageNavigatorDock))
    {
        delete context;
        return 1;
    }
    if (ExpectDockToggle(window,
                         app,
                         QStringLiteral("xqToggleWorkflowToolsDockAction"),
                         workflowDock))
    {
        delete context;
        return 1;
    }

    auto* diagnosticsDock =
        FindDock(window, QStringLiteral("xqDiagnosticsDock"));
    auto* taskHistoryDock =
        FindDock(window, QStringLiteral("xqTaskHistoryDock"));
    if (Expect(diagnosticsDock != nullptr,
               "Workbench layout should expose a Diagnostics dock"))
    {
        delete context;
        return 1;
    }
    if (Expect(taskHistoryDock != nullptr,
               "Workbench layout should expose a Task History dock"))
    {
        delete context;
        return 1;
    }
    if (ExpectDockToggle(window,
                         app,
                         QStringLiteral("xqToggleDiagnosticsDockAction"),
                         diagnosticsDock))
    {
        delete context;
        return 1;
    }
    if (ExpectDockToggle(window,
                         app,
                         QStringLiteral("xqToggleTaskHistoryDockAction"),
                         taskHistoryDock))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
