#include "Core/xq_ApplicationContext.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>

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

QAction* FindAction(xq::presentation::MainWindow& window,
                    const QString& objectName)
{
    return window.findChild<QAction*>(objectName);
}

QMenu* FindMenu(xq::presentation::MainWindow& window,
                const QString& objectName)
{
    return window.findChild<QMenu*>(objectName);
}

bool MenuContainsAction(QMenu* menu, QAction* action)
{
    if (!menu || !action)
        return false;

    return menu->actions().contains(action);
}

bool ToolbarContainsAction(QToolBar* toolbar, QAction* action)
{
    if (!toolbar || !action)
        return false;

    return toolbar->actions().contains(action);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    window.show();
    app.processEvents();

    auto* fileMenu = FindMenu(window, QStringLiteral("FileMenu"));
    auto* editMenu = FindMenu(window, QStringLiteral("EditMenu"));
    auto* viewMenu = FindMenu(window, QStringLiteral("ViewMenu"));
    auto* toolsMenu = FindMenu(window, QStringLiteral("ToolsMenu"));
    if (Expect(fileMenu != nullptr &&
                   fileMenu->title() == QStringLiteral("&File"),
               "Workbench menu bar should expose the original File menu"))
    {
        delete context;
        return 1;
    }
    if (Expect(editMenu != nullptr &&
                   editMenu->title() == QStringLiteral("&Edit"),
               "Workbench menu bar should expose the original Edit menu"))
    {
        delete context;
        return 1;
    }
    if (Expect(viewMenu != nullptr &&
                   viewMenu->title() == QStringLiteral("&View"),
               "Workbench menu bar should expose the original View menu"))
    {
        delete context;
        return 1;
    }
    if (Expect(toolsMenu != nullptr &&
                   toolsMenu->title() == QStringLiteral("&Tools"),
               "Workbench menu bar should expose the original Tools menu"))
    {
        delete context;
        return 1;
    }

    struct ActionExpectation
    {
        const char* ObjectName;
        const char* Text;
    };

    const ActionExpectation fileActions[] = {
        {"xqNewProjectAction", "New Project"},
        {"xqOpenProjectAction", "Open Project"},
        {"xqSaveProjectAction", "Save"},
        {"xqSaveAsProjectAction", "Save As..."},
        {"xqCloseProjectAction", "Close Workspace"},
        {"xqImportDataAction", "Open Data File..."},
        {"xqImportDicomAction", "Import DICOM..."},
        {"xqSaveSceneAction", "Save All as MITK Scene..."},
        {"xqExitAction", "Exit"},
    };
    for (const auto& expected : fileActions)
    {
        auto* action =
            FindAction(window, QString::fromLatin1(expected.ObjectName));
        if (Expect(action != nullptr,
                   "File menu should expose the original Workbench action"))
        {
            delete context;
            return 1;
        }
        if (Expect(action->text().remove(QLatin1Char('&')) ==
                       QString::fromLatin1(expected.Text),
                   "File menu action should keep original Workbench text"))
        {
            delete context;
            return 1;
        }
        if (Expect(MenuContainsAction(fileMenu, action),
                   "File menu should own every File action"))
        {
            delete context;
            return 1;
        }
    }

    auto* recentProjectsMenu =
        FindMenu(window, QStringLiteral("xqRecentProjectsMenu"));
    if (Expect(recentProjectsMenu != nullptr &&
                   recentProjectsMenu->title() ==
                       QStringLiteral("Recent Projects"),
               "File menu should restore the Recent Projects submenu"))
    {
        delete context;
        return 1;
    }

    auto* undoAction = FindAction(window, QStringLiteral("xqUndoAction"));
    auto* redoAction = FindAction(window, QStringLiteral("xqRedoAction"));
    if (Expect(undoAction != nullptr && redoAction != nullptr &&
                   MenuContainsAction(editMenu, undoAction) &&
                   MenuContainsAction(editMenu, redoAction),
               "Edit menu should restore Undo and Redo actions"))
    {
        delete context;
        return 1;
    }
    if (Expect(undoAction->shortcut().toString() == QStringLiteral("Ctrl+Z") &&
                   redoAction->shortcut().toString() == QStringLiteral("Ctrl+Y"),
               "Undo and Redo actions should keep original shortcuts"))
    {
        delete context;
        return 1;
    }

    auto* screenshotAction =
        FindAction(window, QStringLiteral("xqScreenshotAction"));
    auto* volumeRenderingAction =
        FindAction(window, QStringLiteral("xqVolumeRenderingAction"));
    auto* crosshairAction =
        FindAction(window, QStringLiteral("xqCrosshairAction"));
    auto* viewPresetMenu =
        FindMenu(window, QStringLiteral("xqViewPresetsMenu"));
    if (Expect(screenshotAction != nullptr &&
                   MenuContainsAction(viewMenu, screenshotAction),
               "View menu should restore the Screenshot action"))
    {
        delete context;
        return 1;
    }
    if (Expect(volumeRenderingAction != nullptr &&
                   volumeRenderingAction->isCheckable() &&
                   MenuContainsAction(viewMenu, volumeRenderingAction),
               "View menu should restore checkable Volume Rendering action"))
    {
        delete context;
        return 1;
    }
    if (Expect(crosshairAction != nullptr &&
                   crosshairAction->isCheckable() &&
                   crosshairAction->isChecked() &&
                   MenuContainsAction(viewMenu, crosshairAction),
               "View menu should restore checked Crosshair action"))
    {
        delete context;
        return 1;
    }
    if (Expect(viewPresetMenu != nullptr &&
                   viewPresetMenu->actions().size() >= 3,
               "View menu should restore View Presets submenu"))
    {
        delete context;
        return 1;
    }

    const ActionExpectation toolsActions[] = {
        {"xqOpenPreferencesAction", "Preferences..."},
        {"xqMeasureDistanceAction", "Measure Distance"},
        {"xqMeasureAngleAction", "Measure Angle"},
        {"xqMeasureAreaAction", "Measure Surface Area"},
        {"xqMeasureVolumeAction", "Measure Volume"},
    };
    for (const auto& expected : toolsActions)
    {
        auto* action =
            FindAction(window, QString::fromLatin1(expected.ObjectName));
        if (Expect(action != nullptr &&
                       action->text().remove(QLatin1Char('&')) ==
                           QString::fromLatin1(expected.Text) &&
                       MenuContainsAction(toolsMenu, action),
                   "Tools menu should restore original Workbench utility actions"))
        {
            delete context;
            return 1;
        }
    }

    auto* mainToolbar =
        window.findChild<QToolBar*>(QStringLiteral("mainActionsToolBar"));
    if (Expect(mainToolbar != nullptr,
               "Main Workbench toolbar should exist"))
    {
        delete context;
        return 1;
    }
    if (Expect(ToolbarContainsAction(
                   mainToolbar,
                   FindAction(window, QStringLiteral("xqOpenProjectAction"))) &&
                   ToolbarContainsAction(mainToolbar, undoAction) &&
                   ToolbarContainsAction(mainToolbar, redoAction) &&
                   ToolbarContainsAction(mainToolbar, screenshotAction),
               "Main toolbar should restore project, undo/redo, and screenshot entries"))
    {
        delete context;
        return 1;
    }
    if (Expect(!FindAction(window,
                           QStringLiteral("xqOpenProjectAction"))
                    ->icon()
                    .isNull() &&
                   !undoAction->icon().isNull() &&
                   !screenshotAction->icon().isNull(),
               "Restored toolbar actions should use original XQ icons"))
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
    FindAction(window, QStringLiteral("xqImportDicomAction"))->trigger();
    FindAction(window, QStringLiteral("xqOpenPreferencesAction"))->trigger();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Import DICOM is not available in Windows monolith v1.")) &&
                   diagnostics.contains(QStringLiteral(
                       "Preferences dialog is not available in Windows monolith v1.")),
               "Unmigrated Workbench actions should report honest v1 diagnostics"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
