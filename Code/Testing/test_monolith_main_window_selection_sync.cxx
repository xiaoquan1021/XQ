#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataManagementService.h"
#include "Core/xq_DataSelectionService.h"
#include "Presentation/xq_DataHierarchyModel.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
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

QString CurrentNodeId(QTreeView* view)
{
    return view->currentIndex()
        .data(xq::presentation::DataHierarchyModel::NodeIdRole)
        .toString();
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    auto* hierarchyView =
        window.findChild<QTreeView*>(QStringLiteral("xqDataHierarchyView"));
    if (Expect(hierarchyView != nullptr,
               "MainWindow should expose the data hierarchy tree view"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto firstImport =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA A")),
                                       &errorMessage);
    if (Expect(firstImport.Succeeded, "first image import should succeed"))
    {
        delete context;
        return 1;
    }

    app.processEvents();

    if (Expect(CurrentNodeId(hierarchyView) ==
                   QStringLiteral("data-image-001"),
               "import should select the imported data row in the tree"))
    {
        delete context;
        return 1;
    }

    const auto secondImport =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-002"),
                                           QStringLiteral("CTA B")),
                                       &errorMessage);
    if (Expect(secondImport.Succeeded, "second image import should succeed"))
    {
        delete context;
        return 1;
    }

    app.processEvents();

    if (Expect(CurrentNodeId(hierarchyView) ==
                   QStringLiteral("data-image-002"),
               "second import should move tree selection to the new data row"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataSelection()->SelectCatalogEntry(
                   QStringLiteral("image-001"),
                   &errorMessage),
               "programmatic catalog selection should succeed"))
    {
        delete context;
        return 1;
    }

    app.processEvents();

    if (Expect(CurrentNodeId(hierarchyView) ==
                   QStringLiteral("data-image-001"),
               "programmatic selection should update the tree current row"))
    {
        delete context;
        return 1;
    }

    context->DataSelection()->Clear();
    app.processEvents();

    if (Expect(!hierarchyView->currentIndex().isValid(),
               "clearing data selection should clear the tree current index"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataSelection()->SelectCatalogEntry(
                   QStringLiteral("image-001"),
                   &errorMessage),
               "selection before remove should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(context->DataManagement()->RemoveEntry(
                   QStringLiteral("image-001"),
                   &errorMessage),
               "removing selected data should succeed"))
    {
        delete context;
        return 1;
    }
    app.processEvents();

    if (Expect(!hierarchyView->currentIndex().isValid(),
               "removing selected data should clear stale tree selection"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
