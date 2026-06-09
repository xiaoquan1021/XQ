#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Infrastructure/xq_FlowSimulationWorkflowActionHandler.h"
#include "Infrastructure/xq_MultiPhysicsWorkflowActionHandler.h"
#include "Infrastructure/xq_RomSimulationWorkflowActionHandler.h"
#include "Presentation/xq_MainWindow.h"

#include <xq_MitkROMJob.h>
#include <xq_MitkGrid.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ROMJob.h>
#include <xq_TetGenGrid.h>

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>

#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkTetra.h>
#include <vtkUnstructuredGrid.h>

#include <iostream>
#include <memory>

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

xq::core::DataImportRequest MakeRomImport(const QString& id,
                                          const QString& displayName)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = QStringLiteral("C:/studies/") + id;
    request.DisplayName = displayName;
    request.Modality = QStringLiteral("ROMSimulation");
    request.WorkflowRole = xq::core::DataWorkflowRole::ROMSimulation;
    return request;
}

xq::core::DataImportRequest MakeResultImport(const QString& id,
                                             const QString& displayName)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = QStringLiteral("C:/studies/") + id;
    request.DisplayName = displayName;
    request.Modality = QStringLiteral("SimulationResult");
    request.WorkflowRole = xq::core::DataWorkflowRole::SimulationResult;
    return request;
}

mitk::DataNode::Pointer MakeRomNode(const std::string& name)
{
    auto job = std::make_unique<xq_ROMJob>();
    job->SetJobName(name);
    job->SetModelType("1D");
    job->SetCapProp("inlet", "role", "inflow");
    job->SetCapProp("outlet", "role", "outflow");
    job->SetRCR("outlet", 100.0, 1.0e-5, 900.0);

    auto mitkJob = xq_MitkROMJob::New();
    mitkJob->SetROMJob(std::move(job));
    mitkJob->SetStatus("configured");

    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(mitkJob);
    xq::pipeline::MarkGeneratedNode(node,
                                    xq::pipeline::Stage::ROMSimulation,
                                    "build-1d-network",
                                    "test",
                                    "1");
    xq::pipeline::SetStringProperty(
        node, xq::pipeline::kSourceMeshProperty, "ROM Mesh");
    xq::pipeline::SetStringProperty(node, "xq.rom.status", "configured");
    return node;
}

