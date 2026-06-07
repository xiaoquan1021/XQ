#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataSelectionService.h"
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
    const bool created = context->Projects()->CreateProject(projectName,
                                                            projectPath,
                                                            errorMessage);
    if (!created)
    {
        delete context;
        return false;
    }

    const auto importResult =
        context->DataImports()->Import(MakeImageImport(imageId,
                                                       projectName +
                                                           QStringLiteral(" Image")),
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

    QString errorMessage;
    const QString projectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("OpenedStudy.xqproj"));
    if (Expect(WriteProjectWithImage(projectPath,
                                     QStringLiteral("OpenedStudy"),
                                     QStringLiteral("opened-image"),
                                     &errorMessage),
               "fixture project should save with an image"))
    {
        return 1;
    }

    auto* context = xq::core::ApplicationContext::CreateDefault();
    int selectionSignals = 0;
    QObject::connect(context->DataSelection(),
                     &xq::core::DataSelectionService::SelectionChanged,
                     [&selectionSignals](const QString&, const QString&) {
                         ++selectionSignals;
                     });

    const auto firstResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA A")),
                                       &errorMessage);
    if (Expect(firstResult.Succeeded,
               "context image import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   QStringLiteral("image-001"),
               "successful import should select the imported catalog entry"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedHierarchyNodeId() ==
                   QStringLiteral("data-image-001"),
               "successful import should select the imported hierarchy node"))
    {
        delete context;
        return 1;
    }
    if (Expect(selectionSignals == 1,
               "successful import should emit one data selection change"))
    {
        delete context;
        return 1;
    }

    errorMessage.clear();
    const auto duplicateResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("Duplicate CTA")),
                                       &errorMessage);
    if (Expect(!duplicateResult.Succeeded,
               "duplicate import should fail"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   QStringLiteral("image-001"),
               "failed import should keep previous catalog selection"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedHierarchyNodeId() ==
                   QStringLiteral("data-image-001"),
               "failed import should keep previous hierarchy selection"))
    {
        delete context;
        return 1;
    }
    if (Expect(selectionSignals == 1,
               "failed import should not emit a selection change"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->ProjectSession()->Open(projectPath, &errorMessage),
               "successful project open should load replacement state"))
    {
        delete context;
        return 1;
    }
    if (Expect(!context->DataSelection()->HasSelection(),
               "successful project open should clear stale data selection"))
    {
        delete context;
        return 1;
    }
    if (Expect(selectionSignals == 2,
               "successful project open should emit one clear selection change"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataSelection()->SelectCatalogEntry(
                   QStringLiteral("opened-image"),
                   &errorMessage),
               "loaded replacement data should be selectable"))
    {
        delete context;
        return 1;
    }
    if (Expect(selectionSignals == 3,
               "selecting replacement data should emit one change"))
    {
        delete context;
        return 1;
    }

    const QString missingProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("MissingStudy.xqproj"));
    errorMessage.clear();
    if (Expect(!context->ProjectSession()->Open(missingProjectPath,
                                                &errorMessage),
               "missing project open should fail"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   QStringLiteral("opened-image"),
               "failed project open should keep previous catalog selection"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedHierarchyNodeId() ==
                   QStringLiteral("data-opened-image"),
               "failed project open should keep previous hierarchy selection"))
    {
        delete context;
        return 1;
    }
    if (Expect(selectionSignals == 3,
               "failed project open should not emit a selection change"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
