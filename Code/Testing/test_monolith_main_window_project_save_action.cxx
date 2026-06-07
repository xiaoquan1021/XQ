#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_ProjectService.h"
#include "Core/xq_TaskRunner.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    const QString projectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("SaveStudy.xqproj"));

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    auto* saveAction =
        window.findChild<QAction*>(QStringLiteral("xqSaveProjectAction"));
    if (Expect(saveAction != nullptr,
               "MainWindow should expose a save project action"))
    {
        delete context;
        return 1;
    }
    if (Expect(!saveAction->isEnabled(),
               "save project action should start disabled"))
    {
        delete context;
        return 1;
    }

    int diagnostics = 0;
    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&diagnostics](const QString&) {
                         ++diagnostics;
                     });

    QString errorMessage;
    if (Expect(context->Projects()->CreateProject(QStringLiteral("SaveStudy"),
                                                  projectPath,
                                                  &errorMessage),
               "creating a project should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(saveAction->isEnabled(),
               "save project action should enable after project create"))
    {
        delete context;
        return 1;
    }

    const auto importResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001")),
                                       &errorMessage);
    if (Expect(importResult.Succeeded,
               "import before save should succeed"))
    {
        delete context;
        return 1;
    }

    saveAction->trigger();
    app.processEvents();

    const QJsonObject savedRoot = ReadJsonObject(projectPath);
    const QJsonObject savedProject =
        savedRoot.value(QStringLiteral("project")).toObject();
    if (Expect(savedProject.value(QStringLiteral("name")).toString() ==
                   QStringLiteral("SaveStudy"),
               "save action should write project metadata"))
    {
        delete context;
        return 1;
    }
    if (Expect(savedProject.value(QStringLiteral("dataCatalog"))
                   .toArray()
                   .size() == 1,
               "save action should write data catalog metadata"))
    {
        delete context;
        return 1;
    }
    if (Expect(savedProject.value(QStringLiteral("dataHierarchy"))
                   .toArray()
                   .size() >= 2,
               "save action should write data hierarchy metadata"))
    {
        delete context;
        return 1;
    }
    if (Expect(!context->Tasks()->History().isEmpty() &&
                   context->Tasks()->History().last().Name ==
                       QStringLiteral("Save Project") &&
                   context->Tasks()->History().last().Succeeded,
               "save action should run through ProjectSessionService"))
    {
        delete context;
        return 1;
    }

    delete context;

    auto* emptyContext = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow emptyWindow(*emptyContext);
    auto* staleSaveAction =
        emptyWindow.findChild<QAction*>(QStringLiteral("xqSaveProjectAction"));
    if (Expect(staleSaveAction != nullptr,
               "empty window should expose a save project action"))
    {
        delete emptyContext;
        return 1;
    }

    int staleDiagnostics = 0;
    QObject::connect(emptyContext,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&staleDiagnostics](const QString&) {
                         ++staleDiagnostics;
                     });

    staleSaveAction->setEnabled(true);
    staleSaveAction->trigger();
    app.processEvents();

    if (Expect(staleDiagnostics > 0,
               "forced stale save should post a diagnostic"))
    {
        delete emptyContext;
        return 1;
    }
    if (Expect(!staleSaveAction->isEnabled(),
               "forced stale save should disable without an active project"))
    {
        delete emptyContext;
        return 1;
    }

    delete emptyContext;
    return 0;
}
