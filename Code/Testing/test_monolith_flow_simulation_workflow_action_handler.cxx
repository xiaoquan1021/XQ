#include "Infrastructure/xq_FlowSimulationWorkflowActionHandler.h"

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

#include <xq_Grid.h>
#include <xq_MeshPipeline.h>
#include <xq_MitkGrid.h>
#include <xq_MitkSolverJob.h>
#include <xq_TetGenGrid.h>
#include <xq_Model.h>
#include <xq_PipelineDataUtils.h>
#include <xq_PolyGeometry.h>
#include <xq_SimulationPrepPipeline.h>
#include <xq_SolverJob.h>

#include <QCoreApplication>

#include <mitkDataNode.h>

#include <vtkCellData.h>
#include <vtkCellArray.h>
#include <vtkIntArray.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkTetra.h>
#include <vtkTriangle.h>
#include <vtkUnstructuredGrid.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

vtkSmartPointer<vtkPolyData> MakeSpherePolyData()
{
    auto sphereSource = vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetRadius(5.0);
    sphereSource->SetThetaResolution(16);
    sphereSource->SetPhiResolution(16);
    sphereSource->Update();
    return sphereSource->GetOutput();
}

vtkSmartPointer<vtkPoints> MakeTetraPoints()
{
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->InsertNextPoint(0.0, 0.0, 0.0);
    points->InsertNextPoint(1.0, 0.0, 0.0);
    points->InsertNextPoint(0.0, 1.0, 0.0);
    points->InsertNextPoint(0.0, 0.0, 1.0);
    return points;
}

vtkSmartPointer<vtkPolyData> MakeTetraSurfacePolyData()
{
    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(MakeTetraPoints());

    auto cells = vtkSmartPointer<vtkCellArray>::New();
    const int triangles[4][3] = {
        {0, 2, 1},
        {0, 1, 3},
        {1, 2, 3},
        {2, 0, 3},
    };
    for (const auto& ids : triangles)
    {
        auto triangle = vtkSmartPointer<vtkTriangle>::New();
        triangle->GetPointIds()->SetId(0, ids[0]);
        triangle->GetPointIds()->SetId(1, ids[1]);
        triangle->GetPointIds()->SetId(2, ids[2]);
        cells->InsertNextCell(triangle);
    }
    polyData->SetPolys(cells);

    auto faceIds = vtkSmartPointer<vtkIntArray>::New();
    faceIds->SetNumberOfComponents(1);
    faceIds->SetNumberOfTuples(4);
    for (vtkIdType i = 0; i < 4; ++i)
        faceIds->SetValue(i, static_cast<int>(i));
    faceIds->SetName("FaceIds");
    polyData->GetCellData()->AddArray(faceIds);
    return polyData;
}

vtkSmartPointer<vtkUnstructuredGrid> MakeTetraVolumeMesh()
{
    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->SetPoints(MakeTetraPoints());

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

mitk::DataNode::Pointer MakeModelNode(const std::string& name)
{
    auto* geometry = new xq_PolyGeometry();
    geometry->SetWholeVtkPolyData(MakeSpherePolyData());

    auto faceIds = vtkSmartPointer<vtkIntArray>::New();
    faceIds->SetNumberOfComponents(1);
    faceIds->SetNumberOfTuples(
        geometry->GetWholeVtkPolyData()->GetNumberOfCells());
    for (vtkIdType i = 0; i < faceIds->GetNumberOfTuples(); ++i)
        faceIds->SetValue(i, 0);
    geometry->AssignFaceIds(faceIds);

    FaceInfo wallFace;
    wallFace.id = 0;
    wallFace.name = "wall";
    wallFace.type = "wall";
    geometry->SetFaceInfo(0, wallFace);

    auto model = xq_Model::New();
    model->SetModelElement(geometry, 0);

    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(model);
    node->SetBoolProperty("xq.model.qa.ok", true);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Model);
    return node;
}

