#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataManagementService.h"
#include "Core/xq_ProjectSessionService.h"
#include "Core/xq_ProjectService.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QTemporaryDir>
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

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    auto* projectPage =
        window.findChild<QWidget*>(QStringLiteral("xqWorkflowPage_project"));
    if (Expect(projectPage != nullptr,
               "project workflow page should exist"))
    {
        delete context;
        return 1;
    }

    auto* nameLabel =
        projectPage->findChild<QLabel*>(QStringLiteral("xqProjectPageName"));
    auto* pathLabel =
        projectPage->findChild<QLabel*>(QStringLiteral("xqProjectPagePath"));
    auto* schemaLabel =
        projectPage->findChild<QLabel*>(QStringLiteral("xqProjectPageSchema"));
    auto* dataCountLabel =
        projectPage->findChild<QLabel*>(
            QStringLiteral("xqProjectPageDataCount"));

    if (Expect(nameLabel != nullptr,
               "project page should expose a project name label"))
    {
        delete context;
        return 1;
    }
    if (Expect(pathLabel != nullptr,
               "project page should expose a project path label"))
    {
        delete context;
        return 1;
    }
    if (Expect(schemaLabel != nullptr,
               "project page should expose a schema label"))
    {
        delete context;
        return 1;
    }
    if (Expect(dataCountLabel != nullptr,
               "project page should expose a data count label"))
    {
        delete context;
        return 1;
    }

    if (Expect(nameLabel->text() == QStringLiteral("No project"),
               "project page should start with no project metadata"))
    {
        delete context;
        return 1;
    }
    if (Expect(dataCountLabel->text() == QStringLiteral("Data items: 0"),
               "project page should start with zero data items"))
    {
        delete context;
        return 1;
    }

    const QString projectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("WorkflowStudy.xqproj"));
    QString errorMessage;
    if (Expect(context->Projects()->CreateProject(
                   QStringLiteral("WorkflowStudy"),
                   projectPath,
                   &errorMessage),
               "project create should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(nameLabel->text() == QStringLiteral("WorkflowStudy"),
               "project create should update project page name"))
    {
        delete context;
        return 1;
    }
    if (Expect(pathLabel->text() == projectPath,
               "project create should update project page path"))
    {
        delete context;
        return 1;
    }
    if (Expect(schemaLabel->text() == QStringLiteral("Schema: 2.0"),
               "project create should update project page schema"))
    {
        delete context;
        return 1;
    }

    const auto importResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001")),
                                       &errorMessage);
    if (Expect(importResult.Succeeded, "image import should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(dataCountLabel->text() == QStringLiteral("Data items: 1"),
               "image import should update project page data count"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataManagement()->RemoveEntry(
                   QStringLiteral("image-001"),
                   &errorMessage),
               "data remove should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(dataCountLabel->text() == QStringLiteral("Data items: 0"),
               "data remove should update project page data count"))
    {
        delete context;
        return 1;
    }

    const QString previousName = nameLabel->text();
    const QString previousPath = pathLabel->text();
    const QString missingPath =
        QDir(tempDir.path()).filePath(QStringLiteral("Missing.xqproj"));
    if (Expect(!context->ProjectSession()->Open(missingPath, &errorMessage),
               "failed open should reject missing project files"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(nameLabel->text() == previousName &&
                   pathLabel->text() == previousPath,
               "failed open should leave project page metadata unchanged"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
