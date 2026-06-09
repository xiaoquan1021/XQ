#include "Core/xq_ApplicationContext.h"
#include "Presentation/xq_MainWindow.h"

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

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

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
    auto* mainToolbar =
        window.findChild<QToolBar*>(QStringLiteral("mainActionsToolBar"));
    if (Expect(mainToolbar != nullptr,
               "Workbench layout should restore the original main actions toolbar name"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
