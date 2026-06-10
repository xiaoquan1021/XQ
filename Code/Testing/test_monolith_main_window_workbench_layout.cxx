#include "Core/xq_ApplicationContext.h"
#include "Core/xq_WorkflowSelectionService.h"
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
#include <QVBoxLayout>
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

QAction* FindAction(xq::presentation::MainWindow& window,
                    const QString& objectName)
{
    return window.findChild<QAction*>(objectName);
}

bool MenuContainsAction(QMenu* menu, QAction* action)
{
    if (!menu || !action)
        return false;

    return menu->actions().contains(action);
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
    if (Expect(dataManagerDock->widget() != nullptr &&
                   dataManagerDock->findChild<QTreeView*>(
                       QStringLiteral("xqDataHierarchyView")) != nullptr,
               "Data Manager dock should own a panel containing the data hierarchy tree"))
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
    auto* workflowNavigation = workflowDock->findChild<QListWidget*>(
        QStringLiteral("xqWorkflowNavigation"));
    auto* workflowPages = workflowDock->findChild<QStackedWidget*>(
        QStringLiteral("xqWorkflowPages"));
    if (Expect(workflowPages != nullptr,
               "Workflow dock should keep discoverable workflow pages"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowNavigation != nullptr &&
                   !workflowNavigation->isVisible(),
               "Workflow navigation should stay hidden so the right Tools dock shows the selected tool page"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowDock->widget() != nullptr &&
                   workflowDock->widget()->layout() != nullptr &&
                   workflowDock->widget()->layout()->indexOf(workflowPages) >=
                       0,
               "Workflow pages should be the primary visible Tools dock content"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowDock->width() >= 320,
               "Workflow Tools dock should be wide enough to show the selected tool page on startup"))
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

    auto* showDataManagerAction =
        FindAction(window, QStringLiteral("xqShowDataManagerViewAction"));
    auto* showImageNavigatorAction =
        FindAction(window, QStringLiteral("xqShowImageNavigatorViewAction"));
    auto* showWorkspaceExplorerAction =
        FindAction(window, QStringLiteral("xqShowWorkspaceExplorerViewAction"));
    if (Expect(showDataManagerAction != nullptr &&
                   showDataManagerAction->text().remove(QLatin1Char('&')) ==
                       QStringLiteral("Data Manager") &&
                   !showDataManagerAction->isCheckable() &&
                   MenuContainsAction(viewMenu, showDataManagerAction),
               "View menu should restore the original Data Manager view action"))
    {
        delete context;
        return 1;
    }
    if (Expect(showImageNavigatorAction != nullptr &&
                   showImageNavigatorAction->text().remove(QLatin1Char('&')) ==
                       QStringLiteral("Image Navigator") &&
                   !showImageNavigatorAction->isCheckable() &&
                   MenuContainsAction(viewMenu, showImageNavigatorAction),
               "View menu should restore the original Image Navigator view action"))
    {
        delete context;
        return 1;
    }
    if (Expect(showWorkspaceExplorerAction != nullptr &&
                   showWorkspaceExplorerAction->text().remove(QLatin1Char('&')) ==
                       QStringLiteral("Workspace Explorer") &&
                   !showWorkspaceExplorerAction->isCheckable() &&
                   MenuContainsAction(viewMenu, showWorkspaceExplorerAction),
               "View menu should restore the original Workspace Explorer view action"))
    {
        delete context;
        return 1;
    }
    const auto viewActions = viewMenu->actions();
    if (Expect(viewActions.size() >= 3 &&
                   viewActions.at(0) == showDataManagerAction &&
                   viewActions.at(1) == showImageNavigatorAction &&
                   viewActions.at(2) == showWorkspaceExplorerAction,
               "Original Workbench view actions should stay at the top of the View menu"))
    {
        delete context;
        return 1;
    }

    dataManagerDock->hide();
    imageNavigatorDock->hide();
    workflowDock->hide();
    app.processEvents();
    showDataManagerAction->trigger();
    showImageNavigatorAction->trigger();
    app.processEvents();
    if (Expect(dataManagerDock->isVisible() &&
                   imageNavigatorDock->isVisible(),
               "Workbench view actions should show their matching docks without toggling them off"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("meshing")),
               "Layout test should be able to select Meshing before showing Workspace Explorer"))
    {
        delete context;
        return 1;
    }
    showWorkspaceExplorerAction->trigger();
    app.processEvents();
    if (Expect(workflowDock->isVisible() &&
                   context->WorkflowSelection()->SelectedWorkflowId() ==
                       QStringLiteral("project"),
               "Workspace Explorer view action should show the Project workflow page"))
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

    auto* resetViewPresetAction =
        FindAction(window, QStringLiteral("xqResetViewPresetAction"));
    auto* defaultViewPresetAction =
        FindAction(window, QStringLiteral("xqViewPreset_Default"));
    auto* viewerViewPresetAction =
        FindAction(window, QStringLiteral("xqViewPreset_Viewer"));
    auto* analysisViewPresetAction =
        FindAction(window, QStringLiteral("xqViewPreset_Analysis"));
    if (Expect(resetViewPresetAction != nullptr &&
                   resetViewPresetAction->text().remove(QLatin1Char('&')) ==
                       QStringLiteral("Reset View Preset"),
               "View Presets should expose the original Reset View Preset action"))
    {
        delete context;
        return 1;
    }
    if (Expect(defaultViewPresetAction != nullptr &&
                   defaultViewPresetAction->text().remove(QLatin1Char('&')) ==
                       QStringLiteral("Default"),
               "View Presets should expose the original Default preset action"))
    {
        delete context;
        return 1;
    }
    if (Expect(viewerViewPresetAction != nullptr &&
                   viewerViewPresetAction->text().remove(QLatin1Char('&')) ==
                       QStringLiteral("Viewer"),
               "View Presets should expose the original Viewer preset action"))
    {
        delete context;
        return 1;
    }
    if (Expect(analysisViewPresetAction != nullptr &&
                   analysisViewPresetAction->text().remove(QLatin1Char('&')) ==
                       QStringLiteral("Analysis"),
               "View Presets should expose the original Analysis preset action"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("meshing")),
               "Layout test should be able to select a non-default workflow"))
    {
        delete context;
        return 1;
    }
    dataManagerDock->hide();
    imageNavigatorDock->hide();
    workflowDock->hide();
    diagnosticsDock->hide();
    taskHistoryDock->hide();
    app.processEvents();

    resetViewPresetAction->trigger();
    app.processEvents();

    if (Expect(dataManagerDock->isVisible() &&
                   imageNavigatorDock->isVisible() &&
                   workflowDock->isVisible(),
               "Reset View Preset should restore the core Workbench docks"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.dockWidgetArea(dataManagerDock) ==
                       Qt::LeftDockWidgetArea &&
                   window.dockWidgetArea(imageNavigatorDock) ==
                       Qt::LeftDockWidgetArea &&
                   window.dockWidgetArea(workflowDock) ==
                       Qt::RightDockWidgetArea,
               "Reset View Preset should restore the default Workbench dock areas"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectedWorkflowId() ==
                   QStringLiteral("project"),
               "Reset View Preset should return the workflow toolbar to Project"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("meshing")),
               "Layout test should be able to select Meshing before applying Default preset"))
    {
        delete context;
        return 1;
    }
    dataManagerDock->hide();
    imageNavigatorDock->hide();
    workflowDock->hide();
    diagnosticsDock->show();
    taskHistoryDock->show();
    app.processEvents();

    defaultViewPresetAction->trigger();
    app.processEvents();

    if (Expect(dataManagerDock->isVisible() &&
                   imageNavigatorDock->isVisible() &&
                   workflowDock->isVisible(),
               "Default View Preset should restore the three-pane Workbench layout"))
    {
        delete context;
        return 1;
    }
    if (Expect(!diagnosticsDock->isVisible() &&
                   !taskHistoryDock->isVisible(),
               "Default View Preset should hide bottom utility docks"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.dockWidgetArea(dataManagerDock) ==
                       Qt::LeftDockWidgetArea &&
                   window.dockWidgetArea(imageNavigatorDock) ==
                       Qt::LeftDockWidgetArea &&
                   window.dockWidgetArea(workflowDock) ==
                       Qt::RightDockWidgetArea,
               "Default View Preset should restore the expected dock areas"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectedWorkflowId() ==
                   QStringLiteral("project"),
               "Default View Preset should select the Project workflow"))
    {
        delete context;
        return 1;
    }

    viewerViewPresetAction->trigger();
    app.processEvents();

    if (Expect(dataManagerDock->isVisible() &&
                   !imageNavigatorDock->isVisible() &&
                   !workflowDock->isVisible() &&
                   !diagnosticsDock->isVisible() &&
                   !taskHistoryDock->isVisible(),
               "Viewer View Preset should focus the render host with only Data Manager visible"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.dockWidgetArea(dataManagerDock) ==
                   Qt::LeftDockWidgetArea,
               "Viewer View Preset should keep Data Manager on the left"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("flow-simulation")),
               "Layout test should be able to select Flow Simulation before applying Analysis preset"))
    {
        delete context;
        return 1;
    }
    analysisViewPresetAction->trigger();
    app.processEvents();

    if (Expect(dataManagerDock->isVisible() &&
                   workflowDock->isVisible() &&
                   diagnosticsDock->isVisible() &&
                   !imageNavigatorDock->isVisible() &&
                   !taskHistoryDock->isVisible(),
               "Analysis View Preset should show data, tools, and diagnostics"))
    {
        delete context;
        return 1;
    }
    if (Expect(window.dockWidgetArea(dataManagerDock) ==
                       Qt::LeftDockWidgetArea &&
                   window.dockWidgetArea(workflowDock) ==
                       Qt::RightDockWidgetArea &&
                   window.dockWidgetArea(diagnosticsDock) ==
                       Qt::BottomDockWidgetArea,
               "Analysis View Preset should restore analysis dock areas"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectedWorkflowId() ==
                   QStringLiteral("flow-simulation"),
               "Analysis View Preset should preserve the active workflow page"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