mitk::DataNode::Pointer MakeResultNode(const std::string& name)
{
    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->InsertNextPoint(0.0, 0.0, 0.0);
    points->InsertNextPoint(1.0, 0.0, 0.0);
    points->InsertNextPoint(0.0, 1.0, 0.0);
    points->InsertNextPoint(0.0, 0.0, 1.0);
    grid->SetPoints(points);

    auto tetra = vtkSmartPointer<vtkTetra>::New();
    tetra->GetPointIds()->SetId(0, 0);
    tetra->GetPointIds()->SetId(1, 1);
    tetra->GetPointIds()->SetId(2, 2);
    tetra->GetPointIds()->SetId(3, 3);

    auto cells = vtkSmartPointer<vtkCellArray>::New();
    cells->InsertNextCell(tetra);
    grid->SetCells(VTK_TETRA, cells);

    auto pressure = vtkSmartPointer<vtkDoubleArray>::New();
    pressure->SetName("coupled_pressure");
    pressure->SetNumberOfComponents(1);
    pressure->SetNumberOfTuples(4);
    for (vtkIdType i = 0; i < 4; ++i)
        pressure->SetValue(i, 80.0 + static_cast<double>(i));
    grid->GetPointData()->AddArray(pressure);

    auto* tetGrid = new xq_TetGenGrid();
    tetGrid->SetVolumeMesh(grid);

    auto mitkGrid = xq_MitkGrid::New();
    mitkGrid->SetMesh(tetGrid, 0);

    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(mitkGrid);
    xq::pipeline::MarkGeneratedNode(node,
                                    xq::pipeline::Stage::Result,
                                    "coupled_result_import",
                                    "test",
                                    "1");
    node->SetStringProperty("xq.type", "result");
    xq::pipeline::SetStringProperty(
        node, "xq.result.field_names", "point:coupled_pressure");
    xq::pipeline::SetStringProperty(node,
                                    "xq.result.field_name",
                                    "point:coupled_pressure");
    node->SetIntProperty("xq.result.field_count", 1);
    return node;
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

    romSelector->setCurrentIndex(
        romSelector->findData(
            QStringLiteral("calibrate-boundary-conditions")));
    app.processEvents();
    if (Expect(romButton != nullptr &&
                   romButton->text() ==
                       QStringLiteral("Run Calibrate Boundary Conditions"),
               "ROM Simulation action should include calibration operation"))
    {
        delete context;
        return 1;
    }
    auto* targetFlowRate =
        FindNumericParameter(window, QStringLiteral("target-flow-rate"));
    auto* resistanceScale =
        FindNumericParameter(window, QStringLiteral("resistance-scale"));
    if (Expect(targetFlowRate != nullptr && resistanceScale != nullptr,
               "ROM calibration should expose calibration parameters"))
    {
        delete context;
        return 1;
    }
    targetFlowRate->setValue(72.5);
    resistanceScale->setValue(1.5);
    app.processEvents();

    const auto existingRom =
        context->DataImports()->Import(MakeRomImport(
                                           QStringLiteral("existing-rom"),
                                           QStringLiteral("Existing ROM")),
                                       &errorMessage);
    if (Expect(existingRom.Succeeded, "ROM job import should succeed"))
    {
        delete context;
        return 1;
    }
    auto romJobNode = MakeRomNode("Existing ROM");
    context->DataStorage()->Add(romJobNode);
    context->DataNodes()->BindNode(QStringLiteral("existing-rom"),
                                   romJobNode);
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("rom-simulation")),
               "ROM Simulation workflow should be selectable before calibration"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    romButton->click();
    app.processEvents();
    if (Expect(context->DataCatalog()->FindById(
                   QStringLiteral(
                       "existing-rom-calibrate-boundary-conditions")) !=
                   nullptr,
               "ROM Simulation calibration should register calibrated entry"))
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

    multiphysicsSelector->setCurrentIndex(
        multiphysicsSelector->findData(
            QStringLiteral("review-coupled-results")));
    app.processEvents();
    if (Expect(multiphysicsButton != nullptr &&
                   multiphysicsButton->text() ==
                       QStringLiteral("Run Review Coupled Results"),
               "MultiPhysics action should include review operation"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindIntegerParameter(window,
                                    QStringLiteral("sample-count")) !=
                   nullptr,
               "Coupled review should expose sample count parameter"))
    {
        delete context;
        return 1;
    }
    const auto coupledResult =
        context->DataImports()->Import(MakeResultImport(
                                           QStringLiteral("coupled-result"),
                                           QStringLiteral("Coupled Result")),
                                       &errorMessage);
    if (Expect(coupledResult.Succeeded,
               "MultiPhysics result import should succeed"))
    {
        delete context;
        return 1;
    }
    auto coupledResultNode = MakeResultNode("Coupled Result");
    context->DataStorage()->Add(coupledResultNode);
    context->DataNodes()->BindNode(QStringLiteral("coupled-result"),
                                   coupledResultNode);
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("multiphysics")),
               "MultiPhysics workflow should be selectable before review"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    multiphysicsButton->click();
    app.processEvents();
    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   QStringLiteral("coupled-result"),
               "MultiPhysics review should preserve result selection"))
    {
        delete context;
        return 1;
    }
    std::string reviewStatus;
    if (Expect(coupledResultNode->GetStringProperty(
                   "xq.review.multiphysics.status",
                   reviewStatus) &&
                   reviewStatus == "ready",
               "MultiPhysics review should store review status"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
