#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_ProjectFilePathProvider.h"
#include "Core/xq_ProjectService.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>

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
    int NewRequests = 0;
    int OpenRequests = 0;

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
        return {};
    }
};

QAction* FindAction(xq::presentation::MainWindow& window,
                    const QString& objectName)
{
    return window.findChild<QAction*>(objectName);
}

QPushButton* FindButton(xq::presentation::MainWindow& window,
                        const QString& objectName)
{
    return window.findChild<QPushButton*>(objectName);
}

QLabel* FindLabel(xq::presentation::MainWindow& window,
                  const QString& objectName)
{
    return window.findChild<QLabel*>(objectName);
}

xq::core::DataImportRequest MakeImageImport(const QString& id)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = QStringLiteral("C:/studies/") + id;
    request.DisplayName = id;
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    const QString createdProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("CreatedStudy.xqproj"));
    const QString openedProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("OpenedStudy.xqproj"));

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    FakeProjectPathProvider provider;
    provider.NextNewProjectName = QStringLiteral("CreatedStudy");
    provider.NextNewProjectPath = createdProjectPath;
    window.SetProjectFilePathProvider(&provider);

    auto* newProjectAction =
        FindAction(window, QStringLiteral("xqNewProjectAction"));
    auto* openProjectAction =
        FindAction(window, QStringLiteral("xqOpenProjectAction"));
    auto* saveProjectAction =
        FindAction(window, QStringLiteral("xqSaveProjectAction"));
    auto* newProjectButton =
        FindButton(window, QStringLiteral("xqProjectNewButton"));
    auto* openProjectButton =
        FindButton(window, QStringLiteral("xqProjectOpenButton"));
    auto* nameLabel = FindLabel(window, QStringLiteral("xqProjectPageName"));
    auto* pathLabel = FindLabel(window, QStringLiteral("xqProjectPagePath"));
    auto* schemaLabel =
        FindLabel(window, QStringLiteral("xqProjectPageSchema"));
    auto* projectTree =
        window.findChild<QTreeWidget*>(QStringLiteral("xqProjectStructureTree"));

    if (Expect(newProjectAction != nullptr &&
                   openProjectAction != nullptr &&
                   saveProjectAction != nullptr,
               "project menu actions should exist"))
    {
        delete context;
        return 1;
    }
    if (Expect(newProjectButton != nullptr &&
                   openProjectButton != nullptr,
               "project page buttons should exist"))
    {
        delete context;
        return 1;
    }

    newProjectAction->trigger();
    app.processEvents();

    if (Expect(provider.NewRequests == 1,
               "New Project action should ask the provider for a path"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Projects()->HasActiveProject(),
               "New Project action should create an active project"))
    {
        delete context;
        return 1;
    }
    if (Expect(nameLabel->text() == QStringLiteral("CreatedStudy") &&
                   pathLabel->text() == createdProjectPath &&
                   schemaLabel->text() == QStringLiteral("Schema: 2.0"),
               "New Project action should update project page metadata"))
    {
        delete context;
        return 1;
    }
    if (Expect(projectTree->topLevelItemCount() == 1 &&
                   projectTree->topLevelItem(0)->text(0) ==
                       QStringLiteral("CreatedStudy"),
               "New Project action should update project tree root"))
    {
        delete context;
        return 1;
    }
    if (Expect(saveProjectAction->isEnabled(),
               "New Project action should enable Save"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto importResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001")),
                                       &errorMessage);
    if (Expect(importResult.Succeeded, "test image import should succeed"))
    {
        delete context;
        return 1;
    }
    saveProjectAction->trigger();
    app.processEvents();

    auto* openerContext = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow openerWindow(*openerContext);
    FakeProjectPathProvider openProvider;
    openProvider.NextOpenProjectPath = createdProjectPath;
    openerWindow.SetProjectFilePathProvider(&openProvider);

    auto* openerOpenAction =
        FindAction(openerWindow, QStringLiteral("xqOpenProjectAction"));
    auto* openerNameLabel =
        FindLabel(openerWindow, QStringLiteral("xqProjectPageName"));
    auto* openerDataCountLabel =
        FindLabel(openerWindow, QStringLiteral("xqProjectPageDataCount"));
    openerOpenAction->trigger();
    app.processEvents();

    if (Expect(openProvider.OpenRequests == 1,
               "Open Project action should ask the provider for a path"))
    {
        delete context;
        delete openerContext;
        return 1;
    }
    if (Expect(openerContext->Projects()->HasActiveProject(),
               "Open Project action should open the selected project"))
    {
        delete context;
        delete openerContext;
        return 1;
    }
    if (Expect(openerNameLabel->text() == QStringLiteral("CreatedStudy") &&
                   openerDataCountLabel->text() ==
                       QStringLiteral("Data items: 1"),
               "Open Project action should restore metadata and catalog"))
    {
        delete context;
        delete openerContext;
        return 1;
    }

    openProvider.NextNewProjectName = QStringLiteral("ButtonStudy");
    openProvider.NextNewProjectPath = openedProjectPath;
    auto* openerNewButton =
        FindButton(openerWindow, QStringLiteral("xqProjectNewButton"));
    openerNewButton->click();
    app.processEvents();
    if (Expect(openProvider.NewRequests == 1 &&
                   openerNameLabel->text() == QStringLiteral("ButtonStudy"),
               "Project page New Project button should share menu behavior"))
    {
        delete context;
        delete openerContext;
        return 1;
    }

    openProvider.NextOpenProjectPath = createdProjectPath;
    auto* openerOpenButton =
        FindButton(openerWindow, QStringLiteral("xqProjectOpenButton"));
    openerOpenButton->click();
    app.processEvents();
    if (Expect(openProvider.OpenRequests == 2 &&
                   openerNameLabel->text() == QStringLiteral("CreatedStudy"),
               "Project page Open Project button should share menu behavior"))
    {
        delete context;
        delete openerContext;
        return 1;
    }

    QStringList diagnostics;
    QObject::connect(openerContext,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&diagnostics](const QString& message) {
                         diagnostics.append(message);
                     });
    openProvider.NextOpenProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("missing.xqproj"));
    openerOpenAction->trigger();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Open Project failed: Project file does not exist.")),
               "Open Project should report service failure diagnostics"))
    {
        delete context;
        delete openerContext;
        return 1;
    }
    if (Expect(openerNameLabel->text() == QStringLiteral("CreatedStudy"),
               "failed Open Project should leave current project visible"))
    {
        delete context;
        delete openerContext;
        return 1;
    }

    delete context;
    delete openerContext;
    return 0;
}
