#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Infrastructure/xq_FlowSimulationWorkflowActionHandler.h"
#include "Infrastructure/xq_MultiPhysicsWorkflowActionHandler.h"
#include "Infrastructure/xq_RomSimulationWorkflowActionHandler.h"
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

QComboBox* FindOptionParameter(xq::presentation::MainWindow& window,
                               const QString& parameterId)
{
    return window.findChild<QComboBox*>(
        QStringLiteral("xqWorkflowParameter_%1").arg(parameterId));
}

xq::core::DataImportRequest MakeMeshImport(const QString& id,
                                           const QString& displayName)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = QStringLiteral("C:/studies/") + id;
    request.DisplayName = displayName;
    request.Modality = QStringLiteral("CFD");
    request.WorkflowRole = xq::core::DataWorkflowRole::Mesh;
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
    xq::infrastructure::RegisterDynamicFlowSimulationWorkflowActionHandler(
        *context,
        nullptr);
    xq::infrastructure::RegisterDynamicRomSimulationWorkflowActionHandler(
        *context,
        nullptr);
    xq::infrastructure::RegisterDynamicMultiPhysicsWorkflowActionHandler(
        *context,
        nullptr);
    xq::presentation::MainWindow window(*context);

    auto* flowSelector =
        FindSelector(window, QStringLiteral("flow-simulation"));
    if (Expect(flowSelector != nullptr,
               "Flow Simulation page should expose an operation selector"))
    {
        delete context;
        return 1;
    }
    if (Expect(flowSelector->count() == 3,
               "Flow Simulation selector should expose domain operations"))
    {
        delete context;
        return 1;
    }
    if (Expect(flowSelector->itemData(1).toString() ==
                       QStringLiteral("run-steady-flow") &&
                   flowSelector->itemText(1) ==
                       QStringLiteral("Steady Flow Solve"),
               "Flow Simulation selector should preserve operation order"))
    {
        delete context;
        return 1;
    }

    flowSelector->setCurrentIndex(
        flowSelector->findData(QStringLiteral("run-steady-flow")));
    app.processEvents();
    auto* flowButton =
        FindActionButton(window, QStringLiteral("flow-simulation"));
    if (Expect(flowButton != nullptr &&
                   flowButton->text() ==
                       QStringLiteral("Run Steady Flow Solve"),
               "Flow Simulation action should include selected operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window,
                                    QStringLiteral("convergence-tolerance")) !=
                       nullptr &&
                   FindIntegerParameter(window,
                                        QStringLiteral("max-iterations")) !=
                       nullptr,
               "Steady Flow Solve should expose flow parameters"))
    {
        delete context;
        return 1;
    }

    flowSelector->setCurrentIndex(
        flowSelector->findData(QStringLiteral("configure-cfd-job")));
    app.processEvents();
    auto* solverProfile =
        FindOptionParameter(window, QStringLiteral("solver-profile"));
    if (Expect(solverProfile != nullptr &&
                   solverProfile->count() == 3 &&
                   solverProfile->itemData(0).toString() ==
                       QStringLiteral("steady") &&
                   solverProfile->itemText(1) ==
                       QStringLiteral("Pulsatile"),
               "Configure CFD Job should expose ordered solver profile options"))
    {
        delete context;
        return 1;
    }
    solverProfile->setCurrentIndex(
        solverProfile->findData(QStringLiteral("transient")));
    app.processEvents();
    if (Expect(context->WorkflowOperations()
                   ->ParameterValues(QStringLiteral("flow-simulation"),
                                     QStringLiteral("configure-cfd-job"))
                   .value(QStringLiteral("solver-profile"))
                   .toString() == QStringLiteral("transient"),
               "solver profile option selector should update Core state"))
    {
        delete context;
        return 1;
    }

    flowSelector->setCurrentIndex(
        flowSelector->findData(QStringLiteral("run-steady-flow")));
    app.processEvents();

    QString errorMessage;
    const auto flowMesh =
        context->DataImports()->Import(MakeMeshImport(
                                           QStringLiteral("flow-mesh"),
                                           QStringLiteral("Aorta Mesh")),
                                       &errorMessage);
    if (Expect(flowMesh.Succeeded, "flow mesh import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("flow-simulation")),
               "Flow Simulation workflow should be selectable before run"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(flowButton->isEnabled(),
               "compatible mesh should enable Flow Simulation action"))
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
    flowButton->click();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Run Flow Simulation failed: Active simulation prep node is required for steady flow solve.")),
               "Flow Simulation action should report infrastructure validation"))
    {
        delete context;
        return 1;
    }

    auto* romSelector =
        FindSelector(window, QStringLiteral("rom-simulation"));
    if (Expect(romSelector != nullptr,
               "ROM Simulation page should expose an operation selector"))
    {
        delete context;
        return 1;
    }
    if (Expect(romSelector->count() == 3,
               "ROM Simulation selector should expose domain operations"))
    {
        delete context;
        return 1;
    }
    if (Expect(romSelector->itemData(1).toString() ==
                       QStringLiteral("run-rom-solver") &&
                   romSelector->itemText(1) ==
                       QStringLiteral("ROM Solver"),
               "ROM Simulation selector should preserve operation order"))
    {
        delete context;
        return 1;
    }

    romSelector->setCurrentIndex(
        romSelector->findData(QStringLiteral("run-rom-solver")));
    app.processEvents();
    auto* romButton =
        FindActionButton(window, QStringLiteral("rom-simulation"));
    if (Expect(romButton != nullptr &&
                   romButton->text() == QStringLiteral("Run ROM Solver"),
               "ROM Simulation action should include selected operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window,
                                    QStringLiteral("rom-time-step")) !=
                       nullptr &&
                   FindIntegerParameter(window,
                                        QStringLiteral("cardiac-cycles")) !=
                       nullptr,
               "ROM Solver should expose solver parameters"))
    {
        delete context;
        return 1;
    }

    const auto romMesh =
        context->DataImports()->Import(MakeMeshImport(
                                           QStringLiteral("rom-mesh"),
                                           QStringLiteral("ROM Mesh")),
                                       &errorMessage);
    if (Expect(romMesh.Succeeded, "ROM mesh import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("rom-simulation")),
               "ROM Simulation workflow should be selectable before run"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(romButton->isEnabled(),
               "compatible mesh should enable ROM Simulation action"))
    {
        delete context;
        return 1;
    }
    romButton->click();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Run ROM Simulation failed: ROM Solver is not wired to a native ROM Simulation runtime yet.")),
               "ROM Simulation action should report unsupported operation"))
    {
        delete context;
        return 1;
    }

    auto* multiphysicsSelector =
        FindSelector(window, QStringLiteral("multiphysics"));
    if (Expect(multiphysicsSelector != nullptr,
               "MultiPhysics page should expose an operation selector"))
    {
        delete context;
        return 1;
    }
    if (Expect(multiphysicsSelector->count() == 3,
               "MultiPhysics selector should expose domain operations"))
    {
        delete context;
        return 1;
    }
    if (Expect(multiphysicsSelector->itemData(1).toString() ==
                       QStringLiteral("run-coupled-solve") &&
                   multiphysicsSelector->itemText(1) ==
                       QStringLiteral("Coupled Solve"),
               "MultiPhysics selector should preserve operation order"))
    {
        delete context;
        return 1;
    }

    multiphysicsSelector->setCurrentIndex(
        multiphysicsSelector->findData(
            QStringLiteral("run-coupled-solve")));
    app.processEvents();
    auto* multiphysicsButton =
        FindActionButton(window, QStringLiteral("multiphysics"));
    if (Expect(multiphysicsButton != nullptr &&
                   multiphysicsButton->text() ==
                       QStringLiteral("Run Coupled Solve"),
               "MultiPhysics action should include selected operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindNumericParameter(window,
                                    QStringLiteral("coupled-time-step")) !=
                       nullptr &&
                   FindIntegerParameter(window,
                                        QStringLiteral("nonlinear-iterations")) !=
                       nullptr,
               "Coupled Solve should expose multiphysics parameters"))
    {
        delete context;
        return 1;
    }

    const auto multiphysicsMesh =
        context->DataImports()->Import(MakeMeshImport(
                                           QStringLiteral("multiphysics-mesh"),
                                           QStringLiteral("Coupled Mesh")),
                                       &errorMessage);
    if (Expect(multiphysicsMesh.Succeeded,
               "MultiPhysics mesh import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("multiphysics")),
               "MultiPhysics workflow should be selectable before run"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(multiphysicsButton->isEnabled(),
               "compatible mesh should enable MultiPhysics action"))
    {
        delete context;
        return 1;
    }
    multiphysicsButton->click();
    app.processEvents();
    if (Expect(diagnostics.contains(QStringLiteral(
                   "Run Multi-Physics failed: Coupled Solve is not wired to a native Multi-Physics runtime yet.")),
               "MultiPhysics action should report unsupported operation"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
