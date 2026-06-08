#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportCommand.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataSelectionService.h"
#include "Presentation/xq_DataHierarchyModel.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QModelIndex>
#include <QTreeView>

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

class TestDataImportCommand : public xq::core::DataImportCommand
{
public:
    explicit TestDataImportCommand(xq::core::ApplicationContext& context)
        : m_Context(context)
    {
    }

    xq::core::DataImportCommandResult RunImport(
        xq::core::ApplicationContext& context) override
    {
        ++Invocations;
        if (Expect(&context == &m_Context,
                   "import command should receive the window context"))
            return {};

        xq::core::DataImportRequest request;
        request.RequestedId = QStringLiteral("image-001");
        request.SourcePath = QStringLiteral("C:/studies/cta-a.nii.gz");
        request.DisplayName = QStringLiteral("CTA A");
        request.Modality = QStringLiteral("CT");
        request.WorkflowRole = xq::core::DataWorkflowRole::Image;

        QString message;
        const auto result = context.DataImports()->Import(request, &message);

        xq::core::DataImportCommandResult commandResult;
        commandResult.Succeeded = result.Succeeded;
        commandResult.CatalogEntryId = result.EntryId;
        commandResult.Message = message;
        return commandResult;
    }

    int Invocations = 0;

private:
    xq::core::ApplicationContext& m_Context;
};

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    auto* importAction =
        window.findChild<QAction*>(QStringLiteral("xqImportDataAction"));
    if (Expect(importAction != nullptr,
               "MainWindow should expose an import data action"))
    {
        delete context;
        return 1;
    }
    if (Expect(importAction->isEnabled(),
               "import data action should start enabled"))
    {
        delete context;
        return 1;
    }

    int diagnosticCount = 0;
    QString lastDiagnostic;
    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&diagnosticCount, &lastDiagnostic](const QString& message) {
                         ++diagnosticCount;
                         lastDiagnostic = message;
                     });

    importAction->trigger();
    app.processEvents();

    if (Expect(diagnosticCount == 1,
               "missing import command should post one diagnostic"))
    {
        delete context;
        return 1;
    }
    if (Expect(lastDiagnostic == QStringLiteral("No data import command is configured."),
               "missing import command should explain why import cannot run"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataCatalog()->Entries().isEmpty(),
               "missing import command should not mutate the data catalog"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataStorage()->GetAll()->empty(),
               "missing import command should not mutate MITK data storage"))
    {
        delete context;
        return 1;
    }

    TestDataImportCommand command(*context);
    window.SetDataImportCommand(&command);

    importAction->trigger();
    app.processEvents();

    if (Expect(command.Invocations == 1,
               "triggering import action should invoke the configured command"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataCatalog()->Entries().size() == 1,
               "successful import command should add one catalog entry"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   QStringLiteral("image-001"),
               "successful import command should leave imported data selected"))
    {
        delete context;
        return 1;
    }

    auto* removeAction =
        window.findChild<QAction*>(QStringLiteral("xqRemoveDataAction"));
    if (Expect(removeAction != nullptr,
               "MainWindow should still expose the remove data action"))
    {
        delete context;
        return 1;
    }
    if (Expect(removeAction->isEnabled(),
               "successful import should refresh data action enablement"))
    {
        delete context;
        return 1;
    }

    auto* selectionLabel =
        window.findChild<QLabel*>(QStringLiteral("xqDataPageSelection"));
    if (Expect(selectionLabel != nullptr,
               "MainWindow should expose the data page selection label"))
    {
        delete context;
        return 1;
    }
    if (Expect(selectionLabel->text() == QStringLiteral("Selected data: CTA A"),
               "successful import should refresh the data page selection label"))
    {
        delete context;
        return 1;
    }

    auto* hierarchyView =
        window.findChild<QTreeView*>(QStringLiteral("xqDataHierarchyView"));
    if (Expect(hierarchyView != nullptr,
               "MainWindow should expose the data hierarchy tree"))
    {
        delete context;
        return 1;
    }
    auto* hierarchyModel =
        qobject_cast<xq::presentation::DataHierarchyModel*>(
            hierarchyView->model());
    if (Expect(hierarchyModel != nullptr,
               "data hierarchy tree should use DataHierarchyModel"))
    {
        delete context;
        return 1;
    }
    const QModelIndex imagesIndex =
        hierarchyModel->index(0, 0, QModelIndex());
    if (Expect(imagesIndex.isValid(),
               "successful import should expose an Images folder"))
    {
        delete context;
        return 1;
    }
    const QModelIndex imageIndex =
        hierarchyModel->index(0, 0, imagesIndex);
    if (Expect(imageIndex.isValid(),
               "successful import should expose the imported image row"))
    {
        delete context;
        return 1;
    }
    if (Expect(hierarchyModel->data(imageIndex, Qt::DisplayRole).toString() ==
                   QStringLiteral("CTA A"),
               "imported image row should show the imported display name"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