mitk::DataNode::Pointer MakeSimpleFlowModelNode(const std::string& name)
{
    auto* geometry = new xq_PolyGeometry();
    geometry->SetWholeVtkPolyData(MakeTetraSurfacePolyData());

    FaceInfo wallFaceA;
    wallFaceA.id = 0;
    wallFaceA.name = "wall_a";
    wallFaceA.type = "wall";
    geometry->SetFaceInfo(wallFaceA.id, wallFaceA);

    FaceInfo inletFace;
    inletFace.id = 1;
    inletFace.name = "inlet";
    inletFace.type = "inlet";
    geometry->SetFaceInfo(inletFace.id, inletFace);

    FaceInfo outletFace;
    outletFace.id = 2;
    outletFace.name = "outlet";
    outletFace.type = "outlet";
    geometry->SetFaceInfo(outletFace.id, outletFace);

    FaceInfo wallFaceB;
    wallFaceB.id = 3;
    wallFaceB.name = "wall_b";
    wallFaceB.type = "wall";
    geometry->SetFaceInfo(wallFaceB.id, wallFaceB);

    auto model = xq_Model::New();
    model->SetModelElement(geometry, 0);

    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(model);
    node->SetBoolProperty("xq.model.qa.ok", true);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Model);
    return node;
}

mitk::DataNode::Pointer MakeSimpleFlowMeshNode(
    const std::string& name,
    const std::string& modelName)
{
    auto* tetGrid = new xq_TetGenGrid();
    tetGrid->SetVolumeMesh(MakeTetraVolumeMesh());
    tetGrid->SetSurfaceMesh(MakeTetraSurfacePolyData());

    auto mitkGrid = xq_MitkGrid::New();
    mitkGrid->SetMesh(tetGrid, 0);

    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(mitkGrid);
    node->SetBoolProperty("xq.mesh.qa.ok", true);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::VolumeMesh);
    xq::pipeline::SetStringProperty(
        node,
        xq::pipeline::kSourceModelProperty,
        modelName);
    return node;
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

bool PrepareFlowOperation(xq::core::ApplicationContext& context,
                          const QString& operationId)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("flow-simulation")))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SelectOperation(
            QStringLiteral("flow-simulation"),
            operationId,
            &message))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SetParameterValue(
            QStringLiteral("flow-simulation"),
            QStringLiteral("configure-cfd-job"),
            QStringLiteral("solver-profile"),
            QStringLiteral("steady"),
            &message))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SetParameterValue(
            QStringLiteral("flow-simulation"),
            QStringLiteral("configure-cfd-job"),
            QStringLiteral("inlet-count"),
            0,
            &message))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SetParameterValue(
            QStringLiteral("flow-simulation"),
            QStringLiteral("configure-cfd-job"),
            QStringLiteral("outlet-count"),
            0,
            &message))
    {
        return false;
    }

    return true;
}

bool PrepareFlowWorkflow(xq::core::ApplicationContext& context,
                         const QString& operationId =
                             QStringLiteral("configure-cfd-job"))
{
    if (!PrepareFlowOperation(context, operationId))
        return false;

    QString message;
    const auto importResult =
        context.DataImports()->Import(MakeMeshImport(), &message);
    return importResult.Succeeded;
}

