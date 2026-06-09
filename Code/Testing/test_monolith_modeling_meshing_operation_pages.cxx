#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Infrastructure/xq_MeshingWorkflowActionHandler.h"
#include "Infrastructure/xq_ModelingWorkflowActionHandler.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>

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

    modelingSelector->setCurrentIndex(
        modelingSelector->findData(QStringLiteral("trim-branches")));
    app.processEvents();
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
                   "Run Modeling failed: Trim Branches is not wired to a native Modeling runtime yet.")),
               "Modeling action should report unsupported operation"))
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

    meshingSelector->setCurrentIndex(
        meshingSelector->findData(QStringLiteral("boundary-layers")));
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
