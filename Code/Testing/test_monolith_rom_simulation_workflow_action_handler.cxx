#include "Infrastructure/xq_RomSimulationWorkflowActionHandler.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_RenderRefreshService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"

#include <xq_MitkGrid.h>
#include <xq_MitkROMJob.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ROMJob.h>
#include <xq_TetGenGrid.h>

#include <QCoreApplication>

#include <mitkDataNode.h>

#include <vtkCellArray.h>
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

xq::core::DataImportRequest MakeMeshImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("mesh-001");
    request.SourcePath = QStringLiteral("C:/studies/mesh-001.xqmesh");
    request.DisplayName = QStringLiteral("Main Mesh");
    request.Modality = QStringLiteral("Mesh");
    request.WorkflowRole = xq::core::DataWorkflowRole::Mesh;
    return request;
}

vtkSmartPointer<vtkUnstructuredGrid> MakeTetraVolumeMesh()
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
    return grid;
}

mitk::DataNode::Pointer MakeMeshNode()
{
    auto* tetGrid = new xq_TetGenGrid();
    tetGrid->SetVolumeMesh(MakeTetraVolumeMesh());

    auto mitkGrid = xq_MitkGrid::New();
    mitkGrid->SetMesh(tetGrid, 0);

    auto node = mitk::DataNode::New();
    node->SetName("Main Mesh");
    node->SetData(mitkGrid);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::VolumeMesh);
    xq::pipeline::SetStringProperty(
        node, xq::pipeline::kSourceModelProperty, "Main Model");
    return node;
}

mitk::DataNode::Pointer MakeConfiguredRomNode()
{
    auto job = std::make_unique<xq_ROMJob>();
    job->SetJobName("Existing ROM");
    job->SetModelType("1D");
    job->SetCapProp("inlet", "role", "inflow");
    job->SetCapProp("outlet", "role", "outflow");
    job->SetRCR("outlet", 100.0, 1.0e-5, 900.0);
    job->SetProperty("branch_count", "1");
    job->SetProperty("outlet_count", "1");
    job->AddOutputField("pressure");
    job->AddOutputField("flow");

    auto mitkJob = xq_MitkROMJob::New();
    mitkJob->SetROMJob(std::move(job));
    mitkJob->SetStatus("configured");

    auto node = mitk::DataNode::New();
    node->SetName("Existing ROM");
    node->SetData(mitkJob);
    xq::pipeline::MarkGeneratedNode(node,
                                    xq::pipeline::Stage::ROMSimulation,
                                    "build-1d-network",
                                    "test",
                                    "1");
    xq::pipeline::SetStringProperty(
        node, xq::pipeline::kSourceMeshProperty, "Main Mesh");
    xq::pipeline::SetStringProperty(node, "xq.rom.status", "configured");
    xq::pipeline::SetStringProperty(node, "xq.rom.model_type", "1D");
    return node;
}

xq::core::DataImportRequest MakeRomImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("rom-001");
    request.SourcePath = QStringLiteral("C:/studies/rom-001.xqrom");
    request.DisplayName = QStringLiteral("Existing ROM");
    request.Modality = QStringLiteral("ROMSimulation");
    request.WorkflowRole = xq::core::DataWorkflowRole::ROMSimulation;
    return request;
}

bool PrepareRomWorkflow(xq::core::ApplicationContext& context)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("rom-simulation")))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SelectOperation(
            QStringLiteral("rom-simulation"),
            QStringLiteral("build-1d-network"),
            &message))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SetParameterValue(
            QStringLiteral("rom-simulation"),
            QStringLiteral("build-1d-network"),
            QStringLiteral("branch-count"),
            1,
            &message))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SetParameterValue(
            QStringLiteral("rom-simulation"),
            QStringLiteral("build-1d-network"),
            QStringLiteral("outlet-count"),
            1,
            &message))
    {
        return false;
    }

    const auto importResult =
        context.DataImports()->Import(MakeMeshImport(), &message);
    return importResult.Succeeded;
}

bool PrepareExistingRomWorkflow(xq::core::ApplicationContext& context,
                                const QString& operationId)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("rom-simulation")))
    {
        return false;
    }
    const auto importResult =
        context.DataImports()->Import(MakeRomImport(), &message);
    if (!importResult.Succeeded)
        return false;

    return context.WorkflowOperations()->SelectOperation(
        QStringLiteral("rom-simulation"),
        operationId,
        &message);
}

bool PrepareRomWorkflow(xq::core::ApplicationContext& context,
                        const QString& operationId)
{
    if (!PrepareRomWorkflow(context))
        return false;

    QString message;
    return context.WorkflowOperations()->SelectOperation(
        QStringLiteral("rom-simulation"),
        operationId,
        &message);
}

