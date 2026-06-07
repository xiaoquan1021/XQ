#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_ProjectSessionService.h"
#include "Core/xq_ProjectService.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QStatusBar>
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

bool WriteProjectWithImage(const QString& projectPath,
                           const QString& projectName,
                           QString* errorMessage)
{
    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (!context->Projects()->CreateProject(projectName,
                                            projectPath,
                                            errorMessage))
    {
        delete context;
        return false;
    }

    const auto importResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001")),
                                       errorMessage);
    if (!importResult.Succeeded)
    {
        delete context;
        return false;
    }

    const bool saved = context->ProjectSession()->Save(errorMessage);
    delete context;
    return saved;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    const QString firstProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("FirstStudy.xqproj"));
    const QString secondProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("SecondStudy.xqproj"));

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    auto* statusBar =
        window.findChild<QStatusBar*>(QStringLiteral("xqProjectStatusBar"));
    if (Expect(statusBar != nullptr,
               "MainWindow should expose a project status bar"))
    {
        delete context;
        return 1;
    }

    if (Expect(window.windowTitle() == QStringLiteral("XQ"),
               "new MainWindow should start with the base title"))
    {
        delete context;
        return 1;
    }
    if (Expect(statusBar->currentMessage() ==
                   QStringLiteral("No project"),
               "new MainWindow should show a no-project status"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    if (Expect(context->Projects()->CreateProject(QStringLiteral("FirstStudy"),
                                                  firstProjectPath,
                                                  &errorMessage),
               "creating a project through context should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(window.windowTitle() == QStringLiteral("XQ - FirstStudy"),
               "project create should update the window title"))
    {
        delete context;
        return 1;
    }
    if (Expect(statusBar->currentMessage().contains(QStringLiteral("FirstStudy")) &&
                   statusBar->currentMessage().contains(firstProjectPath),
               "project create should update the status message"))
    {
        delete context;
        return 1;
    }

    const QString titleAfterCreate = window.windowTitle();
    const QString statusAfterCreate = statusBar->currentMessage();
    if (Expect(context->ProjectSession()->Save(&errorMessage),
               "project save should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(window.windowTitle() == titleAfterCreate &&
                   statusBar->currentMessage() == statusAfterCreate,
               "save should not change project window state"))
    {
        delete context;
        return 1;
    }

    const QString missingPath =
        QDir(tempDir.path()).filePath(QStringLiteral("Missing.xqproj"));
    if (Expect(!context->ProjectSession()->Open(missingPath, &errorMessage),
               "failed open should reject missing project files"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(window.windowTitle() == titleAfterCreate &&
                   statusBar->currentMessage() == statusAfterCreate,
               "failed open should leave project window state unchanged"))
    {
        delete context;
        return 1;
    }

    if (Expect(WriteProjectWithImage(secondProjectPath,
                                     QStringLiteral("SecondStudy"),
                                     &errorMessage),
               "second project fixture should be saved"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->ProjectSession()->Open(secondProjectPath,
                                               &errorMessage),
               "session open should load the second project"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(window.windowTitle() == QStringLiteral("XQ - SecondStudy"),
               "session open should update the window title"))
    {
        delete context;
        return 1;
    }
    if (Expect(statusBar->currentMessage().contains(QStringLiteral("SecondStudy")) &&
                   statusBar->currentMessage().contains(secondProjectPath),
               "session open should update the status message"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
