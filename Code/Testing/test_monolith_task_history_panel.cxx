#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
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

xq::core::DataImportRequest MakeImport(const QString& id,
                                       const QString& displayName,
                                       xq::core::DataWorkflowRole role)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = QStringLiteral("C:/studies/") + id;
    request.DisplayName = displayName;
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = role;
    return request;
}

QString CellText(QTableWidget* table, int row, int column)
{
    auto* item = table->item(row, column);
    return item ? item->text() : QString();
}

QPushButton* FindWorkflowAction(xq::presentation::MainWindow& window,
                                const QString& workflowId)
{
    return window.findChild<QPushButton*>(
        QStringLiteral("xqWorkflowPrimaryAction_%1").arg(workflowId));
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    auto* taskTable =
        window.findChild<QTableWidget*>(QStringLiteral("xqTaskHistoryTable"));
    if (Expect(taskTable != nullptr,
               "MainWindow should expose a task history table"))
    {
        delete context;
        return 1;
    }
    if (Expect(taskTable->columnCount() == 3,
               "task history table should expose three columns"))
    {
        delete context;
        return 1;
    }
    if (Expect(taskTable->horizontalHeaderItem(0)->text() ==
                   QStringLiteral("Task"),
               "task history first column should be Task"))
    {
        delete context;
        return 1;
    }
    if (Expect(taskTable->horizontalHeaderItem(1)->text() ==
                   QStringLiteral("Status"),
               "task history second column should be Status"))
    {
        delete context;
        return 1;
    }
    if (Expect(taskTable->horizontalHeaderItem(2)->text() ==
                   QStringLiteral("Message"),
               "task history third column should be Message"))
    {
        delete context;
        return 1;
    }
    if (Expect(taskTable->editTriggers() == QAbstractItemView::NoEditTriggers,
               "task history table should be read-only"))
    {
        delete context;
        return 1;
    }
    if (Expect(taskTable->rowCount() == 0,
               "task history table should start empty"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto imageImport =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA Image"),
                                           xq::core::DataWorkflowRole::Image),
                                       &errorMessage);
    if (Expect(imageImport.Succeeded, "image import should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(taskTable->rowCount() == 1,
               "successful import should append one task history row"))
    {
        delete context;
        return 1;
    }
    if (Expect(CellText(taskTable, 0, 0) ==
                   QStringLiteral("Import CTA Image"),
               "import row should show task name"))
    {
        delete context;
        return 1;
    }
    if (Expect(CellText(taskTable, 0, 1) == QStringLiteral("Succeeded"),
               "import row should show succeeded status"))
    {
        delete context;
        return 1;
    }
    if (Expect(CellText(taskTable, 0, 2) ==
                   QStringLiteral("Imported data catalog entry."),
               "import row should show task message"))
    {
        delete context;
        return 1;
    }

    const auto duplicateImport =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA Image"),
                                           xq::core::DataWorkflowRole::Image),
                                       &errorMessage);
    if (Expect(!duplicateImport.Succeeded,
               "duplicate image import should fail"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(taskTable->rowCount() == 2,
               "failed duplicate import should append one task history row"))
    {
        delete context;
        return 1;
    }
    if (Expect(CellText(taskTable, 1, 0) ==
                   QStringLiteral("Import CTA Image"),
               "duplicate import row should show task name"))
    {
        delete context;
        return 1;
    }
    if (Expect(CellText(taskTable, 1, 1) == QStringLiteral("Failed"),
               "duplicate import row should show failed status"))
    {
        delete context;
        return 1;
    }
    if (Expect(CellText(taskTable, 1, 2) ==
                   QStringLiteral("Duplicate hierarchy node id."),
               "duplicate import row should show failure message"))
    {
        delete context;
        return 1;
    }

    auto* diagnostics =
        window.findChild<QTextEdit*>(QStringLiteral("xqDiagnosticsLog"));
    if (Expect(diagnostics != nullptr,
               "MainWindow should still expose diagnostics log"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "image preprocessing workflow should be selectable"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    auto* imageAction =
        FindWorkflowAction(window, QStringLiteral("image-preprocessing"));
    if (Expect(imageAction != nullptr && imageAction->isEnabled(),
               "image preprocessing action should be enabled for image data"))
    {
        delete context;
        return 1;
    }
    imageAction->click();
    app.processEvents();

    if (Expect(taskTable->rowCount() == 3,
               "workflow action should append one task history row"))
    {
        delete context;
        return 1;
    }
    if (Expect(CellText(taskTable, 2, 0) ==
                   QStringLiteral("Run Image Preprocessing"),
               "workflow action row should show task name"))
    {
        delete context;
        return 1;
    }
    if (Expect(CellText(taskTable, 2, 1) == QStringLiteral("Succeeded"),
               "workflow action row should show succeeded status"))
    {
        delete context;
        return 1;
    }
    if (Expect(CellText(taskTable, 2, 2) ==
                   QStringLiteral("Image Preprocessing action requested for CTA Image."),
               "workflow action row should show task message"))
    {
        delete context;
        return 1;
    }
    if (Expect(diagnostics->toPlainText().contains(QStringLiteral(
                   "Run Image Preprocessing succeeded: Image Preprocessing action requested for CTA Image.")),
               "diagnostics log should still receive task bridge diagnostic"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
