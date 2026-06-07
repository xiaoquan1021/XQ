#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataSelectionService.h"
#include "Presentation/xq_DataHierarchyModel.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
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

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    auto* removeAction =
        window.findChild<QAction*>(QStringLiteral("xqRemoveDataAction"));
    if (Expect(removeAction != nullptr,
               "MainWindow should expose a remove data action"))
    {
        delete context;
        return 1;
    }
    if (Expect(!removeAction->isEnabled(),
               "remove data action should start disabled"))
    {
        delete context;
        return 1;
    }

    int diagnostics = 0;
    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&diagnostics](const QString&) {
                         ++diagnostics;
                     });

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

    if (Expect(removeAction->isEnabled(),
               "remove data action should enable when data is selected"))
    {
        delete context;
        return 1;
    }

    removeAction->trigger();
    app.processEvents();

    if (Expect(context->DataCatalog()->Entries().isEmpty(),
               "triggering remove action should remove selected catalog data"))
    {
        delete context;
        return 1;
    }
    if (Expect(!context->DataSelection()->HasSelection(),
               "triggering remove action should clear data selection"))
    {
        delete context;
        return 1;
    }
    if (Expect(!removeAction->isEnabled(),
               "remove data action should disable after removal clears selection"))
    {
        delete context;
        return 1;
    }

    auto* hierarchyView =
        window.findChild<QTreeView*>(QStringLiteral("xqDataHierarchyView"));
    if (Expect(hierarchyView != nullptr,
               "MainWindow should still expose the data hierarchy tree"))
    {
        delete context;
        return 1;
    }
    auto* hierarchyModel =
        qobject_cast<xq::presentation::DataHierarchyModel*>(
            hierarchyView->model());
    if (Expect(hierarchyModel != nullptr,
               "data hierarchy tree should still use DataHierarchyModel"))
    {
        delete context;
        return 1;
    }
    const QModelIndex imagesIndex =
        hierarchyModel->index(0, 0, QModelIndex());
    if (Expect(hierarchyModel->rowCount(imagesIndex) == 0,
               "remove action should refresh the visible tree rows"))
    {
        delete context;
        return 1;
    }

    removeAction->setEnabled(true);
    removeAction->trigger();
    app.processEvents();

    if (Expect(diagnostics > 0,
               "forced stale remove action should post a diagnostic"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
