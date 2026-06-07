#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_ProjectService.h"
#include "Core/xq_ProjectSessionService.h"

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
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    int emittedDiagnostics = 0;
    QString lastDiagnostic;
    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&emittedDiagnostics,
                      &lastDiagnostic](const QString& message) {
                         ++emittedDiagnostics;
                         lastDiagnostic = message;
                     });

    context->PostDiagnostic(QString());
    context->PostDiagnostic(QStringLiteral("   "));
    if (Expect(context->Diagnostics().isEmpty(),
               "empty manual diagnostics should still be ignored"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto importResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001")),
                                       &errorMessage);
    if (Expect(importResult.Succeeded, "image import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Diagnostics().size() == 1,
               "successful import should post one task diagnostic"))
    {
        delete context;
        return 1;
    }
    if (Expect(lastDiagnostic.contains(QStringLiteral("Import image-001")) &&
                   lastDiagnostic.contains(QStringLiteral("Imported data")),
               "successful import diagnostic should include task and message"))
    {
        delete context;
        return 1;
    }

    const auto duplicateImport =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001")),
                                       &errorMessage);
    if (Expect(!duplicateImport.Succeeded,
               "duplicate import should fail"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Diagnostics().size() == 2,
               "failed duplicate import should post one task diagnostic"))
    {
        delete context;
        return 1;
    }
    if (Expect(lastDiagnostic.contains(QStringLiteral("Import image-001")) &&
                   lastDiagnostic.contains(QStringLiteral("failed")),
               "failed import diagnostic should include task failure state"))
    {
        delete context;
        return 1;
    }

    const QString projectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("Diagnostics.xqproj"));
    if (Expect(context->Projects()->CreateProject(
                   QStringLiteral("Diagnostics"),
                   projectPath,
                   &errorMessage),
               "project create should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->ProjectSession()->Save(&errorMessage),
               "project save should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Diagnostics().size() == 3,
               "successful save should post one task diagnostic"))
    {
        delete context;
        return 1;
    }
    if (Expect(lastDiagnostic.contains(QStringLiteral("Save Project")) &&
                   lastDiagnostic.contains(QStringLiteral("succeeded")),
               "empty-message save should still post useful task diagnostic"))
    {
        delete context;
        return 1;
    }
    if (Expect(emittedDiagnostics == 3,
               "task diagnostics should emit through DiagnosticPosted"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
