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
#include <xq_MitkSolverJob.h>
#include <xq_Model.h>
#include <xq_PipelineDataUtils.h>
#include <xq_PolyGeometry.h>
#include <xq_SolverJob.h>

#include <QCoreApplication>

#include <mitkDataNode.h>

#include <vtkCellData.h>
#include <vtkIntArray.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>

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

bool PrepareFlowWorkflow(xq::core::ApplicationContext& context)
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
            QStringLiteral("configure-cfd-job"),
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

    const auto importResult =
        context.DataImports()->Import(MakeMeshImport(), &message);
    return importResult.Succeeded;
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
                       solverJob->GetSolverType() == "xq_export_only" &&
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

    return 0;
}
