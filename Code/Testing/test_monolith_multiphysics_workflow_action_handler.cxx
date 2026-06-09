#include "Infrastructure/xq_MultiPhysicsWorkflowActionHandler.h"

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

#include <xq_MitkMultiPhysicsJob.h>
#include <xq_MitkROMJob.h>
#include <xq_MitkGrid.h>
#include <xq_MultiPhysicsJob.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ROMJob.h>
#include <xq_TetGenGrid.h>

#include <QCoreApplication>

#include <mitkDataNode.h>

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

xq::core::DataImportRequest MakeRomImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("rom-001");
    request.SourcePath = QStringLiteral("C:/studies/rom-001.xqrom");
    request.DisplayName = QStringLiteral("Main ROM");
    request.Modality = QStringLiteral("ROMSimulation");
    request.WorkflowRole = xq::core::DataWorkflowRole::ROMSimulation;
    return request;
}

std::unique_ptr<xq_ROMJob> MakeRomJob()
{
    auto job = std::make_unique<xq_ROMJob>();
    job->SetJobName("Main ROM");
    job->SetModelType("1D");
    job->SetCapProp("inlet", "role", "inflow");
    job->SetCapProp("outlet", "role", "outflow");
    job->SetRCR("outlet", 100.0, 1.0e-5, 900.0);
    job->AddOutputField("pressure");
    job->AddOutputField("flow");
    return job;
}

mitk::DataNode::Pointer MakeRomNode()
{
    auto mitkJob = xq_MitkROMJob::New();
    mitkJob->SetROMJob(MakeRomJob());
    mitkJob->SetStatus("configured");

    auto node = mitk::DataNode::New();
    node->SetName("Main ROM");
    node->SetData(mitkJob);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::ROMSimulation);
    xq::pipeline::SetStringProperty(
        node, xq::pipeline::kSourceMeshProperty, "Main Mesh");
    xq::pipeline::SetStringProperty(node, "xq.rom.status", "configured");
    return node;
}

xq::core::DataImportRequest MakeResultImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("result-001");
    request.SourcePath = QStringLiteral("C:/studies/result-001.vtu");
    request.DisplayName = QStringLiteral("Coupled Result");
    request.Modality = QStringLiteral("SimulationResult");
    request.WorkflowRole = xq::core::DataWorkflowRole::SimulationResult;
    return request;
}

vtkSmartPointer<vtkUnstructuredGrid> MakeResultGrid()
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

    auto displacement = vtkSmartPointer<vtkDoubleArray>::New();
    displacement->SetName("wall_displacement");
    displacement->SetNumberOfComponents(1);
    displacement->SetNumberOfTuples(4);
    for (vtkIdType i = 0; i < 4; ++i)
        displacement->SetValue(i, 0.1 * static_cast<double>(i));
    grid->GetPointData()->AddArray(displacement);
    return grid;
}

mitk::DataNode::Pointer MakeResultNode()
{
    auto* tetGrid = new xq_TetGenGrid();
    tetGrid->SetVolumeMesh(MakeResultGrid());

    auto mitkGrid = xq_MitkGrid::New();
    mitkGrid->SetMesh(tetGrid, 0);

    auto node = mitk::DataNode::New();
    node->SetName("Coupled Result");
    node->SetData(mitkGrid);
    xq::pipeline::MarkGeneratedNode(node,
                                    xq::pipeline::Stage::Result,
                                    "coupled_result_import",
                                    "test",
                                    "1");
    node->SetStringProperty("xq.type", "result");
    xq::pipeline::SetStringProperty(
        node,
        "xq.result.field_names",
        "point:coupled_pressure,point:wall_displacement");
    xq::pipeline::SetStringProperty(node,
                                    "xq.result.field_name",
                                    "point:coupled_pressure");
    node->SetIntProperty("xq.result.field_count", 2);
    xq::pipeline::SetStringProperty(
        node, "xq.result.source_multiphysics", "Main ROM_multiphysics");
    return node;
}

bool PrepareMultiPhysicsReviewWorkflow(xq::core::ApplicationContext& context)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("multiphysics")))
    {
        return false;
    }
    const auto importResult =
        context.DataImports()->Import(MakeResultImport(), &message);
    if (!importResult.Succeeded)
        return false;

    return context.WorkflowOperations()->SelectOperation(
        QStringLiteral("multiphysics"),
        QStringLiteral("review-coupled-results"),
        &message);
}

bool PrepareMultiPhysicsWorkflow(xq::core::ApplicationContext& context)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("multiphysics")))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SelectOperation(
            QStringLiteral("multiphysics"),
            QStringLiteral("configure-coupling"),
            &message))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SetParameterValue(
            QStringLiteral("multiphysics"),
            QStringLiteral("configure-coupling"),
            QStringLiteral("coupling-iterations"),
            3,
            &message))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SetParameterValue(
            QStringLiteral("multiphysics"),
            QStringLiteral("configure-coupling"),
            QStringLiteral("relaxation-factor"),
            0.5,
            &message))
    {
        return false;
    }

    const auto importResult =
        context.DataImports()->Import(MakeRomImport(), &message);
    return importResult.Succeeded;
}

