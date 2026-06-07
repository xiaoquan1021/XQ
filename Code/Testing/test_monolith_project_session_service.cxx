#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_ProjectService.h"
#include "Core/xq_ProjectSessionService.h"
#include "Core/xq_TaskRunner.h"

#include <QCoreApplication>
#include <QDir>
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

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    const QString projectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("SessionStudy.xqproj"));

    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->ProjectSession() != nullptr,
               "ApplicationContext should expose ProjectSessionService"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    if (Expect(context->Projects()->CreateProject(QStringLiteral("SessionStudy"),
                                                  projectPath,
                                                  &errorMessage),
               "CreateProject should create a session project"))
    {
        delete context;
        return 1;
    }

    xq::core::DataImportRequest importRequest;
    importRequest.RequestedId = QStringLiteral("session-image");
    importRequest.SourcePath = QStringLiteral("C:/data/session-image");
    importRequest.DisplayName = QStringLiteral("Session Image");
    importRequest.Modality = QStringLiteral("CT");
    importRequest.WorkflowRole = xq::core::DataWorkflowRole::Image;

    const auto importResult =
        context->DataImports()->Import(importRequest, &errorMessage);
    if (Expect(importResult.Succeeded,
               "context import should succeed before session save"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->ProjectSession()->Save(&errorMessage),
               "ProjectSession save should persist project and catalog"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Tasks()->History().size() == 2,
               "ProjectSession save should add a task after import"))
    {
        delete context;
        return 1;
    }

    delete context;

    auto* reopenedContext = xq::core::ApplicationContext::CreateDefault();
    if (Expect(reopenedContext->ProjectSession()->Open(projectPath,
                                                       &errorMessage),
               "ProjectSession open should load project and catalog"))
    {
        delete reopenedContext;
        return 1;
    }
    if (Expect(reopenedContext->Projects()->CurrentProject() != nullptr,
               "ProjectSession open should restore project metadata"))
    {
        delete reopenedContext;
        return 1;
    }
    if (Expect(reopenedContext->Projects()->CurrentProject()->Name ==
                   QStringLiteral("SessionStudy"),
               "ProjectSession open should restore project name"))
    {
        delete reopenedContext;
        return 1;
    }
    if (Expect(reopenedContext->DataCatalog()->Entries().size() == 1,
               "ProjectSession open should restore catalog entries"))
    {
        delete reopenedContext;
        return 1;
    }
    if (Expect(reopenedContext->DataCatalog()->Entries().at(0).Id ==
                   QStringLiteral("session-image"),
               "ProjectSession open should restore catalog entry ids"))
    {
        delete reopenedContext;
        return 1;
    }
    if (Expect(reopenedContext->Tasks()->History().size() == 1,
               "ProjectSession open should record a task"))
    {
        delete reopenedContext;
        return 1;
    }
    if (Expect(reopenedContext->Tasks()->History().at(0).Succeeded,
               "ProjectSession open task should succeed"))
    {
        delete reopenedContext;
        return 1;
    }

    delete reopenedContext;
    return 0;
}
