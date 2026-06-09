#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_PreferencesService.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QTableWidget>

#include <mitkDataNode.h>
#include <mitkProperties.h>

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

QString TableValue(QTableWidget* table, const QString& key)
{
    if (!table)
        return QString();

    for (int row = 0; row < table->rowCount(); ++row)
    {
        auto* keyItem = table->item(row, 0);
        auto* valueItem = table->item(row, 1);
        if (keyItem && valueItem && keyItem->text() == key)
            return valueItem->text();
    }

    return QString();
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    window.show();
    app.processEvents();

    auto* volumeRenderingAction =
        window.findChild<QAction*>(QStringLiteral("xqVolumeRenderingAction"));
    auto* crosshairAction =
        window.findChild<QAction*>(QStringLiteral("xqCrosshairAction"));
    auto* propertiesTable =
        window.findChild<QTableWidget*>(QStringLiteral("xqDataPropertiesTable"));
    if (Expect(volumeRenderingAction != nullptr &&
                   volumeRenderingAction->isCheckable(),
               "Volume Rendering action should exist and be checkable"))
    {
        delete context;
        return 1;
    }
    if (Expect(crosshairAction != nullptr &&
                   crosshairAction->isCheckable() &&
                   crosshairAction->isChecked(),
               "Crosshair action should exist and start checked"))
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

    auto node = mitk::DataNode::New();
    node->SetName("CTA A");
    node->SetBoolProperty("volumerendering", false);
    node->SetProperty("material.representation", mitk::IntProperty::New(2));
    context->DataStorage()->Add(node);
    if (Expect(context->DataNodes()->BindNode(QStringLiteral("image-001"),
                                              node,
                                              &errorMessage),
               "test should bind selected image data to a MITK node"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectCatalogEntry(
                   QStringLiteral("image-001"),
                   &errorMessage),
               "test should select imported image data"))
    {
        delete context;
        return 1;
    }
    propertiesTable->show();
    app.processEvents();

    volumeRenderingAction->trigger();
    app.processEvents();

    bool volumeRendering = false;
    node->GetBoolProperty("volumerendering", volumeRendering);
    if (Expect(volumeRendering,
               "Volume Rendering action should enable selected node volume rendering"))
    {
        delete context;
        return 1;
    }
    if (Expect(TableValue(propertiesTable,
                          QStringLiteral("volumerendering")) ==
                   QStringLiteral("true"),
               "Data Manager properties should refresh after Volume Rendering"))
    {
        delete context;
        return 1;
    }

    volumeRenderingAction->trigger();
    app.processEvents();
    volumeRendering = true;
    node->GetBoolProperty("volumerendering", volumeRendering);
    if (Expect(!volumeRendering,
               "Volume Rendering action should disable selected node volume rendering"))
    {
        delete context;
        return 1;
    }

    crosshairAction->trigger();
    app.processEvents();
    if (Expect(!context->Preferences()->BoolValue(
                   QStringLiteral("view.crosshair.enabled"),
                   true),
               "Crosshair action should persist disabled state"))
    {
        delete context;
        return 1;
    }
    crosshairAction->trigger();
    app.processEvents();
    if (Expect(context->Preferences()->BoolValue(
                   QStringLiteral("view.crosshair.enabled"),
                   false),
               "Crosshair action should persist enabled state"))
    {
        delete context;
        return 1;
    }

    for (const auto& message : diagnostics)
    {
        if (Expect(message != QStringLiteral(
                       "Volume Rendering is not available in Windows monolith v1.") &&
                       message != QStringLiteral(
                           "Crosshair toggle is not available in Windows monolith v1."),
                   "Migrated View actions should not post unavailable diagnostics"))
        {
            delete context;
            return 1;
        }
    }

    delete context;
    return 0;
}
