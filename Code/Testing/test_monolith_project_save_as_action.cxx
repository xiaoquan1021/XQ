#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_ProjectFilePathProvider.h"
#include "Core/xq_ProjectSessionService.h"
#include "Core/xq_ProjectService.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
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
    QString SaveAsName;
    QString SaveAsPath;
    int SaveAsRequests = 0;

    xq::core::ProjectFilePath NewProjectFilePath() override { return {}; }
    QString OpenProjectFilePath() override { return {}; }

    xq::core::ProjectFilePath SaveAsProjectFilePath(
        const xq::core::ProjectMetadata&) override
    {
        ++SaveAsRequests;
        return {SaveAsName, SaveAsPath};
    }
};

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

QJsonObject ReadJsonObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return {};

    return document.object();
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

    const QString originalPath =
        QDir(tempDir.path()).filePath(QStringLiteral("Original.xqproj"));
    const QString saveAsPath =
        QDir(tempDir.path()).filePath(QStringLiteral("SavedAs.xqproj"));

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    FakeProjectPathProvider provider;
    provider.SaveAsName = QStringLiteral("SavedAs");
    provider.SaveAsPath = saveAsPath;
    window.SetProjectFilePathProvider(&provider);

    auto* saveAsAction =
        FindAction(window, QStringLiteral("xqSaveAsProjectAction"));
    auto* saveAction =
        FindAction(window, QStringLiteral("xqSaveProjectAction"));
    auto* nameLabel = FindLabel(window, QStringLiteral("xqProjectPageName"));
    auto* pathLabel = FindLabel(window, QStringLiteral("xqProjectPagePath"));
    if (Expect(saveAsAction != nullptr && saveAction != nullptr,
               "Project Save As and Save actions should exist"))
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

    saveAsAction->setEnabled(true);
    saveAsAction->trigger();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Save As failed: No active project to save.")),
               "Save As without an active project should report a diagnostic"))
    {
        delete context;
        return 1;
    }
    if (Expect(provider.SaveAsRequests == 0,
               "Save As without an active project should not ask for a path"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    if (Expect(context->Projects()->CreateProject(QStringLiteral("Original"),
                                                  originalPath,
                                                  &errorMessage),
               "creating an original project should succeed"))
    {
        delete context;
        return 1;
    }
    const auto importResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001")),
                                       &errorMessage);
    if (Expect(importResult.Succeeded, "test image import should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    saveAsAction->trigger();
    app.processEvents();

    if (Expect(provider.SaveAsRequests == 1,
               "Save As should ask the provider for a new path"))
    {
        delete context;
        return 1;
    }
    if (Expect(QFile::exists(saveAsPath),
               "Save As should write the selected project file"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Projects()->CurrentProject()->Name ==
                   QStringLiteral("SavedAs") &&
                   context->Projects()->CurrentProject()->ProjectFilePath ==
                       saveAsPath,
               "Save As should update the active project metadata"))
    {
        delete context;
        return 1;
    }
    if (Expect(nameLabel->text() == QStringLiteral("SavedAs") &&
                   pathLabel->text() == saveAsPath,
               "Save As should refresh the Project workflow page"))
    {
        delete context;
        return 1;
    }
    if (Expect(saveAction->isEnabled(),
               "Save should remain enabled after Save As"))
    {
        delete context;
        return 1;
    }

    const QJsonObject savedRoot = ReadJsonObject(saveAsPath);
    const QJsonObject savedProject =
        savedRoot.value(QStringLiteral("project")).toObject();
    if (Expect(savedProject.value(QStringLiteral("name")).toString() ==
                   QStringLiteral("SavedAs") &&
                   savedProject.value(QStringLiteral("dataCatalog"))
                           .toArray()
                           .size() == 1,
               "Save As should persist renamed project metadata and catalog"))
    {
        delete context;
        return 1;
    }

    auto* reopenedContext = xq::core::ApplicationContext::CreateDefault();
    if (Expect(reopenedContext->ProjectSession()->Open(saveAsPath,
                                                       &errorMessage),
               "Saved As project should reopen through ProjectSessionService"))
    {
        delete context;
        delete reopenedContext;
        return 1;
    }
    if (Expect(reopenedContext->Projects()->CurrentProject()->Name ==
                   QStringLiteral("SavedAs") &&
                   reopenedContext->DataCatalog()->Entries().size() == 1,
               "Saved As project should restore metadata and catalog"))
    {
        delete context;
        delete reopenedContext;
        return 1;
    }

    delete context;
    delete reopenedContext;
    return 0;
}
