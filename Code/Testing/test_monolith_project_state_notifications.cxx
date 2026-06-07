#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_ProjectService.h"
#include "Core/xq_ProjectSessionService.h"

#include <QCoreApplication>
#include <QDir>
#include <QObject>
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

xq::core::DataImportRequest MakeImageImport(const QString& id,
                                            const QString& displayName)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = QStringLiteral("C:/studies/") + id;
    request.DisplayName = displayName;
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

bool WriteProjectWithImage(const QString& projectPath,
                           const QString& projectName,
                           const QString& imageId,
                           QString* errorMessage)
{
    auto* context = xq::core::ApplicationContext::CreateDefault();
    const bool created =
        context->Projects()->CreateProject(projectName,
                                           projectPath,
                                           errorMessage);
    if (!created)
    {
        delete context;
        return false;
    }

    const auto importResult =
        context->DataImports()->Import(MakeImageImport(imageId,
                                                       imageId),
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
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    const QString firstProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("FirstStudy.xqproj"));
    const QString secondProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("SecondStudy.xqproj"));

    QString errorMessage;
    xq::core::ProjectService projectService;
    int projectChanges = 0;
    xq::core::ProjectMetadata lastProject;

    QObject::connect(&projectService,
                     &xq::core::ProjectService::ProjectChanged,
                     [&projectChanges,
                      &lastProject](const xq::core::ProjectMetadata& project) {
                         ++projectChanges;
                         lastProject = project;
                     });

    if (Expect(!projectService.CreateProject(QString(),
                                             firstProjectPath,
                                             &errorMessage),
               "failed create should reject empty project names"))
        return 1;
    if (Expect(projectChanges == 0,
               "failed create should not emit ProjectChanged"))
        return 1;

    if (Expect(projectService.CreateProject(QStringLiteral("FirstStudy"),
                                            firstProjectPath,
                                            &errorMessage),
               "successful create should accept project metadata"))
        return 1;
    if (Expect(projectChanges == 1,
               "successful create should emit one ProjectChanged"))
        return 1;
    if (Expect(lastProject.Name == QStringLiteral("FirstStudy"),
               "create ProjectChanged should include project name"))
        return 1;
    if (Expect(lastProject.ProjectFilePath == firstProjectPath,
               "create ProjectChanged should include project path"))
        return 1;

    if (Expect(projectService.SaveProject(&errorMessage),
               "save should persist the active project"))
        return 1;
    if (Expect(projectChanges == 1,
               "save should not emit ProjectChanged"))
        return 1;

    const QString missingPath =
        QDir(tempDir.path()).filePath(QStringLiteral("Missing.xqproj"));
    if (Expect(!projectService.OpenProject(missingPath, &errorMessage),
               "failed open should reject missing project files"))
        return 1;
    if (Expect(projectChanges == 1,
               "failed open should not emit ProjectChanged"))
        return 1;
    if (Expect(projectService.CurrentProject() != nullptr &&
                   projectService.CurrentProject()->Name ==
                       QStringLiteral("FirstStudy"),
               "failed open should preserve the previous current project"))
        return 1;

    if (Expect(WriteProjectWithImage(secondProjectPath,
                                     QStringLiteral("SecondStudy"),
                                     QStringLiteral("image-002"),
                                     &errorMessage),
               "second project fixture should be saved"))
        return 1;

    xq::core::DataCatalogService catalog;
    xq::core::DataHierarchyService hierarchy;
    if (Expect(projectService.OpenProject(secondProjectPath,
                                          catalog,
                                          hierarchy,
                                          &errorMessage),
               "successful open should load project data"))
        return 1;
    if (Expect(projectChanges == 2,
               "successful open should emit one ProjectChanged"))
        return 1;
    if (Expect(lastProject.Name == QStringLiteral("SecondStudy"),
               "open ProjectChanged should include loaded project name"))
        return 1;
    if (Expect(projectService.CurrentProject() != nullptr &&
                   projectService.CurrentProject()->Name ==
                       QStringLiteral("SecondStudy"),
               "successful open should preserve loaded metadata"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    int contextProjectChanges = 0;
    QObject::connect(context->Projects(),
                     &xq::core::ProjectService::ProjectChanged,
                     [&contextProjectChanges](
                         const xq::core::ProjectMetadata&) {
                         ++contextProjectChanges;
                     });

    if (Expect(context->ProjectSession()->Open(secondProjectPath,
                                               &errorMessage),
               "ProjectSession open should load through ProjectService"))
    {
        delete context;
        return 1;
    }
    if (Expect(contextProjectChanges == 1,
               "ProjectSession open should emit one project change"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