bool RegisterSimulationPrepFixture(xq::core::ApplicationContext& context,
                                   QString* message)
{
    const QString modelEntryId = QStringLiteral("model-001");
    const QString meshEntryId = QStringLiteral("mesh-001");
    const QString simEntryId = QStringLiteral("sim-001");

    xq::core::DataCatalogEntry modelEntry;
    modelEntry.Id = modelEntryId;
    modelEntry.DisplayName = QStringLiteral("Flow Model");
    modelEntry.SourcePath = QStringLiteral("C:/studies/model-001.xqmodel");
    modelEntry.Modality = QStringLiteral("Model");
    modelEntry.WorkflowRole = xq::core::DataWorkflowRole::Model;
    if (!context.DataCatalog()->RegisterEntry(modelEntry, message))
        return false;

    xq::core::DataCatalogEntry meshEntry;
    meshEntry.Id = meshEntryId;
    meshEntry.DisplayName = QStringLiteral("Flow Mesh");
    meshEntry.SourcePath = QStringLiteral("C:/studies/mesh-001.xqmesh");
    meshEntry.Modality = QStringLiteral("Mesh");
    meshEntry.WorkflowRole = xq::core::DataWorkflowRole::Mesh;
    if (!context.DataCatalog()->RegisterEntry(meshEntry, message))
        return false;

    if (!context.DataHierarchy()->AddFolder(
            QStringLiteral("models"),
            context.DataHierarchy()->RootId(),
            QStringLiteral("Models"),
            message))
    {
        return false;
    }
    if (!context.DataHierarchy()->AddDataEntry(
            QStringLiteral("data-model-001"),
            QStringLiteral("models"),
            modelEntryId,
            modelEntry.DisplayName,
            message))
    {
        return false;
    }
    if (!context.DataHierarchy()->AddFolder(
            QStringLiteral("meshes"),
            context.DataHierarchy()->RootId(),
            QStringLiteral("Meshes"),
            message))
    {
        return false;
    }
    if (!context.DataHierarchy()->AddDataEntry(
            QStringLiteral("data-mesh-001"),
            QStringLiteral("meshes"),
            meshEntryId,
            meshEntry.DisplayName,
            message))
    {
        return false;
    }

    auto modelNode = MakeSimpleFlowModelNode("Flow Model");
    auto meshNode = MakeSimpleFlowMeshNode("Flow Mesh", "Flow Model");
    context.DataStorage()->Add(modelNode);
    context.DataStorage()->Add(meshNode, modelNode);
    if (!context.DataNodes()->BindNode(modelEntryId, modelNode, message))
        return false;
    if (!context.DataNodes()->BindNode(meshEntryId, meshNode, message))
        return false;

    xq_BoundaryCondition inlet;
    inlet.faceName = "inlet";
    inlet.faceRole = "inflow";
    inlet.bcType = "prescribed_velocity";
    inlet.parameters["value"] = "1.25";

    xq_BoundaryCondition outlet;
    outlet.faceName = "outlet";
    outlet.faceRole = "outflow";
    outlet.bcType = "resistance";
    outlet.parameters["value"] = "1200.0";

    xq_BoundaryCondition wallA;
    wallA.faceName = "wall_a";
    wallA.faceRole = "wall";
    wallA.bcType = "no_slip";

    xq_BoundaryCondition wallB;
    wallB.faceName = "wall_b";
    wallB.faceRole = "wall";
    wallB.bcType = "no_slip";

    xq_SimulationPrepRequest request;
    request.jobName = "Flow Mesh_cfd";
    request.solverType = "xq_simple_flow";
    request.boundaryConditions = {inlet, outlet, wallA, wallB};
    request.faceRoleOverrides = {
        {"inlet", "inflow"},
        {"outlet", "outflow"},
        {"wall_a", "wall"},
        {"wall_b", "wall"},
    };

    const auto simResult =
        xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep(
            context.DataStorage().GetPointer(),
            modelNode,
            meshNode,
            request);
    if (!simResult.ok || simResult.node.IsNull())
    {
        if (message)
        {
            *message = simResult.diagnostics.empty()
                           ? QStringLiteral("Simulation prep fixture failed.")
                           : QString::fromStdString(
                                 simResult.diagnostics.front().message);
        }
        return false;
    }

    xq::core::DataCatalogEntry simEntry;
    simEntry.Id = simEntryId;
    simEntry.DisplayName = QStringLiteral("Flow Mesh_cfd");
    simEntry.SourcePath = QStringLiteral("xq://generated/simulation-prep/sim-001");
    simEntry.Modality = QStringLiteral("SimulationPrep");
    simEntry.WorkflowRole = xq::core::DataWorkflowRole::SimulationPrep;
    if (!context.DataCatalog()->RegisterEntry(simEntry, message))
        return false;

    if (!context.DataHierarchy()->FindNode(QStringLiteral("simulations")) &&
        !context.DataHierarchy()->AddFolder(
            QStringLiteral("simulations"),
            context.DataHierarchy()->RootId(),
            QStringLiteral("Simulations"),
            message))
    {
        return false;
    }
    if (!context.DataHierarchy()->AddDataEntry(
            QStringLiteral("data-sim-001"),
            QStringLiteral("simulations"),
            simEntryId,
            simEntry.DisplayName,
            message))
    {
        return false;
    }
    if (!context.DataNodes()->BindNode(simEntryId, simResult.node, message))
        return false;

    return context.DataSelection()->SelectCatalogEntry(simEntryId, message);
}

