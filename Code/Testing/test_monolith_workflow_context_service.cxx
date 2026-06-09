#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataManagementService.h"
#include "Core/xq_WorkflowContextService.h"
#include "Core/xq_WorkflowRegistry.h"
#include "Core/xq_WorkflowSelectionService.h"

#include <QCoreApplication>
#include <QObject>

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
    QCoreApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    auto* workflowContext = context->WorkflowContext();
    auto* workflowSelection = context->WorkflowSelection();

    if (Expect(workflowContext != nullptr,
               "ApplicationContext should expose WorkflowContextService"))
    {
        delete context;
        return 1;
    }

    const auto initialSnapshot = workflowContext->Snapshot();
    const auto& workflows = xq::core::DefaultWorkflowRegistry();
    if (Expect(initialSnapshot.WorkflowId == workflows.front().Id,
               "default workflow context should use first workflow id"))
    {
        delete context;
        return 1;
    }
    if (Expect(initialSnapshot.WorkflowTitle == workflows.front().Title,
               "default workflow context should expose workflow title"))
    {
        delete context;
        return 1;
    }
    if (Expect(!initialSnapshot.RequiresSelectedData,
               "default project workflow should not require selected data"))
    {
        delete context;
        return 1;
    }
    if (Expect(!initialSnapshot.HasSelectedData,
               "default workflow context should start without selected data"))
    {
        delete context;
        return 1;
    }
    if (Expect(initialSnapshot.HasCompatibleSelection,
               "workflows without data requirements should be compatible"))
    {
        delete context;
        return 1;
    }
    if (Expect(xq::core::WorkflowContextService::AcceptedDataRolesForWorkflow(
                   QStringLiteral("project")).isEmpty(),
               "project workflow should not require selected data roles"))
    {
        delete context;
        return 1;
    }

    const auto preprocessingRoles =
        xq::core::WorkflowContextService::AcceptedDataRolesForWorkflow(
            QStringLiteral("image-preprocessing"));
    if (Expect(preprocessingRoles.contains(
                   xq::core::DataWorkflowRole::DICOMSeries),
               "image preprocessing should accept DICOM series"))
    {
        delete context;
        return 1;
    }
    if (Expect(preprocessingRoles.contains(
                   xq::core::DataWorkflowRole::Image),
               "image preprocessing should accept image data"))
    {
        delete context;
        return 1;
    }
    if (Expect(!preprocessingRoles.contains(
                   xq::core::DataWorkflowRole::Model),
               "image preprocessing should not accept model data"))
    {
        delete context;
        return 1;
    }

    const auto segmentation3dRoles =
        xq::core::WorkflowContextService::AcceptedDataRolesForWorkflow(
            QStringLiteral("segmentation-3d"));
    if (Expect(segmentation3dRoles.contains(
                   xq::core::DataWorkflowRole::Segmentation),
               "3D segmentation should accept generated segmentation data"))
    {
        delete context;
        return 1;
    }

    int contextChanges = 0;
    QObject::connect(workflowContext,
                     &xq::core::WorkflowContextService::ContextChanged,
                     [&contextChanges]() {
                         ++contextChanges;
                     });

    if (Expect(workflowSelection->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "image preprocessing workflow should be selectable"))
    {
        delete context;
        return 1;
    }
    auto snapshot = workflowContext->Snapshot();
    if (Expect(contextChanges == 1,
               "workflow selection should emit one context change"))
    {
        delete context;
        return 1;
    }
    if (Expect(snapshot.RequiresSelectedData,
               "image preprocessing should require selected data"))
    {
        delete context;
        return 1;
    }
    if (Expect(!snapshot.HasCompatibleSelection,
               "image preprocessing without selection should be incompatible"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto imageImport =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA Image"),
                                           xq::core::DataWorkflowRole::Image),
                                       &errorMessage);
    if (Expect(imageImport.Succeeded, "image import should succeed"))
    {
        delete context;
        return 1;
    }
    snapshot = workflowContext->Snapshot();
    if (Expect(contextChanges == 2,
               "selecting imported image should emit one context change"))
    {
        delete context;
        return 1;
    }
    if (Expect(snapshot.SelectedCatalogEntryId ==
                   QStringLiteral("image-001"),
               "workflow context should expose selected catalog entry id"))
    {
        delete context;
        return 1;
    }
    if (Expect(snapshot.SelectedDataDisplayName ==
                   QStringLiteral("CTA Image"),
               "workflow context should expose selected display name"))
    {
        delete context;
        return 1;
    }
    if (Expect(snapshot.SelectedDataRole ==
                   xq::core::DataWorkflowRole::Image,
               "workflow context should expose selected data role"))
    {
        delete context;
        return 1;
    }
    if (Expect(snapshot.HasSelectedData,
               "workflow context should report selected data"))
    {
        delete context;
        return 1;
    }
    if (Expect(snapshot.HasCompatibleSelection,
               "image data should be compatible with image preprocessing"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataManagement()->RenameEntry(
                   QStringLiteral("image-001"),
                   QStringLiteral("Renamed CTA"),
                   &errorMessage),
               "selected image rename should succeed"))
    {
        delete context;
        return 1;
    }
    snapshot = workflowContext->Snapshot();
    if (Expect(contextChanges == 3,
               "renaming selected data should emit one context change"))
    {
        delete context;
        return 1;
    }
    if (Expect(snapshot.SelectedDataDisplayName ==
                   QStringLiteral("Renamed CTA"),
               "workflow context should refresh selected data display name"))
    {
        delete context;
        return 1;
    }

    if (Expect(workflowSelection->SelectWorkflow(QStringLiteral("meshing")),
               "meshing workflow should be selectable"))
    {
        delete context;
        return 1;
    }
    snapshot = workflowContext->Snapshot();
    if (Expect(contextChanges == 4,
               "meshing workflow selection should emit one context change"))
    {
        delete context;
        return 1;
    }
    if (Expect(snapshot.RequiresSelectedData,
               "meshing should require selected data"))
    {
        delete context;
        return 1;
    }
    if (Expect(!snapshot.HasCompatibleSelection,
               "image data should not be compatible with meshing"))
    {
        delete context;
        return 1;
    }

    const auto modelImport =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("model-001"),
                                           QStringLiteral("Aorta Model"),
                                           xq::core::DataWorkflowRole::Model),
                                       &errorMessage);
    if (Expect(modelImport.Succeeded, "model import should succeed"))
    {
        delete context;
        return 1;
    }
    snapshot = workflowContext->Snapshot();
    if (Expect(contextChanges == 5,
               "selecting imported model should emit one context change"))
    {
        delete context;
        return 1;
    }
    if (Expect(snapshot.SelectedCatalogEntryId ==
                   QStringLiteral("model-001"),
               "workflow context should track selected model id"))
    {
        delete context;
        return 1;
    }
    if (Expect(snapshot.SelectedDataRole ==
                   xq::core::DataWorkflowRole::Model,
               "workflow context should track selected model role"))
    {
        delete context;
        return 1;
    }
    if (Expect(snapshot.HasCompatibleSelection,
               "model data should be compatible with meshing"))
    {
        delete context;
        return 1;
    }

    if (Expect(!workflowSelection->SelectWorkflow(
                   QStringLiteral("missing-workflow")),
               "invalid workflow should be rejected"))
    {
        delete context;
        return 1;
    }
    if (Expect(contextChanges == 5,
               "invalid workflow should not emit context change"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
