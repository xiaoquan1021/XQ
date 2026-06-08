#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataManagementService.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QLabel>
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

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    auto* dataPage =
        window.findChild<QWidget*>(QStringLiteral("xqWorkflowPage_data"));
    if (Expect(dataPage != nullptr, "data workflow page should exist"))
    {
        delete context;
        return 1;
    }

    auto* selectionLabel =
        dataPage->findChild<QLabel*>(QStringLiteral("xqDataPageSelection"));
    auto* catalogIdLabel =
        dataPage->findChild<QLabel*>(QStringLiteral("xqDataPageCatalogId"));
    auto* displayNameLabel =
        dataPage->findChild<QLabel*>(QStringLiteral("xqDataPageDisplayName"));
    auto* sourcePathLabel =
        dataPage->findChild<QLabel*>(QStringLiteral("xqDataPageSourcePath"));
    auto* workflowRoleLabel =
        dataPage->findChild<QLabel*>(QStringLiteral("xqDataPageWorkflowRole"));

    if (Expect(selectionLabel != nullptr,
               "data page should expose a selection label"))
    {
        delete context;
        return 1;
    }
    if (Expect(catalogIdLabel != nullptr,
               "data page should expose a catalog id label"))
    {
        delete context;
        return 1;
    }
    if (Expect(displayNameLabel != nullptr,
               "data page should expose a display name label"))
    {
        delete context;
        return 1;
    }
    if (Expect(sourcePathLabel != nullptr,
               "data page should expose a source path label"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowRoleLabel != nullptr,
               "data page should expose a workflow role label"))
    {
        delete context;
        return 1;
    }

    if (Expect(selectionLabel->text() == QStringLiteral("No data selected"),
               "data page should start without selected data"))
    {
        delete context;
        return 1;
    }
    if (Expect(catalogIdLabel->text().isEmpty() &&
                   displayNameLabel->text().isEmpty() &&
                   sourcePathLabel->text().isEmpty() &&
                   workflowRoleLabel->text().isEmpty(),
               "data page should clear metadata without selection"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto importResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA A")),
                                       &errorMessage);
    if (Expect(importResult.Succeeded, "image import should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(selectionLabel->text() ==
                   QStringLiteral("Selected data: CTA A"),
               "data import should update selected data text"))
    {
        delete context;
        return 1;
    }
    if (Expect(catalogIdLabel->text() == QStringLiteral("ID: image-001"),
               "data import should update catalog id text"))
    {
        delete context;
        return 1;
    }
    if (Expect(displayNameLabel->text() == QStringLiteral("Name: CTA A"),
               "data import should update display name text"))
    {
        delete context;
        return 1;
    }
    if (Expect(sourcePathLabel->text() ==
                   QStringLiteral("Source: C:/studies/image-001"),
               "data import should update source path text"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowRoleLabel->text() == QStringLiteral("Role: Image"),
               "data import should update workflow role text"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataManagement()->RenameEntry(
                   QStringLiteral("image-001"),
                   QStringLiteral("Renamed CTA"),
                   &errorMessage),
               "data rename should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(selectionLabel->text() ==
                   QStringLiteral("Selected data: Renamed CTA") &&
                   displayNameLabel->text() ==
                       QStringLiteral("Name: Renamed CTA"),
               "data rename should refresh selected data labels"))
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

    if (Expect(selectionLabel->text() == QStringLiteral("No data selected"),
               "data remove should clear selected data text"))
    {
        delete context;
        return 1;
    }
    if (Expect(catalogIdLabel->text().isEmpty() &&
                   displayNameLabel->text().isEmpty() &&
                   sourcePathLabel->text().isEmpty() &&
                   workflowRoleLabel->text().isEmpty(),
               "data remove should clear metadata labels"))
    {
        delete context;
        return 1;
    }

    const auto modelImport =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("model-001"),
                                           QStringLiteral("Aorta Model"),
                                           xq::core::DataWorkflowRole::Model),
                                       &errorMessage);
    if (Expect(modelImport.Succeeded, "model import should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(workflowRoleLabel->text() == QStringLiteral("Role: Model"),
               "model import should update workflow role text"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
