#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Infrastructure/xq_MeshingWorkflowActionHandler.h"
#include "Infrastructure/xq_ModelingWorkflowActionHandler.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>

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

QComboBox* FindSelector(xq::presentation::MainWindow& window,
                        const QString& workflowId)
{
    return window.findChild<QComboBox*>(
        QStringLiteral("xqWorkflowOperationSelector_%1").arg(workflowId));
}

QPushButton* FindActionButton(xq::presentation::MainWindow& window,
                              const QString& workflowId)
{
    return window.findChild<QPushButton*>(
        QStringLiteral("xqWorkflowPrimaryAction_%1").arg(workflowId));
}

QDoubleSpinBox* FindNumericParameter(xq::presentation::MainWindow& window,
                                     const QString& parameterId)
{
    return window.findChild<QDoubleSpinBox*>(
        QStringLiteral("xqWorkflowParameter_%1").arg(parameterId));
}

QSpinBox* FindIntegerParameter(xq::presentation::MainWindow& window,
                               const QString& parameterId)
{
    return window.findChild<QSpinBox*>(
        QStringLiteral("xqWorkflowParameter_%1").arg(parameterId));
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
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context->WorkflowActions(),
        context->WorkflowOperations());
    xq::infrastructure::RegisterDynamicModelingWorkflowActionHandler(*context,
                                                                     nullptr);
    xq::infrastructure::RegisterDynamicMeshingWorkflowActionHandler(*context,
                                                                    nullptr);
    xq::presentation::MainWindow window(*context);

    auto* modelingSelector =
        FindSelector(window, QStringLiteral("modeling"));
    if (Expect(modelingSelector != nullptr,
               "Modeling page should expose an operation selector"))
    {
        delete context;
        return 1;
    }
    if (Expect(modelingSelector->count() == 3,
               "Modeling selector should expose domain operations"))
    {
        delete context;
        return 1;
    }
    if (Expect(modelingSelector->itemData(1).toString() ==
                       QStringLiteral("build-solid-model") &&
                   modelingSelector->itemText(1) ==
                       QStringLiteral("Build Solid Model"),
               "Modeling selector should preserve operation order"))
    {
        delete context;
        return 1;
    }
    auto* modelLabel =
        window.findChild<QLabel*>(QStringLiteral("xqModelingModelLabel"));
    auto* modelCombo =
        window.findChild<QComboBox*>(
            QStringLiteral("xqModelingModelSelector"));
    auto* facesTable =
        window.findChild<QTableWidget*>(
            QStringLiteral("xqModelingFacesTable"));
    auto* operationTabs =
        window.findChild<QTabWidget*>(
            QStringLiteral("xqModelingOperationTabs"));
    auto* loftSurfaceButton =
        window.findChild<QPushButton*>(
            QStringLiteral("xqModelingLoftSurfaceButton"));
    auto* buildSolidButton =
        window.findChild<QPushButton*>(
            QStringLiteral("xqModelingBuildSolidModelButton"));
    auto* trimBranchesButton =
        window.findChild<QPushButton*>(
            QStringLiteral("xqModelingTrimBranchesButton"));
    if (Expect(modelLabel != nullptr &&
                   modelLabel->text() == QStringLiteral("Model:") &&
                   modelCombo != nullptr,
               "Modeling page should restore legacy model selector row"))
    {
        delete context;
        return 1;
    }
    if (Expect(facesTable != nullptr &&
                   facesTable->selectionMode() ==
                       QAbstractItemView::SingleSelection,
               "Modeling page should restore legacy faces table anchor"))
    {
        delete context;
        return 1;
    }
    if (Expect(operationTabs != nullptr &&
                   operationTabs->count() == 3 &&
                   operationTabs->tabText(0) == QStringLiteral("Create") &&
                   operationTabs->tabText(1) == QStringLiteral("Edit") &&
                   operationTabs->tabText(2) == QStringLiteral("Export"),
               "Modeling page should restore legacy operation tabs"))
    {
        delete context;
        return 1;
    }
    if (Expect(loftSurfaceButton != nullptr &&
                   buildSolidButton != nullptr &&
                   trimBranchesButton != nullptr,
               "Modeling page should restore legacy operation buttons"))
    {
        delete context;
        return 1;
    }
    if (Expect(loftSurfaceButton->isCheckable() &&
                   buildSolidButton->isCheckable() &&
                   trimBranchesButton->isCheckable(),
               "Modeling operation buttons should be checkable"))
    {
        delete context;
        return 1;
    }

    modelingSelector->setCurrentIndex(
        modelingSelector->findData(QStringLiteral("trim-branches")));
    app.processEvents();
    if (Expect(trimBranchesButton->isChecked(),
               "Modeling trim button should mirror selector changes"))
    {
        delete context;
        return 1;
    }
    auto* modelingButton =
        FindActionButton(window, QStringLiteral("modeling"));
    if (Expect(modelingButton != nullptr &&
                   modelingButton->text() ==
                       QStringLiteral("Run Trim Branches"),
               "Modeling action should include selected operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window,
                                    QStringLiteral("trim-distance")) !=
                   nullptr,
               "Trim Branches should expose modeling parameters"))
    {
        delete context;
        return 1;
    }
    buildSolidButton->click();
    app.processEvents();
    if (Expect(context->WorkflowOperations()->SelectedOperationId(
                   QStringLiteral("modeling")) ==
                   QStringLiteral("build-solid-model"),
               "Modeling Build Solid button should update Core operation state"))
    {
        delete context;
        return 1;
    }
    if (Expect(modelingSelector->currentData().toString() ==
                   QStringLiteral("build-solid-model") &&
                   buildSolidButton->isChecked(),
               "Modeling selector and tool button should stay synchronized"))
    {
        delete context;
        return 1;
    }
    trimBranchesButton->click();
    app.processEvents();

    QString errorMessage;
    const auto segmentationImport =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("modeling-seg"),
                                           QStringLiteral("Aorta Segmentation"),
                                           xq::core::DataWorkflowRole::Segmentation),
                                       &errorMessage);
    if (Expect(segmentationImport.Succeeded,
               "modeling segmentation import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("modeling")),
               "Modeling workflow should be selectable before run"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(modelingButton->isEnabled(),
               "compatible segmentation should enable Modeling action"))
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
    modelingButton->click();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Run Modeling failed: Active segmentation node is required for modeling.")),
               "Modeling action should report native trim validation"))
    {
        delete context;
        return 1;
    }

    auto* meshingSelector =
        FindSelector(window, QStringLiteral("meshing"));
    if (Expect(meshingSelector != nullptr,
               "Meshing page should expose an operation selector"))
    {
        delete context;
        return 1;
    }
    if (Expect(meshingSelector->count() == 3,
               "Meshing selector should expose domain operations"))
    {
        delete context;
        return 1;
    }
    if (Expect(meshingSelector->itemData(1).toString() ==
                       QStringLiteral("generate-volume-mesh") &&
                   meshingSelector->itemText(1) ==
                       QStringLiteral("Generate Volume Mesh"),
               "Meshing selector should preserve operation order"))
    {
        delete context;
        return 1;
    }
    auto* meshModelLabel =
        window.findChild<QLabel*>(QStringLiteral("xqMeshingModelLabel"));
    auto* meshModelCombo =
        window.findChild<QComboBox*>(
            QStringLiteral("xqMeshingModelSelector"));
    auto* newMeshButton =
        window.findChild<QPushButton*>(
            QStringLiteral("xqMeshingNewMeshButton"));
    auto* meshingTabs =
        window.findChild<QTabWidget*>(QStringLiteral("xqMeshingTabs"));
    auto* globalGroup =
        window.findChild<QGroupBox*>(
            QStringLiteral("xqMeshingGlobalParamsGroup"));
    auto* meshTypeCombo =
        window.findChild<QComboBox*>(
            QStringLiteral("xqMeshingMeshTypeCombo"));
    auto* globalEdgeSpin =
        window.findChild<QDoubleSpinBox*>(
            QStringLiteral("xqMeshingGlobalEdgeSizeSpinBox"));
    auto* localSizeTable =
        window.findChild<QTableWidget*>(
            QStringLiteral("xqMeshingLocalSizeTable"));
    auto* boundaryLayerCheckBox =
        window.findChild<QCheckBox*>(
            QStringLiteral("xqMeshingBoundaryLayerCheckBox"));
    auto* boundaryLayerGroup =
        window.findChild<QGroupBox*>(
            QStringLiteral("xqMeshingBoundaryLayerParamsGroup"));
    auto* boundaryLayerPreviewButton =
        window.findChild<QPushButton*>(
            QStringLiteral("xqMeshingBoundaryLayerPreviewButton"));
    auto* refinementTable =
        window.findChild<QTableWidget*>(
            QStringLiteral("xqMeshingRefinementRegionsTable"));
    auto* statisticsGroup =
        window.findChild<QGroupBox*>(
            QStringLiteral("xqMeshingStatisticsGroup"));
    auto* surfaceMeshButton =
        window.findChild<QPushButton*>(
            QStringLiteral("xqMeshingGenerateSurfaceMeshButton"));
    auto* volumeMeshButton =
        window.findChild<QPushButton*>(
            QStringLiteral("xqMeshingGenerateVolumeMeshButton"));
    auto* boundaryLayersButton =
        window.findChild<QPushButton*>(
            QStringLiteral("xqMeshingBoundaryLayersButton"));
    if (Expect(meshModelLabel != nullptr &&
                   meshModelLabel->text() == QStringLiteral("Model:") &&
                   meshModelCombo != nullptr &&
                   newMeshButton != nullptr &&
                   newMeshButton->text() == QStringLiteral("New Mesh..."),
               "Meshing page should restore legacy model selector row"))
    {
        delete context;
        return 1;
    }
    if (Expect(meshingTabs != nullptr &&
                   meshingTabs->count() == 5 &&
                   meshingTabs->tabText(0) ==
                       QStringLiteral("Global Settings") &&
                   meshingTabs->tabText(1) == QStringLiteral("Local Size") &&
                   meshingTabs->tabText(2) ==
                       QStringLiteral("Boundary Layer") &&
                   meshingTabs->tabText(3) ==
                       QStringLiteral("Refinement Regions") &&
                   meshingTabs->tabText(4) == QStringLiteral("Advanced"),
               "Meshing page should restore legacy meshing tabs"))
    {
        delete context;
        return 1;
    }
    if (Expect(globalGroup != nullptr &&
                   globalGroup->title() ==
                       QStringLiteral("Global Mesh Parameters") &&
                   meshTypeCombo != nullptr &&
                   meshTypeCombo->itemText(0) == QStringLiteral("TetGen") &&
                   globalEdgeSpin != nullptr &&
                   globalEdgeSpin->value() == 1.0,
               "Meshing global tab should expose TetGen parameters"))
    {
        delete context;
        return 1;
    }
    if (Expect(localSizeTable != nullptr &&
                   localSizeTable->columnCount() == 3 &&
                   localSizeTable->horizontalHeaderItem(0)->text() ==
                       QStringLiteral("Face Name") &&
                   localSizeTable->horizontalHeaderItem(2)->text() ==
                       QStringLiteral("Edge Size"),
               "Meshing page should restore local-size table anchor"))
    {
        delete context;
        return 1;
    }
    if (Expect(boundaryLayerCheckBox != nullptr &&
                   boundaryLayerCheckBox->text() ==
                       QStringLiteral("Enable Boundary Layer Mesh") &&
                   boundaryLayerGroup != nullptr &&
                   !boundaryLayerGroup->isEnabled() &&
                   boundaryLayerPreviewButton != nullptr &&
                   !boundaryLayerPreviewButton->isEnabled(),
               "Meshing boundary layer tab should start disabled"))
    {
        delete context;
        return 1;
    }
    boundaryLayerCheckBox->setChecked(true);
    app.processEvents();
    if (Expect(boundaryLayerGroup->isEnabled() &&
                   boundaryLayerPreviewButton->isEnabled(),
               "Meshing boundary layer toggle should enable parameters"))
    {
        delete context;
        return 1;
    }
    if (Expect(refinementTable != nullptr &&
                   refinementTable->columnCount() == 7 &&
                   refinementTable->horizontalHeaderItem(0)->text() ==
                       QStringLiteral("Name") &&
                   refinementTable->horizontalHeaderItem(6)->text() ==
                       QStringLiteral("Target Size"),
               "Meshing page should restore refinement-region table anchor"))
    {
        delete context;
        return 1;
    }
    if (Expect(statisticsGroup != nullptr &&
                   statisticsGroup->title() ==
                       QStringLiteral("Mesh Statistics"),
               "Meshing advanced tab should restore statistics group"))
    {
        delete context;
        return 1;
    }
    if (Expect(surfaceMeshButton != nullptr &&
                   volumeMeshButton != nullptr &&
                   boundaryLayersButton != nullptr &&
                   surfaceMeshButton->isCheckable() &&
                   volumeMeshButton->isCheckable() &&
                   boundaryLayersButton->isCheckable(),
               "Meshing page should restore operation buttons"))
    {
        delete context;
        return 1;
    }

    meshingSelector->setCurrentIndex(
        meshingSelector->findData(QStringLiteral("boundary-layers")));
    app.processEvents();
    if (Expect(boundaryLayersButton->isChecked(),
               "Meshing boundary layer button should mirror selector changes"))
    {
        delete context;
        return 1;
    }
    volumeMeshButton->click();
    app.processEvents();
    if (Expect(context->WorkflowOperations()->SelectedOperationId(
                   QStringLiteral("meshing")) ==
                   QStringLiteral("generate-volume-mesh"),
               "Meshing Volume Mesh button should update Core operation state"))
    {
        delete context;
        return 1;
    }
    if (Expect(meshingSelector->currentData().toString() ==
                   QStringLiteral("generate-volume-mesh") &&
                   volumeMeshButton->isChecked(),
               "Meshing selector and tool button should stay synchronized"))
    {
        delete context;
        return 1;
    }
    boundaryLayersButton->click();
    app.processEvents();
    auto* meshingButton =
        FindActionButton(window, QStringLiteral("meshing"));
    if (Expect(meshingButton != nullptr &&
                   meshingButton->text() ==
                       QStringLiteral("Run Boundary Layers"),
               "Meshing action should include selected operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window,
                                    QStringLiteral("growth-rate")) !=
                       nullptr &&
                   FindIntegerParameter(window,
                                        QStringLiteral("layer-count")) !=
                       nullptr,
               "Boundary Layers should expose meshing parameters"))
    {
        delete context;
        return 1;
    }

    const auto modelImport =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("meshing-model"),
                                           QStringLiteral("Aorta Model"),
                                           xq::core::DataWorkflowRole::Model),
                                       &errorMessage);
    if (Expect(modelImport.Succeeded,
               "meshing model import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("meshing")),
               "Meshing workflow should be selectable before run"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(meshingButton->isEnabled(),
               "compatible model should enable Meshing action"))
    {
        delete context;
        return 1;
    }
    meshingButton->click();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Run Meshing failed: Active model node is required for meshing.")),
               "Meshing action should report infrastructure validation"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