bool PrepareHiddenRomSolverWorkflow(xq::core::ApplicationContext& context)
{
    if (!PrepareRomWorkflow(context))
        return false;

    QVector<xq::core::WorkflowOperationDescriptor> operations =
        context.WorkflowOperations()->OperationsForWorkflow(
            QStringLiteral("rom-simulation"));
    xq::core::WorkflowOperationDescriptor hiddenSolver;
    hiddenSolver.Id = QStringLiteral("run-rom-solver");
    hiddenSolver.Title = QStringLiteral("ROM Solver");
    operations.push_back(hiddenSolver);

    QString message;
    if (!context.WorkflowOperations()->RegisterOperations(
            QStringLiteral("rom-simulation"),
            operations,
            &message))
    {
        return false;
    }

    return context.WorkflowOperations()->SelectOperation(
        QStringLiteral("rom-simulation"),
        QStringLiteral("run-rom-solver"),
        &message);
}

class FakeRenderRefreshService : public xq::core::RenderRefreshService
{
public:
    int Calls = 0;
    mitk::DataStorage::Pointer LastDataStorage;

    void RefreshDataStorage(mitk::DataStorage::Pointer dataStorage) override
    {
        ++Calls;
        LastDataStorage = dataStorage;
    }
};

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicRomSimulationWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "dynamic ROM handler should register"))
        {
            return 1;
        }
        if (Expect(context->WorkflowActions()->HasHandler(
                       QStringLiteral("rom-simulation")),
                   "dynamic ROM handler should be discoverable"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareRomWorkflow(*context),
                   "missing mesh fixture should prepare ROM workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicRomSimulationWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "missing mesh fixture should install ROM handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "ROM handler should reject missing MITK mesh node"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Active mesh or simulation prep node is required for ROM network build."),
                   "missing ROM mesh diagnostic should be specific"))
        {
            return 1;
        }
        if (Expect(context->DataCatalog()->FindById(
                       QStringLiteral("mesh-001-build-1d-network")) ==
                       nullptr,
                   "failed ROM handler should not register result catalog"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareRomWorkflow(*context),
                   "valid ROM fixture should prepare workflow"))
        {
            return 1;
        }

        auto meshNode = MakeMeshNode();
        context->DataStorage()->Add(meshNode);
        context->DataNodes()->BindNode(QStringLiteral("mesh-001"), meshNode);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicRomSimulationWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "valid ROM fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "ROM handler should create network job"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered ROM network catalog entry."),
                   "ROM handler should report catalog commit success"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }

        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("mesh-001-build-1d-network"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::ROMSimulation &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/rom/mesh-001-build-1d-network"),
                   "ROM handler should register generated catalog entry"))
        {
            return 1;
        }
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-mesh-001-build-1d-network"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("mesh-001-build-1d-network"),
                   "ROM handler should register generated hierarchy entry"))
        {
            return 1;
        }

        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("mesh-001-build-1d-network"));
        auto* mitkJob = resultNode.IsNotNull()
                            ? dynamic_cast<xq_MitkROMJob*>(
                                  resultNode->GetData())
                            : nullptr;
        auto* job = mitkJob ? mitkJob->GetROMJob(0) : nullptr;
        std::string sourceMesh;
        std::string status;
        if (Expect(mitkJob != nullptr &&
                       job != nullptr &&
                       job->GetJobName() == "Main Mesh_rom" &&
                       job->GetModelType() == "1D" &&
                       job->Validate().empty() &&
                       resultNode->GetStringProperty(
                           xq::pipeline::kSourceMeshProperty,
                           sourceMesh) &&
                       sourceMesh == "Main Mesh" &&
                       resultNode->GetStringProperty(
                           "xq.rom.status",
                           status) &&
                       status == "configured" &&
                       xq::pipeline::HasStage(
                           resultNode,
                           xq::pipeline::Stage::ROMSimulation),
                   "ROM handler should bind generated ROM job"))
        {
            return 1;
        }
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("mesh-001-build-1d-network"),
                   "ROM handler should select generated ROM job"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "ROM handler should refresh rendering after success"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareHiddenRomSolverWorkflow(*context),
                   "unsupported ROM solver fixture should prepare workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicRomSimulationWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "unsupported ROM solver fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "unsupported ROM solver should fail"))
        {
            return 1;
        }
        if (Expect(message == QStringLiteral(
                                  "ROM Solver is not wired to a native ROM Simulation runtime yet."),
                   "unsupported ROM solver diagnostic should name operation"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareRomWorkflow(
                       *context,
                       QStringLiteral("calibrate-boundary-conditions")),
                   "missing ROM calibration fixture should prepare workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicRomSimulationWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "missing ROM calibration fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "ROM calibration should require selected ROM job"))
        {
            return 1;
        }
        if (Expect(message == QStringLiteral(
                                  "Active ROM job node is required for boundary calibration."),
                   "missing ROM calibration diagnostic should require ROM job"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareExistingRomWorkflow(
                       *context,
                       QStringLiteral("calibrate-boundary-conditions")),
                   "valid ROM calibration fixture should prepare workflow"))
        {
            return 1;
        }
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("rom-simulation"),
                       QStringLiteral("calibrate-boundary-conditions"),
                       QStringLiteral("target-flow-rate"),
                       72.5,
                       nullptr),
                   "ROM calibration target flow should be configurable"))
        {
            return 1;
        }
        if (Expect(context->WorkflowOperations()->SetParameterValue(
                       QStringLiteral("rom-simulation"),
                       QStringLiteral("calibrate-boundary-conditions"),
                       QStringLiteral("resistance-scale"),
                       1.5,
                       nullptr),
                   "ROM calibration resistance scale should be configurable"))
        {
            return 1;
        }

        auto romNode = MakeConfiguredRomNode();
        context->DataStorage()->Add(romNode);
        context->DataNodes()->BindNode(QStringLiteral("rom-001"), romNode);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicRomSimulationWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "valid ROM calibration fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "ROM calibration should create calibrated job"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered calibrated ROM boundary conditions catalog entry."),
                   "ROM calibration should report catalog commit success"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }

        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("rom-001-calibrate-boundary-conditions"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::ROMSimulation &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/rom/rom-001-calibrate-boundary-conditions"),
                   "ROM calibration should register generated catalog entry"))
        {
            return 1;
        }
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-rom-001-calibrate-boundary-conditions"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("rom-001-calibrate-boundary-conditions"),
                   "ROM calibration should register generated hierarchy entry"))
        {
            return 1;
        }

        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("rom-001-calibrate-boundary-conditions"));
        auto* mitkJob = resultNode.IsNotNull()
                            ? dynamic_cast<xq_MitkROMJob*>(
                                  resultNode->GetData())
                            : nullptr;
        auto* job = mitkJob ? mitkJob->GetROMJob(0) : nullptr;
        double rp = 0.0;
        double c = 0.0;
        double rd = 0.0;
        std::string sourceRom;
        std::string sourceMesh;
        std::string status;
        std::string solverState;
        std::string targetFlowRate;
        std::string scale;
        if (Expect(mitkJob != nullptr &&
                       job != nullptr &&
                       job->GetJobName() ==
                           "Existing ROM_calibrated_bc" &&
                       job->Validate().empty() &&
                       job->GetRCR("outlet", rp, c, rd) &&
                       rp == 150.0 &&
                       c == 1.0e-5 &&
                       rd == 1350.0 &&
                       job->GetProperty("calibration_target_flow_rate") ==
                           "72.500000" &&
                       job->GetProperty("calibration_resistance_scale") ==
                           "1.500000" &&
                       job->GetProperty("solver_state") ==
                           "not_solver_run" &&
                       resultNode->GetStringProperty("xq.source.rom",
                                                     sourceRom) &&
                       sourceRom == "Existing ROM" &&
                       resultNode->GetStringProperty(
                           xq::pipeline::kSourceMeshProperty,
                           sourceMesh) &&
                       sourceMesh == "Main Mesh" &&
                       resultNode->GetStringProperty("xq.rom.status",
                                                     status) &&
                       status == "calibrated" &&
                       resultNode->GetStringProperty(
                           "xq.rom.solver_state",
                           solverState) &&
                       solverState == "not_solver_run" &&
                       resultNode->GetStringProperty(
                           "xq.rom.calibration.target_flow_rate",
                           targetFlowRate) &&
                       targetFlowRate == "72.500000" &&
                       resultNode->GetStringProperty(
                           "xq.rom.calibration.resistance_scale",
                           scale) &&
                       scale == "1.500000" &&
                       xq::pipeline::HasStage(
                           resultNode,
                           xq::pipeline::Stage::ROMSimulation),
                   "ROM calibration should bind calibrated ROM job metadata"))
        {
            return 1;
        }
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral(
                           "rom-001-calibrate-boundary-conditions"),
                   "ROM calibration should select calibrated job"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "ROM calibration should refresh rendering after success"))
        {
            return 1;
        }
    }

    return 0;
}
