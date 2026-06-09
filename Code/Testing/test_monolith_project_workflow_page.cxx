#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataManagementService.h"
#include "Core/xq_ProjectSessionService.h"
#include "Core/xq_ProjectService.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItem>
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
    auto* infoGroup = projectPage->findChild<QGroupBox*>(
        QStringLiteral("xqProjectInformationGroup"));
    auto* projectTree = projectPage->findChild<QTreeWidget*>(
        QStringLiteral("xqProjectStructureTree"));
    auto* newProjectButton = projectPage->findChild<QPushButton*>(
        QStringLiteral("xqProjectNewButton"));
    auto* openProjectButton = projectPage->findChild<QPushButton*>(
        QStringLiteral("xqProjectOpenButton"));
    auto* refreshButton = projectPage->findChild<QPushButton*>(
        QStringLiteral("xqProjectRefreshButton"));
    auto* openFolderButton = projectPage->findChild<QPushButton*>(
        QStringLiteral("xqProjectOpenFolderButton"));

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
    if (Expect(infoGroup != nullptr &&
                   infoGroup->title() == QStringLiteral("Project Information"),
               "project page should restore the Workspace Explorer project information group"))
    {
        delete context;
        return 1;
    }
    if (Expect(projectTree != nullptr &&
                   projectTree->headerItem()->text(0) ==
                       QStringLiteral("Project Structure"),
               "project page should restore the Workspace Explorer project structure tree"))
    {
        delete context;
        return 1;
    }
    if (Expect(projectTree->topLevelItemCount() == 1 &&
                   projectTree->topLevelItem(0)->text(0) ==
                       QStringLiteral("(No project loaded)"),
               "project structure tree should start with the original no-project root"))
    {
        delete context;
        return 1;
    }
    if (Expect(newProjectButton != nullptr &&
                   newProjectButton->text() == QStringLiteral("New Project"),
               "project page should expose the original New Project command anchor"))
    {
        delete context;
        return 1;
    }
    if (Expect(openProjectButton != nullptr &&
                   openProjectButton->text() == QStringLiteral("Open Project"),
               "project page should expose the original Open Project command anchor"))
    {
        delete context;
        return 1;
    }
    if (Expect(refreshButton != nullptr &&
                   refreshButton->text() == QStringLiteral("Refresh") &&
                   !refreshButton->isEnabled(),
               "project page should expose a disabled Refresh command before a project is loaded"))
    {
        delete context;
        return 1;
    }
    if (Expect(openFolderButton != nullptr &&
                   openFolderButton->text() ==
                       QStringLiteral("Open Project Folder") &&
                   !openFolderButton->isEnabled(),
               "project page should expose a disabled Open Project Folder command before a project is loaded"))
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
    if (Expect(refreshButton->isEnabled() &&
                   openFolderButton->isEnabled(),
               "project create should enable project refresh and folder commands"))
    {
        delete context;
        return 1;
    }
    if (Expect(projectTree->topLevelItemCount() == 1 &&
                   projectTree->topLevelItem(0)->text(0) ==
                       QStringLiteral("WorkflowStudy"),
               "project create should update the project structure root"))
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
    if (Expect(projectTree->topLevelItem(0)->childCount() >= 1 &&
                   projectTree->topLevelItem(0)->child(0)->text(0) ==
                       QStringLiteral("Images [1]") &&
                   projectTree->topLevelItem(0)->child(0)->childCount() == 1 &&
                   projectTree->topLevelItem(0)->child(0)->child(0)->text(0) ==
                       QStringLiteral("image-001"),
               "image import should populate the project structure tree"))
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
    if (Expect(projectTree->topLevelItem(0)->childCount() == 0,
               "data remove should update the project structure tree"))
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
