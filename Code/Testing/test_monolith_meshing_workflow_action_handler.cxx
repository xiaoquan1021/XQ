#include "Infrastructure/xq_MeshingWorkflowActionHandler.h"

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
#include <xq_Grid.h>
#include <xq_Model.h>
#include <xq_PipelineDataUtils.h>
#include <xq_PolyGeometry.h>

#include <QCoreApplication>

#include <mitkDataNode.h>

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

    auto model = xq_Model::New();
    model->SetModelElement(geometry, 0);

    auto node = mitk::DataNode::New();
    node->SetName(name);
    node->SetData(model);
    node->SetBoolProperty("xq.model.qa.ok", true);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Model);
    xq::pipeline::SetStringProperty(
        node,
        xq::pipeline::kSourceContourGroupsProperty,
        "Main Segmentation");
    return node;
}

xq::core::DataImportRequest MakeModelImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("model-001");
    request.SourcePath = QStringLiteral("C:/studies/model-001.xqmodel.vtp");
    request.DisplayName = QStringLiteral("Main Model");
    request.Modality = QStringLiteral("Model");
    request.WorkflowRole = xq::core::DataWorkflowRole::Model;
    return request;
}

bool PrepareMeshingWorkflow(xq::core::ApplicationContext& context)
{
    QString message;
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context.WorkflowActions(),
        context.WorkflowOperations());
    if (!context.WorkflowSelection()->SelectWorkflow(
            QStringLiteral("meshing")))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SelectOperation(
            QStringLiteral("meshing"),
            QStringLiteral("generate-volume-mesh"),
            &message))
    {
        return false;
    }
    if (!context.WorkflowOperations()->SetParameterValue(
            QStringLiteral("meshing"),
            QStringLiteral("generate-volume-mesh"),
            QStringLiteral("element-size"),
            2.0,
            &message))
    {
        return false;
    }

    const auto importResult =
        context.DataImports()->Import(MakeModelImport(), &message);
    return importResult.Succeeded;
}

bool PrepareMeshingWorkflow(xq::core::ApplicationContext& context,
                            const QString& operationId)
{
    if (!PrepareMeshingWorkflow(context))
        return false;

    QString message;
    return context.WorkflowOperations()->SelectOperation(
        QStringLiteral("meshing"),
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
                    RegisterDynamicMeshingWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "dynamic meshing handler should register"))
        {
            return 1;
        }
        if (Expect(context->WorkflowActions()->HasHandler(
                       QStringLiteral("meshing")),
                   "dynamic meshing handler should be discoverable"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareMeshingWorkflow(*context),
                   "missing model fixture should prepare meshing workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicMeshingWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "missing model fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "meshing handler should reject missing model node"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Active model node is required for meshing."),
                   "missing model diagnostic should be specific"))
        {
            return 1;
        }
        if (Expect(context->DataCatalog()->FindById(
                       QStringLiteral("model-001-generate-volume-mesh")) ==
                       nullptr,
                   "failed meshing handler should not register result catalog"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareMeshingWorkflow(*context),
                   "valid meshing fixture should prepare workflow"))
        {
            return 1;
        }

        auto modelNode = MakeModelNode("Main Model");
        context->DataStorage()->Add(modelNode);
        context->DataNodes()->BindNode(QStringLiteral("model-001"),
                                       modelNode);

        QString message;
        FakeRenderRefreshService refresh;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicMeshingWorkflowActionHandler(
                        *context,
                        &refresh,
                        &message),
                "valid meshing fixture should install handler"))
        {
            return 1;
        }

        if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "meshing handler should create volume mesh"))
        {
            return 1;
        }
        if (Expect(message ==
                       QStringLiteral(
                           "Registered mesh result catalog entry."),
                   "meshing handler should report catalog commit success"))
        {
            return 1;
        }
        const auto* entry = context->DataCatalog()->FindById(
            QStringLiteral("model-001-generate-volume-mesh"));
        if (Expect(entry != nullptr &&
                       entry->WorkflowRole ==
                           xq::core::DataWorkflowRole::Mesh &&
                       entry->SourcePath ==
                           QStringLiteral(
                               "xq://generated/mesh/model-001-generate-volume-mesh"),
                   "meshing handler should register generated catalog entry"))
        {
            return 1;
        }
        const auto* hierarchyNode = context->DataHierarchy()->FindNode(
            QStringLiteral("data-model-001-generate-volume-mesh"));
        if (Expect(hierarchyNode != nullptr &&
                       hierarchyNode->DataCatalogEntryId ==
                           QStringLiteral("model-001-generate-volume-mesh"),
                   "meshing handler should register generated hierarchy entry"))
        {
            return 1;
        }
        auto resultNode = context->DataNodes()->FindNode(
            QStringLiteral("model-001-generate-volume-mesh"));
        auto* grid = resultNode.IsNotNull()
                         ? dynamic_cast<xq_MitkGrid*>(resultNode->GetData())
                         : nullptr;
        auto* mesh = grid ? grid->GetMesh(0) : nullptr;
        if (Expect(grid != nullptr &&
                       mesh != nullptr &&
                       mesh->GetVolumeMesh() != nullptr &&
                       mesh->GetVolumeMesh()->GetNumberOfCells() > 0,
                   "meshing handler should bind generated volume mesh"))
        {
            return 1;
        }
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("model-001-generate-volume-mesh"),
                   "meshing handler should select generated mesh"))
        {
            return 1;
        }
        if (Expect(refresh.Calls == 1 &&
                       refresh.LastDataStorage.GetPointer() ==
                           context->DataStorage().GetPointer(),
                   "meshing handler should refresh rendering after success"))
        {
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareMeshingWorkflow(
                       *context,
                       QStringLiteral("generate-surface-mesh")),
                   "unsupported surface mesh fixture should prepare workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicMeshingWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "unsupported surface mesh fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "unsupported surface mesh should fail"))
        {
            return 1;
        }
        if (Expect(message == QStringLiteral(
                                  "Generate Surface Mesh is not wired to a native Meshing runtime yet."),
                   "unsupported surface mesh diagnostic should name operation"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        if (Expect(PrepareMeshingWorkflow(*context,
                                          QStringLiteral("boundary-layers")),
                   "unsupported boundary layers fixture should prepare workflow"))
        {
            return 1;
        }

        QString message;
        if (Expect(
                xq::infrastructure::
                    RegisterDynamicMeshingWorkflowActionHandler(
                        *context,
                        nullptr,
                        &message),
                "unsupported boundary layers fixture should install handler"))
        {
            return 1;
        }

        if (Expect(!context->WorkflowActions()->RunActiveWorkflowAction(
                       &message),
                   "unsupported boundary layers should fail"))
        {
            return 1;
        }
        if (Expect(message == QStringLiteral(
                                  "Boundary Layers is not wired to a native Meshing runtime yet."),
                   "unsupported boundary layers diagnostic should name operation"))
        {
            std::cerr << message.toStdString() << '\n';
            return 1;
        }
    }

    return 0;
}
