#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_TaskRunner.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowContextService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_ImagePreprocessingWorkflowService.h"
#include "Domain/xq_WorkflowActionHandlers.h"

#include <QCoreApplication>

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

xq::core::WorkflowContextSnapshot MakeSnapshot(
    const QString& workflowId,
    const QString& workflowTitle,
    const QString& entryId,
    const QString& displayName,
    xq::core::DataWorkflowRole role)
{
    xq::core::WorkflowContextSnapshot snapshot;
    snapshot.WorkflowId = workflowId;
    snapshot.WorkflowTitle = workflowTitle;
    snapshot.RequiresSelectedData = true;
    snapshot.HasSelectedData = !entryId.trimmed().isEmpty();
    snapshot.HasCompatibleSelection = true;
    snapshot.SelectedCatalogEntryId = entryId;
    snapshot.SelectedDataDisplayName = displayName;
    snapshot.SelectedDataRole = role;
    return snapshot;
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

    xq::domain::ImagePreprocessingWorkflowService service;

    const auto imageResult = service.Run(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("image-001"),
                     QStringLiteral("CTA Image"),
                     xq::core::DataWorkflowRole::Image));
    if (Expect(imageResult.Succeeded,
               "image preprocessing service should accept image data"))
        return 1;
    if (Expect(imageResult.SourceCatalogEntryId ==
                   QStringLiteral("image-001"),
               "image preprocessing result should expose source id"))
        return 1;
    if (Expect(imageResult.SelectedDataDisplayName ==
                   QStringLiteral("CTA Image"),
               "image preprocessing result should expose display name"))
        return 1;
    if (Expect(imageResult.Message ==
                   QStringLiteral("Image Preprocessing domain workflow accepted CTA Image."),
               "image preprocessing success message should stay stable"))
        return 1;

    const auto dicomResult = service.Run(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("dicom-001"),
                     QStringLiteral("CT Series"),
                     xq::core::DataWorkflowRole::DICOMSeries));
    if (Expect(dicomResult.Succeeded,
               "image preprocessing service should accept DICOM series"))
        return 1;

    const auto wrongWorkflowResult = service.Run(
        MakeSnapshot(QStringLiteral("path"),
                     QStringLiteral("Path"),
                     QStringLiteral("image-002"),
                     QStringLiteral("Wrong Workflow CTA"),
                     xq::core::DataWorkflowRole::Image));
    if (Expect(!wrongWorkflowResult.Succeeded,
               "image preprocessing service should reject wrong workflow id"))
        return 1;
    if (Expect(wrongWorkflowResult.Message ==
                   QStringLiteral("Image preprocessing service requires the image-preprocessing workflow."),
               "wrong workflow rejection message should be explicit"))
        return 1;

    const auto incompatibleRoleResult = service.Run(
        MakeSnapshot(QStringLiteral("image-preprocessing"),
                     QStringLiteral("Image Preprocessing"),
                     QStringLiteral("model-001"),
                     QStringLiteral("Aorta Model"),
                     xq::core::DataWorkflowRole::Model));
    if (Expect(!incompatibleRoleResult.Succeeded,
               "image preprocessing service should reject incompatible data"))
        return 1;
    if (Expect(incompatibleRoleResult.Message ==
                   QStringLiteral("Selected data is not compatible with image preprocessing."),
               "incompatible role rejection message should be explicit"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *context->WorkflowActions());
    if (Expect(context->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "image preprocessing workflow should be selectable"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto importResult =
        context->DataImports()->Import(MakeImport(
                                           QStringLiteral("registered-image"),
                                           QStringLiteral("Registered CTA"),
                                           xq::core::DataWorkflowRole::Image),
                                       &errorMessage);
    if (Expect(importResult.Succeeded, "registered image import should succeed"))
    {
        delete context;
        return 1;
    }

    QString actionMessage;
    if (Expect(context->WorkflowActions()->RunActiveWorkflowAction(
                   &actionMessage),
               "registered image preprocessing handler should run service"))
    {
        delete context;
        return 1;
    }
    const auto history = context->Tasks()->History();
    if (Expect(!history.empty(),
               "registered image preprocessing handler should record task"))
    {
        delete context;
        return 1;
    }
    if (Expect(history.back().Message ==
                   QStringLiteral("Image Preprocessing domain workflow accepted Registered CTA."),
               "registered image preprocessing handler should use service message"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
