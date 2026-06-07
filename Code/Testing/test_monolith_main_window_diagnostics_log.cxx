#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QTextEdit>

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

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    auto* diagnosticsLog =
        window.findChild<QTextEdit*>(QStringLiteral("xqDiagnosticsLog"));
    if (Expect(diagnosticsLog != nullptr,
               "MainWindow should expose a diagnostics log"))
    {
        delete context;
        return 1;
    }
    if (Expect(diagnosticsLog->isReadOnly(),
               "diagnostics log should be read-only"))
    {
        delete context;
        return 1;
    }
    if (Expect(diagnosticsLog->toPlainText().isEmpty(),
               "diagnostics log should start empty"))
    {
        delete context;
        return 1;
    }

    context->PostDiagnostic(QString());
    context->PostDiagnostic(QStringLiteral("   "));
    app.processEvents();

    if (Expect(diagnosticsLog->toPlainText().isEmpty(),
               "empty diagnostics should remain absent from the log"))
    {
        delete context;
        return 1;
    }

    context->PostDiagnostic(QStringLiteral("Manual diagnostic"));
    app.processEvents();

    if (Expect(diagnosticsLog->toPlainText().contains(
                   QStringLiteral("Manual diagnostic")),
               "manual diagnostic should append to the visible log"))
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
    app.processEvents();

    const QString logText = diagnosticsLog->toPlainText();
    if (Expect(logText.contains(QStringLiteral("Import image-001")) &&
                   logText.contains(QStringLiteral("succeeded")),
               "task diagnostic should append to the visible log"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