bool PrepareMultiPhysicsWorkflow(xq::core::ApplicationContext& context,
                                 const QString& operationId)
{
    if (!PrepareMultiPhysicsWorkflow(context))
        return false;

    QString message;
    return context.WorkflowOperations()->SelectOperation(
        QStringLiteral("multiphysics"),
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
                    RegisterDynamicMultiPhysicsWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "dynamic MultiPhysics handler should register"))
        {
            return 1;
        }
        if (Expect(context->WorkflowActions()->HasHandler(
                       QStringLiteral("multiphysics")),
                   "dynamic MultiPhysics handler should be discoverable"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareMultiPhysicsWorkflow(*context),
                   "missing ROM fixture should prepare MultiPhysics workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicMultiPhysicsWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "missing ROM fixture should install MultiPhysics handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "MultiPhysics handler should reject missing MITK ROM node"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Active ROM or simulation prep node is required for multiphysics coupling."),
                   "missing MultiPhysics ROM diagnostic should be specific"))
        {
            return 1;
        }
        if (Expect(context->DataCatalog()->FindById(
                       QStringLiteral("rom-001-configure-coupling")) ==
                       nullptr,
                   "failed MultiPhysics handler should not register result catalog"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareMultiPhysicsWorkflow(*context),
                   "valid MultiPhysics fixture should prepare workflow"))
        {
            return 1;
        }

        auto romNode = MakeRomNode();
        context->DataStorage()->Add(romNode);
        context->DataNodes()->BindNode(QStringLiteral("rom-001"), romNode);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicMultiPhysicsWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "valid MultiPhysics fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "MultiPhysics handler should create coupling job"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered multiphysics catalog entry."),
                   "MultiPhysics handler should report catalog commit success"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }

        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("rom-001-configure-coupling"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::MultiPhysics &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/multiphysics/rom-001-configure-coupling"),
                   "MultiPhysics handler should register generated catalog entry"))
        {
            return 1;
        }
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-rom-001-configure-coupling"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("rom-001-configure-coupling"),
                   "MultiPhysics handler should register generated hierarchy entry"))
        {
            return 1;
        }

        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("rom-001-configure-coupling"));
        auto* mitkJob = resultNode.IsNotNull()
                            ? dynamic_cast<xq_MitkMultiPhysicsJob*>(
                                  resultNode->GetData())
                            : nullptr;
        auto* job = mitkJob ? mitkJob->GetJob(0) : nullptr;
        std::string sourceRom;
        std::string status;
        if (Expect(mitkJob != nullptr &&
                       job != nullptr &&
                       job->GetJobName() == "Main ROM_multiphysics" &&
                       job->Validate().empty() &&
                       job->GetDomains().size() == 2 &&
                       job->GetEquations().size() == 1 &&
                       job->GetEquations().front().type ==
                           xq_MultiPhysicsEquationType::FSI &&
                       resultNode->GetStringProperty("xq.source.rom",
                                                     sourceRom) &&
                       sourceRom == "Main ROM" &&
                       resultNode->GetStringProperty(
                           "xq.multiphysics.status",
                           status) &&
                       status == "configured" &&
                       xq::pipeline::HasStage(
                           resultNode,
                           xq::pipeline::Stage::MultiPhysics),
                   "MultiPhysics handler should bind generated coupling job"))
        {
            return 1;
        }
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("rom-001-configure-coupling"),
                   "MultiPhysics handler should select generated job"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "MultiPhysics handler should refresh rendering after success"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareMultiPhysicsWorkflow(
                       *context,
                       QStringLiteral("run-coupled-solve")),
                   "unsupported coupled solve fixture should prepare workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicMultiPhysicsWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "unsupported coupled solve fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "unsupported coupled solve should fail"))
        {
            return 1;
        }
        if (Expect(message == QStringLiteral(
                                  "Coupled Solve is not wired to a native Multi-Physics runtime yet."),
                   "unsupported coupled solve diagnostic should name operation"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareMultiPhysicsWorkflow(
                       *context,
                       QStringLiteral("review-coupled-results")),
                   "missing coupled result review fixture should prepare workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicMultiPhysicsWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "missing coupled result review fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "coupled review should require selected result"))
        {
            return 1;
        }
        if (Expect(message == QStringLiteral(
                                  "Active coupled result node is required for multiphysics review."),
                   "missing coupled result diagnostic should require result"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareMultiPhysicsReviewWorkflow(*context),
                   "valid coupled review fixture should prepare workflow"))
        {
            return 1;
        }

        auto resultNode = MakeResultNode();
        context->DataStorage()->Add(resultNode);
        context->DataNodes()->BindNode(QStringLiteral("result-001"),
                                       resultNode);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicMultiPhysicsWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "valid coupled review fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "coupled review should activate result display"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Prepared multiphysics result review for point:coupled_pressure."),
                   "coupled review should report selected scalar"))
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
                       activeScalar == "coupled_pressure" &&
                       resultNode->GetStringProperty(
                           "xq.review.multiphysics.active_scalar",
                           reviewScalar) &&
                       reviewScalar == "point:coupled_pressure" &&
                       resultNode->GetStringProperty(
                           "xq.review.multiphysics.status",
                           reviewStatus) &&
                       reviewStatus == "ready",
                   "coupled review should store scalar review metadata"))
        {
            return 1;
        }
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("result-001"),
                   "coupled review should preserve result selection"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "coupled review should refresh rendering after success"))
        {
            return 1;
        }
    }

    return 0;
}
