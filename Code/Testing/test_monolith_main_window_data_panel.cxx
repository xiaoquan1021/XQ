#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_WorkflowRegistry.h"
#include "Presentation/xq_DataHierarchyModel.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QModelIndex>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTreeView>

#include <mitkDataNode.h>

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

    auto* hierarchyView =
        window.findChild<QTreeView*>(QStringLiteral("xqDataHierarchyView"));
    auto* searchBox =
        window.findChild<QLineEdit*>(QStringLiteral("xqDataManagerSearchBox"));
    auto* opacitySlider =
        window.findChild<QSlider*>(QStringLiteral("xqDataOpacitySlider"));
    auto* opacityValue =
        window.findChild<QLabel*>(QStringLiteral("xqDataOpacityValueLabel"));
    auto* colorButton =
        window.findChild<QPushButton*>(QStringLiteral("xqDataColorButton"));
    auto* propertiesToggle =
        window.findChild<QPushButton*>(QStringLiteral("xqDataPropertiesToggle"));
    auto* propertiesTable =
        window.findChild<QTableWidget*>(QStringLiteral("xqDataPropertiesTable"));
    if (Expect(hierarchyView != nullptr,
               "MainWindow should expose the data hierarchy tree view"))
    {
        delete context;
        return 1;
    }
    if (Expect(searchBox != nullptr,
               "Data Manager should restore the original search box"))
    {
        delete context;
        return 1;
    }
    if (Expect(searchBox->placeholderText() ==
                   QStringLiteral("Search nodes..."),
               "Data Manager search box should keep the original placeholder"))
    {
        delete context;
        return 1;
    }
    if (Expect(opacitySlider != nullptr &&
                   opacityValue != nullptr &&
                   colorButton != nullptr,
               "Data Manager should restore opacity and color controls"))
    {
        delete context;
        return 1;
    }
    if (Expect(opacitySlider->minimum() == 0 &&
                   opacitySlider->maximum() == 100 &&
                   opacitySlider->value() == 100,
               "Data Manager opacity slider should use percentage range"))
    {
        delete context;
        return 1;
    }
    if (Expect(opacityValue->text() == QStringLiteral("100%"),
               "Data Manager opacity label should mirror slider value"))
    {
        delete context;
        return 1;
    }
    if (Expect(colorButton->text() == QStringLiteral("Color"),
               "Data Manager color button should keep the original label"))
    {
        delete context;
        return 1;
    }
    if (Expect(propertiesToggle != nullptr && propertiesTable != nullptr,
               "Data Manager should restore the properties panel controls"))
    {
        delete context;
        return 1;
    }
    if (Expect(!propertiesTable->isVisible(),
               "Data Manager properties table should start collapsed"))
    {
        delete context;
        return 1;
    }
    propertiesToggle->setChecked(true);
    app.processEvents();
    if (Expect(propertiesTable->isVisible(),
               "Data Manager properties toggle should show the table"))
    {
        delete context;
        return 1;
    }
    propertiesToggle->setChecked(false);
    app.processEvents();
    if (Expect(!propertiesTable->isVisible(),
               "Data Manager properties toggle should hide the table"))
    {
        delete context;
        return 1;
    }
    opacitySlider->setValue(35);
    app.processEvents();
    if (Expect(opacityValue->text() == QStringLiteral("35%"),
               "Data Manager opacity label should update with slider changes"))
    {
        delete context;
        return 1;
    }

    auto* hierarchyModel =
        qobject_cast<xq::presentation::DataHierarchyModel*>(
            hierarchyView->model());
    if (Expect(hierarchyModel != nullptr,
               "data hierarchy tree view should use DataHierarchyModel"))
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
    auto mitkNode = mitk::DataNode::New();
    mitkNode->SetName("CTA A node");
    mitkNode->SetFloatProperty("opacity", 0.42f);
    mitkNode->SetColor(0.25f, 0.5f, 0.75f);
    mitkNode->SetBoolProperty("visible", true);
    if (Expect(context->DataNodes()->BindNode(QStringLiteral("image-001"),
                                              mitkNode,
                                              &errorMessage),
               "test should bind imported catalog data to a MITK node"))
    {
        delete context;
        return 1;
    }

    app.processEvents();

    if (Expect(hierarchyModel->rowCount(QModelIndex()) == 1,
               "data hierarchy tree should refresh root rows after import"))
    {
        delete context;
        return 1;
    }

    const QModelIndex imagesIndex =
        hierarchyModel->index(0, 0, QModelIndex());
    if (Expect(imagesIndex.isValid(),
               "imported image folder should be visible in the tree"))
    {
        delete context;
        return 1;
    }
    if (Expect(hierarchyModel->data(imagesIndex, Qt::DisplayRole).toString() ==
                   QStringLiteral("Images"),
               "image folder should keep its display name in the tree"))
    {
        delete context;
        return 1;
    }

    const QModelIndex imageIndex =
        hierarchyModel->index(0, 0, imagesIndex);
    if (Expect(imageIndex.isValid(),
               "imported image row should be visible in the tree"))
    {
        delete context;
        return 1;
    }
    if (Expect(hierarchyModel->data(imageIndex, Qt::DisplayRole).toString() ==
                   QStringLiteral("CTA A"),
               "imported image row should expose its display name"))
    {
        delete context;
        return 1;
    }

    searchBox->setText(QStringLiteral("CTA"));
    app.processEvents();
    if (Expect(!hierarchyView->isRowHidden(0, imagesIndex),
               "Data Manager search should keep matching child rows visible"))
    {
        delete context;
        return 1;
    }
    searchBox->setText(QStringLiteral("no-match"));
    app.processEvents();
    if (Expect(hierarchyView->isRowHidden(0, imagesIndex),
               "Data Manager search should hide non-matching child rows"))
    {
        delete context;
        return 1;
    }
    searchBox->clear();
    app.processEvents();
    if (Expect(!hierarchyView->isRowHidden(0, imagesIndex),
               "Clearing Data Manager search should show child rows again"))
    {
        delete context;
        return 1;
    }

    hierarchyView->setCurrentIndex(imageIndex);
    app.processEvents();

    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   QStringLiteral("image-001"),
               "selecting a data row should update selected catalog entry"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedHierarchyNodeId() ==
                   QStringLiteral("data-image-001"),
               "selecting a data row should update selected hierarchy node"))
    {
        delete context;
        return 1;
    }
    if (Expect(opacitySlider->value() == 42 &&
                   opacityValue->text() == QStringLiteral("42%"),
               "selecting a MITK-backed data row should load node opacity"))
    {
        delete context;
        return 1;
    }
    if (Expect(colorButton->styleSheet().contains(QStringLiteral("#4080bf")),
               "selecting a MITK-backed data row should reflect node color"))
    {
        delete context;
        return 1;
    }
    if (!propertiesTable->isVisible())
    {
        propertiesToggle->setChecked(true);
        app.processEvents();
    }
    if (Expect(TableValue(propertiesTable, QStringLiteral("Name")) ==
                   QStringLiteral("CTA A node"),
               "Data Manager properties should include MITK node name"))
    {
        delete context;
        return 1;
    }
    if (Expect(TableValue(propertiesTable, QStringLiteral("Catalog Id")) ==
                   QStringLiteral("image-001"),
               "Data Manager properties should include catalog id"))
    {
        delete context;
        return 1;
    }
    if (Expect(TableValue(propertiesTable, QStringLiteral("Source")) ==
                   QStringLiteral("C:/studies/image-001"),
               "Data Manager properties should include catalog source path"))
    {
        delete context;
        return 1;
    }
    if (Expect(TableValue(propertiesTable, QStringLiteral("Role")) ==
                   QStringLiteral("Image"),
               "Data Manager properties should include workflow role"))
    {
        delete context;
        return 1;
    }
    if (Expect(TableValue(propertiesTable, QStringLiteral("Opacity")) ==
                   QStringLiteral("0.42"),
               "Data Manager properties should include node opacity"))
    {
        delete context;
        return 1;
    }

    opacitySlider->setValue(64);
    app.processEvents();
    float opacity = 0.0f;
    mitkNode->GetFloatProperty("opacity", opacity);
    if (Expect(opacity > 0.63f && opacity < 0.65f,
               "Data Manager opacity slider should write selected MITK node"))
    {
        delete context;
        return 1;
    }
    if (Expect(TableValue(propertiesTable, QStringLiteral("Opacity")) ==
                   QStringLiteral("0.64"),
               "Data Manager properties should refresh after opacity changes"))
    {
        delete context;
        return 1;
    }

    hierarchyView->setCurrentIndex(imagesIndex);
    app.processEvents();

    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   QStringLiteral("image-001"),
               "selecting a folder should not overwrite data selection"))
    {
        delete context;
        return 1;
    }

    auto* navigation =
        window.findChild<QListWidget*>(QStringLiteral("xqWorkflowNavigation"));
    auto* pages =
        window.findChild<QStackedWidget*>(QStringLiteral("xqWorkflowPages"));
    if (Expect(navigation != nullptr,
               "MainWindow should keep a discoverable workflow navigation"))
    {
        delete context;
        return 1;
    }
    if (Expect(pages != nullptr,
               "MainWindow should keep a discoverable workflow page stack"))
    {
        delete context;
        return 1;
    }
    if (Expect(navigation->count() ==
                   static_cast<int>(
                       xq::core::DefaultWorkflowRegistry().size()),
               "workflow navigation should still expose every workflow"))
    {
        delete context;
        return 1;
    }

    navigation->setCurrentRow(1);
    app.processEvents();

    if (Expect(pages->currentIndex() == 1,
               "workflow navigation should still switch stacked pages"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
