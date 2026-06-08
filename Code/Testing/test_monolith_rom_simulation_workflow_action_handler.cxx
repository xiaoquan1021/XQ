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
        if (Expect(PrepareRomWorkflow(*context,
                                      QStringLiteral("run-rom-solver")),
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
                   "unsupported ROM calibration fixture should prepare workflow"))
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
                "unsupported ROM calibration fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "unsupported ROM calibration should fail"))
        {
            return 1;
        }
        if (Expect(message == QStringLiteral(
                                  "Calibrate Boundary Conditions is not wired to a native ROM Simulation runtime yet."),
                   "unsupported ROM calibration diagnostic should name operation"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    return 0;
}
