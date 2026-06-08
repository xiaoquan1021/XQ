#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportCommand.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_TaskRunner.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Domain/xq_WorkflowActionHandlers.h"
#include "Presentation/xq_MainWindow.h"
#include "xq_MonolithApplication.h"

#include <QAction>
#include <QApplication>

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

class CancelPathProvider : public xq::core::FileImportPathProvider
{
public:
    mutable int Invocations = 0;

    QString ChooseFilePath() const override
    {
        ++Invocations;
        return {};
    }
};

xq::core::DataImportRequest MakeImageImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("image-001");
    request.SourcePath = QStringLiteral("C:/studies/image-001.nii");
    request.DisplayName = QStringLiteral("CTA Image");
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

xq::core::DataImportRequest MakePathImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("path-001");
    request.SourcePath = QStringLiteral("C:/studies/path-001.xqpth");
    request.DisplayName = QStringLiteral("Main Path");
    request.Modality = QStringLiteral("Path");
    request.WorkflowRole = xq::core::DataWorkflowRole::Path;
    return request;
}

xq::core::DataImportRequest MakeSegmentationImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("seg-001");
    request.SourcePath = QStringLiteral("C:/studies/seg-001.xqseg");
    request.DisplayName = QStringLiteral("Main Segmentation");
    request.Modality = QStringLiteral("Segmentation");
    request.WorkflowRole = xq::core::DataWorkflowRole::Segmentation;
    return request;
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

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto context = std::unique_ptr<xq::core::ApplicationContext>(
        xq::core::ApplicationContext::CreateDefault());
    CancelPathProvider provider;
    auto configuredWindow =
        xq::CreateConfiguredMainWindow(*context, &provider);
    auto* window = configuredWindow->Window.get();

    if (Expect(configuredWindow->RenderRefresh != nullptr,
               "configured monolith window should own a render refresh service"))
        return 1;

    auto* importAction =
        window->findChild<QAction*>(QStringLiteral("xqImportDataAction"));
    if (Expect(importAction != nullptr,
               "configured monolith window should expose import action"))
        return 1;
    if (Expect(importAction->isEnabled(),
               "configured monolith import action should be enabled"))
        return 1;

    int notConfiguredDiagnostics = 0;
    QObject::connect(context.get(),
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&notConfiguredDiagnostics](const QString& message) {
                         if (message ==
                             QStringLiteral("No data import command is configured."))
                         {
                             ++notConfiguredDiagnostics;
                         }
                     });

    importAction->trigger();
    app.processEvents();

    if (Expect(provider.Invocations == 1,
               "configured monolith import action should invoke provider"))
        return 1;
    if (Expect(notConfiguredDiagnostics == 0,
               "configured monolith import action should have an import command"))
        return 1;
    if (Expect(context->DataCatalog()->Entries().isEmpty(),
               "cancelled real file dialog path should not mutate catalog"))
        return 1;

    auto preprocessingContext =
        std::unique_ptr<xq::core::ApplicationContext>(
            xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *preprocessingContext->WorkflowActions(),
        preprocessingContext->WorkflowOperations());
    CancelPathProvider preprocessingProvider;
    auto preprocessingWindow =
        xq::CreateConfiguredMainWindow(*preprocessingContext,
                                       &preprocessingProvider);

    if (Expect(preprocessingContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("image-preprocessing")),
               "configured preprocessing workflow should be selectable"))
        return 1;

    QString message;
    const auto importResult =
        preprocessingContext->DataImports()->Import(MakeImageImport(),
                                                    &message);
    if (Expect(importResult.Succeeded,
               "configured preprocessing image import should succeed"))
        return 1;

    if (Expect(!preprocessingContext->WorkflowActions()
                    ->RunActiveWorkflowAction(&message),
               "configured preprocessing action should require a MITK source node"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Active image node is required for image preprocessing."),
               "configured preprocessing action should use infrastructure diagnostic"))
        return 1;
    const auto history = preprocessingContext->Tasks()->History();
    if (Expect(!history.empty() &&
                   !history.back().Succeeded &&
                   history.back().Message == message,
               "configured preprocessing action failure should be recorded"))
        return 1;

    auto pathContext =
        std::unique_ptr<xq::core::ApplicationContext>(
            xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *pathContext->WorkflowActions(),
        pathContext->WorkflowOperations());
    CancelPathProvider pathProvider;
    auto pathWindow =
        xq::CreateConfiguredMainWindow(*pathContext, &pathProvider);

    if (Expect(pathContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("path")),
               "configured path workflow should be selectable"))
        return 1;

    const auto pathImportResult =
        pathContext->DataImports()->Import(MakeImageImport(), &message);
    if (Expect(pathImportResult.Succeeded,
               "configured path image import should succeed"))
        return 1;

    if (Expect(!pathContext->WorkflowActions()->RunActiveWorkflowAction(
                   &message),
               "configured path action should use infrastructure validation"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Path creation requires at least two seed points."),
               "configured path action should require seed points"))
        return 1;

    auto segmentationContext =
        std::unique_ptr<xq::core::ApplicationContext>(
            xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *segmentationContext->WorkflowActions(),
        segmentationContext->WorkflowOperations());
    CancelPathProvider segmentationProvider;
    auto segmentationWindow =
        xq::CreateConfiguredMainWindow(*segmentationContext,
                                       &segmentationProvider);

    if (Expect(segmentationContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("segmentation-2d")),
               "configured segmentation workflow should be selectable"))
        return 1;
    if (Expect(segmentationContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("segmentation-2d"),
                   QStringLiteral("manual-contour"),
                   &message),
               "configured segmentation workflow should select manual contour"))
        return 1;

    const auto segmentationImportResult =
        segmentationContext->DataImports()->Import(MakePathImport(),
                                                   &message);
    if (Expect(segmentationImportResult.Succeeded,
               "configured segmentation path import should succeed"))
        return 1;

    if (Expect(!segmentationContext->WorkflowActions()
                    ->RunActiveWorkflowAction(&message),
               "configured segmentation action should use infrastructure validation"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Active path node is required for 2D segmentation."),
               "configured segmentation action should require a path node"))
        return 1;

    auto modelingContext =
        std::unique_ptr<xq::core::ApplicationContext>(
            xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *modelingContext->WorkflowActions(),
        modelingContext->WorkflowOperations());
    CancelPathProvider modelingProvider;
    auto modelingWindow =
        xq::CreateConfiguredMainWindow(*modelingContext,
                                       &modelingProvider);

    if (Expect(modelingContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("modeling")),
               "configured modeling workflow should be selectable"))
        return 1;
    if (Expect(modelingContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("modeling"),
                   QStringLiteral("build-solid-model"),
                   &message),
               "configured modeling workflow should select build solid model"))
        return 1;

    const auto modelingImportResult =
        modelingContext->DataImports()->Import(MakeSegmentationImport(),
                                               &message);
    if (Expect(modelingImportResult.Succeeded,
               "configured modeling segmentation import should succeed"))
        return 1;

    if (Expect(!modelingContext->WorkflowActions()
                    ->RunActiveWorkflowAction(&message),
               "configured modeling action should use infrastructure validation"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Active segmentation node is required for modeling."),
               "configured modeling action should require a segmentation node"))
        return 1;

    auto meshingContext =
        std::unique_ptr<xq::core::ApplicationContext>(
            xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *meshingContext->WorkflowActions(),
        meshingContext->WorkflowOperations());
    CancelPathProvider meshingProvider;
    auto meshingWindow =
        xq::CreateConfiguredMainWindow(*meshingContext,
                                       &meshingProvider);

    if (Expect(meshingContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("meshing")),
               "configured meshing workflow should be selectable"))
        return 1;
    if (Expect(meshingContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("meshing"),
                   QStringLiteral("generate-volume-mesh"),
                   &message),
               "configured meshing workflow should select generate volume mesh"))
        return 1;

    const auto meshingImportResult =
        meshingContext->DataImports()->Import(MakeModelImport(),
                                              &message);
    if (Expect(meshingImportResult.Succeeded,
               "configured meshing model import should succeed"))
        return 1;

    if (Expect(!meshingContext->WorkflowActions()
                    ->RunActiveWorkflowAction(&message),
               "configured meshing action should use infrastructure validation"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Active model node is required for meshing."),
               "configured meshing action should require a model node"))
        return 1;

    return 0;
}
