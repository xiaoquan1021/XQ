#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportCommand.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataSelectionService.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>

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

class TestDicomImportCommand : public xq::core::DataImportCommand
{
public:
    xq::core::DataImportCommandResult RunImport(
        xq::core::ApplicationContext& context) override
    {
        ++Invocations;

        xq::core::DataImportRequest request;
        request.RequestedId = QStringLiteral("dicom-series-001");
        request.SourcePath = QStringLiteral("C:/studies/dicom");
        request.DisplayName = QStringLiteral("CTA DICOM Series");
        request.Modality = QStringLiteral("CT");
        request.WorkflowRole = xq::core::DataWorkflowRole::DICOMSeries;

        QString message;
        const auto result = context.DataImports()->Import(request, &message);

        xq::core::DataImportCommandResult commandResult;
        commandResult.Succeeded = result.Succeeded;
        commandResult.CatalogEntryId = result.EntryId;
        commandResult.Message = message;
        return commandResult;
    }

    int Invocations = 0;
};

QAction* FindAction(xq::presentation::MainWindow& window,
                    const QString& objectName)
{
    return window.findChild<QAction*>(objectName);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    TestDicomImportCommand command;
    window.SetDicomImportCommand(&command);

    auto* importDicomAction =
        FindAction(window, QStringLiteral("xqImportDicomAction"));
    if (Expect(importDicomAction != nullptr,
               "Import DICOM action should exist"))
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

    importDicomAction->trigger();
    app.processEvents();

    if (Expect(command.Invocations == 1,
               "Import DICOM action should invoke the configured DICOM command"))
    {
        delete context;
        return 1;
    }
    const auto* entry =
        context->DataCatalog()->FindById(QStringLiteral("dicom-series-001"));
    if (Expect(entry != nullptr &&
                   entry->WorkflowRole ==
                       xq::core::DataWorkflowRole::DICOMSeries,
               "Import DICOM should register a DICOMSeries catalog entry"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   QStringLiteral("dicom-series-001"),
               "Import DICOM should select the imported series"))
    {
        delete context;
        return 1;
    }
    if (Expect(!diagnostics.contains(QStringLiteral(
                   "Import DICOM is not available in Windows monolith v1.")),
               "Import DICOM should no longer report unavailable"))
    {
        delete context;
        return 1;
    }
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Imported data catalog entry.")),
               "Import DICOM should report successful import"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
