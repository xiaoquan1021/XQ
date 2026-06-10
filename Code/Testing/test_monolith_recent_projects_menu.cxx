#include "Core/xq_ApplicationContext.h"
#include "Core/xq_PreferencesService.h"
#include "Core/xq_ProjectFilePathProvider.h"
#include "Core/xq_ProjectService.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QMenu>
#include <QTemporaryDir>

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

class FakeProjectPathProvider : public xq::core::ProjectFilePathProvider
{
public:
    QString NextNewProjectName;
    QString NextNewProjectPath;
    QString NextOpenProjectPath;
    QString SaveAsName;
    QString SaveAsPath;
    int NewRequests = 0;
    int OpenRequests = 0;
    int SaveAsRequests = 0;

    xq::core::ProjectFilePath NewProjectFilePath() override
    {
        ++NewRequests;
        return {NextNewProjectName, NextNewProjectPath};
    }

    QString OpenProjectFilePath() override
    {
        ++OpenRequests;
        return NextOpenProjectPath;
    }

    xq::core::ProjectFilePath SaveAsProjectFilePath(
        const xq::core::ProjectMetadata&) override
    {
        ++SaveAsRequests;
        return {SaveAsName, SaveAsPath};
    }
};

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

QAction* FirstRecentProjectAction(QMenu* menu)
{
    if (!menu || menu->actions().isEmpty())
        return nullptr;

    return menu->actions().first();
}

bool RecentMenuShowsPath(QMenu* menu, const QString& path)
{
    if (!menu)
        return false;

    for (auto* action : menu->actions())
    {
        if (action && action->data().toString() == path)
            return true;
    }

    return false;
}

bool CreateProjectFile(const QString& projectName,
                       const QString& path,
                       QString* errorMessage)
{
    xq::core::ProjectService service;
    return service.CreateProject(projectName, path, errorMessage) &&
           service.SaveProject(errorMessage);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    const QString createdPath =
        QDir(tempDir.path()).filePath(QStringLiteral("Created.xqproj"));
    const QString savedAsPath =
        QDir(tempDir.path()).filePath(QStringLiteral("SavedAs.xqproj"));
    const QString externalPath =
        QDir(tempDir.path()).filePath(QStringLiteral("External.xqproj"));

    QString errorMessage;
    if (Expect(CreateProjectFile(QStringLiteral("External"),
                                 externalPath,
                                 &errorMessage),
               "external project fixture should be writable"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    FakeProjectPathProvider provider;
    provider.NextNewProjectName = QStringLiteral("Created");
    provider.NextNewProjectPath = createdPath;
    provider.NextOpenProjectPath = externalPath;
    provider.SaveAsName = QStringLiteral("SavedAs");
    provider.SaveAsPath = savedAsPath;
    window.SetProjectFilePathProvider(&provider);
    window.show();
    app.processEvents();

    auto* recentMenu = FindMenu(window, QStringLiteral("xqRecentProjectsMenu"));
    auto* newProjectAction =
        FindAction(window, QStringLiteral("xqNewProjectAction"));
    auto* openProjectAction =
        FindAction(window, QStringLiteral("xqOpenProjectAction"));
    auto* saveAsAction =
        FindAction(window, QStringLiteral("xqSaveAsProjectAction"));
    if (Expect(recentMenu && newProjectAction && openProjectAction &&
                   saveAsAction,
               "project menu actions and Recent Projects menu should exist"))
    {
        delete context;
        return 1;
    }

    auto* emptyAction = FirstRecentProjectAction(recentMenu);
    if (Expect(emptyAction &&
                   emptyAction->text() == QStringLiteral("(No recent projects)") &&
                   !emptyAction->isEnabled(),
               "empty Recent Projects menu should show a disabled placeholder"))
    {
        delete context;
        return 1;
    }

    newProjectAction->trigger();
    app.processEvents();
    if (Expect(provider.NewRequests == 1 &&
                   RecentMenuShowsPath(recentMenu, createdPath),
               "New Project should add the created project to recent history"))
    {
        delete context;
        return 1;
    }
    if (Expect(FirstRecentProjectAction(recentMenu)->data().toString() ==
                   createdPath,
               "created project should be first in Recent Projects"))
    {
        delete context;
        return 1;
    }

    saveAsAction->trigger();
    app.processEvents();
    if (Expect(provider.SaveAsRequests == 1 &&
                   RecentMenuShowsPath(recentMenu, savedAsPath),
               "Save As should add the new project path to recent history"))
    {
        delete context;
        return 1;
    }
    if (Expect(FirstRecentProjectAction(recentMenu)->data().toString() ==
                       savedAsPath &&
                   !RecentMenuShowsPath(recentMenu, createdPath),
               "Save As should promote the new path and drop the old active path"))
    {
        delete context;
        return 1;
    }

    openProjectAction->trigger();
    app.processEvents();
    if (Expect(provider.OpenRequests == 1 &&
                   RecentMenuShowsPath(recentMenu, externalPath),
               "Open Project should add the opened project to recent history"))
    {
        delete context;
        return 1;
    }
    if (Expect(FirstRecentProjectAction(recentMenu)->data().toString() ==
                   externalPath,
               "opened project should be promoted to first recent item"))
    {
        delete context;
        return 1;
    }

    FirstRecentProjectAction(recentMenu)->trigger();
    app.processEvents();
    if (Expect(provider.OpenRequests == 1,
               "opening from Recent Projects should not ask the path provider"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Projects()->CurrentProject() &&
                   context->Projects()->CurrentProject()->Name ==
                       QStringLiteral("External"),
               "opening from Recent Projects should open the stored project path"))
    {
        delete context;
        return 1;
    }

    auto* secondContext = xq::core::ApplicationContext::CreateDefault();
    secondContext->Preferences()->SetStringValue(
        QStringLiteral("project.recent.0"),
        externalPath);
    xq::presentation::MainWindow secondWindow(*secondContext);
    auto* secondRecentMenu =
        FindMenu(secondWindow, QStringLiteral("xqRecentProjectsMenu"));
    if (Expect(secondRecentMenu &&
                   FirstRecentProjectAction(secondRecentMenu)->data().toString() ==
                       externalPath,
               "Recent Projects menu should rebuild from stored preferences"))
    {
        delete context;
        delete secondContext;
        return 1;
    }

    delete context;
    delete secondContext;
    return 0;
}
