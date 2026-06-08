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
#include <xq_MultiPhysicsJob.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ROMJob.h>

#include <QCoreApplication>

#include <mitkDataNode.h>

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

    return 0;
}