std::filesystem::path SolverCaseDir()
{
    return std::filesystem::temp_directory_path() /
           "xq_monolith_run_steady_flow_case";
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
                    RegisterDynamicFlowSimulationWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "dynamic flow simulation handler should register"))
        {
            return 1;
        }
        if (Expect(context->WorkflowActions()->HasHandler(
                       QStringLiteral("flow-simulation")),
                   "dynamic flow simulation handler should be discoverable"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareFlowWorkflow(*context),
                   "missing mesh fixture should prepare flow workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicFlowSimulationWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "missing mesh fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "flow handler should reject missing mesh node"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Active mesh node is required for flow simulation."),
                   "missing mesh diagnostic should be specific"))
        {
            return 1;
        }
        if (Expect(context->DataCatalog()->FindById(
                       QStringLiteral("mesh-001-configure-cfd-job")) ==
                       nullptr,
                   "failed flow handler should not register result catalog"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareFlowWorkflow(*context),
                   "valid flow fixture should prepare workflow"))
        {
            return 1;
        }

        auto modelNode = MakeModelNode("Main Model");
        context->DataStorage()->Add(modelNode);

        xq_MeshGenerationRequest meshRequest;
        meshRequest.meshName = "Main Mesh";
        meshRequest.globalEdgeSize = 2.0;
        const auto meshResult =
            xq_MeshPipelineService::CreateVolumeMesh(
                context->DataStorage().GetPointer(),
                modelNode,
                meshRequest);
        if (Expect(meshResult.ok && meshResult.node.IsNotNull(),
                   "valid flow fixture should create mesh node"))
        {
            return 1;
        }
        context->DataNodes()->BindNode(QStringLiteral("mesh-001"),
                                       meshResult.node);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicFlowSimulationWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "valid flow fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "flow handler should create simulation prep job"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered simulation prep catalog entry."),
                   "flow handler should report catalog commit success"))
        {
            return 1;
        }
        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("mesh-001-configure-cfd-job"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::SimulationPrep &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/simulation-prep/mesh-001-configure-cfd-job"),
                   "flow handler should register generated catalog entry"))
        {
            return 1;
        }
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-mesh-001-configure-cfd-job"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("mesh-001-configure-cfd-job"),
                   "flow handler should register generated hierarchy entry"))
        {
            return 1;
        }
        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("mesh-001-configure-cfd-job"));
        auto* mitkJob = resultNode.IsNotNull()
                            ? dynamic_cast<xq_MitkSolverJob*>(
                                  resultNode->GetData())
                            : nullptr;
        auto* solverJob = mitkJob ? mitkJob->GetSimJob(0) : nullptr;
        if (Expect(mitkJob != nullptr &&
                       solverJob != nullptr &&
                       solverJob->GetJobName() == "Main Mesh_cfd" &&
                       solverJob->GetSolverType() == "xq_simple_flow" &&
                       xq::pipeline::HasStage(
                           resultNode,
                           xq::pipeline::Stage::SimulationPrep),
                   "flow handler should bind generated simulation prep job"))
        {
            return 1;
        }
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("mesh-001-configure-cfd-job"),
                   "flow handler should select generated simulation prep"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "flow handler should refresh rendering after success"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareFlowWorkflow(
                       *context,
                       QStringLiteral("run-steady-flow")),
                   "steady run missing sim fixture should prepare workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicFlowSimulationWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "steady run missing sim fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "steady run handler should reject missing simulation prep node"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Active simulation prep node is required for steady flow solve."),
                   "missing simulation prep diagnostic should be specific"))
        {
            return 1;
        }
        if (Expect(context->DataCatalog()->FindById(
                       QStringLiteral("mesh-001-run-steady-flow-result-1")) ==
                       nullptr,
                   "failed steady run should not register result catalog"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareFlowOperation(
                       *context,
                       QStringLiteral("run-steady-flow")),
                   "valid steady run fixture should prepare workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(RegisterSimulationPrepFixture(*context, &message),
                   "valid steady run fixture should register simulation prep"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }

        std::filesystem::remove_all(SolverCaseDir());
        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicFlowSimulationWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "valid steady run fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "steady run handler should run solver and import results"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered steady flow result catalog entries."),
                   "steady run should report result catalog commit success"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }

        const auto* resultEntry = context->DataCatalog()->FindById(
            QStringLiteral("sim-001-run-steady-flow-result-1"));
        if (Expect(resultEntry != nullptr &&
                       resultEntry->WorkflowRole ==
                           xq::core::DataWorkflowRole::SimulationResult &&
                       resultEntry->SourcePath.endsWith(
                           QStringLiteral("xq_simple_flow_result_0001.vtu")),
                   "steady run should register imported result catalog entry"))
        {
            return 1;
        }
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-sim-001-run-steady-flow-result-1"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("sim-001-run-steady-flow-result-1"),
                   "steady run should register imported result hierarchy entry"))
        {
            return 1;
        }

        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("sim-001-run-steady-flow-result-1"));
        std::string backendId;
        std::string fields;
        int fieldCount = 0;
        if (Expect(resultNode.IsNotNull() &&
                       xq::pipeline::HasStage(
                           resultNode,
                           xq::pipeline::Stage::Result) &&
                       resultNode->GetStringProperty(
                           "xq.result.backend_id",
                           backendId) &&
                       backendId == "xq_simple_flow" &&
                       resultNode->GetStringProperty(
                           "xq.result.field_names",
                           fields) &&
                       fields.find("point:pressure") != std::string::npos &&
                       fields.find("point:velocity") != std::string::npos &&
                       fields.find("cell:wall_shear") != std::string::npos &&
                       resultNode->GetIntProperty(
                           "xq.result.field_count",
                           fieldCount) &&
                       fieldCount >= 3,
                   "steady run should bind imported result node metadata"))
        {
            return 1;
        }

        auto simNode = context->DataNodes()->FindNode(
            QStringLiteral("sim-001"));
        std::string solverStatus;
        std::string importStatus;
        if (Expect(simNode.IsNotNull() &&
                       simNode->GetStringProperty(
                           "xq.solver.status",
                           solverStatus) &&
                       solverStatus == "completed_with_warnings" &&
                       simNode->GetStringProperty(
                           "xq.solver.result_import_status",
                           importStatus) &&
                       importStatus == "imported",
                   "steady run should update simulation prep status"))
        {
            return 1;
        }

        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("sim-001-run-steady-flow-result-1"),
                   "steady run should select imported result"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "steady run should refresh rendering after success"))
        {
            return 1;
        }

        if (Expect(context->WorkflowOperations()->SelectOperation(
                       QStringLiteral("flow-simulation"),
                       QStringLiteral("review-flow-results"),
                       &message),
                   "review fixture should select review operation"))
        {
            return 1;
        }
        FakeRenderRefreshService reviewRefresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicFlowSimulationWorkflowActionHandler(
                        *context,
                        &reviewRefresh,
                        &message),
                "review fixture should reinstall handler"))
        {
            return 1;
        }
        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "review handler should activate imported result display"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Prepared flow result review for point:pressure."),
                   "review handler should report selected scalar"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }

        bool visible = false;
        bool scalarVisibility = false;
        std::string activeScalar;
        std::string reviewScalar;
        std::string reviewStatus;
        if (Expect(resultNode->GetBoolProperty("visible", visible) &&
                       visible &&
                       resultNode->GetBoolProperty("scalar visibility",
                                                   scalarVisibility) &&
                       scalarVisibility &&
                       resultNode->GetStringProperty(
                           "xq.result.active_scalar",
                           activeScalar) &&
                       activeScalar == "pressure" &&
                       resultNode->GetStringProperty(
                           "xq.review.flow.active_scalar",
                           reviewScalar) &&
                       reviewScalar == "point:pressure" &&
                       resultNode->GetStringProperty(
                           "xq.review.flow.status",
                           reviewStatus) &&
                       reviewStatus == "ready",
                   "review handler should store scalar review metadata"))
        {
            return 1;
        }
        if (Expect(reviewRefresh.Calls == 1 &&
                       reviewRefresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "review handler should refresh rendering after success"))
        {
            return 1;
        }

        std::filesystem::remove_all(SolverCaseDir());
    }

    return 0;
}
