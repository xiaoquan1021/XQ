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

    auto flowContext =
        std::unique_ptr<xq::core::ApplicationContext>(
            xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *flowContext->WorkflowActions(),
        flowContext->WorkflowOperations());
    CancelPathProvider flowProvider;
    auto flowWindow =
        xq::CreateConfiguredMainWindow(*flowContext,
                                       &flowProvider);

    if (Expect(flowContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("flow-simulation")),
               "configured flow workflow should be selectable"))
        return 1;
    if (Expect(flowContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("flow-simulation"),
                   QStringLiteral("configure-cfd-job"),
                   &message),
               "configured flow workflow should select CFD job configuration"))
        return 1;

    const auto flowImportResult =
        flowContext->DataImports()->Import(MakeMeshImport(), &message);
    if (Expect(flowImportResult.Succeeded,
               "configured flow mesh import should succeed"))
        return 1;

    if (Expect(!flowContext->WorkflowActions()
                    ->RunActiveWorkflowAction(&message),
               "configured flow action should use infrastructure validation"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Active mesh node is required for flow simulation."),
               "configured flow action should require a mesh node"))
        return 1;

    if (Expect(flowContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("flow-simulation"),
                   QStringLiteral("run-steady-flow"),
                   &message),
               "configured flow workflow should select steady flow run"))
        return 1;
    if (Expect(!flowContext->WorkflowActions()
                    ->RunActiveWorkflowAction(&message),
               "configured steady flow action should use infrastructure validation"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Active simulation prep node is required for steady flow solve."),
               "configured steady flow action should require a simulation prep node"))
        return 1;

    if (Expect(flowContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("flow-simulation"),
                   QStringLiteral("review-flow-results"),
                   &message),
               "configured flow workflow should select flow result review"))
        return 1;
    if (Expect(!flowContext->WorkflowActions()
                    ->RunActiveWorkflowAction(&message),
               "configured flow review action should use infrastructure validation"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Active simulation result node is required for flow result review."),
               "configured flow review action should require a simulation result node"))
        return 1;

    auto romContext =
        std::unique_ptr<xq::core::ApplicationContext>(
            xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *romContext->WorkflowActions(),
        romContext->WorkflowOperations());
    CancelPathProvider romProvider;
    auto romWindow =
        xq::CreateConfiguredMainWindow(*romContext,
                                       &romProvider);

    if (Expect(romContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("rom-simulation")),
               "configured ROM workflow should be selectable"))
        return 1;
    if (Expect(romContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("rom-simulation"),
                   QStringLiteral("build-1d-network"),
                   &message),
               "configured ROM workflow should select network build"))
        return 1;
    const auto romImportResult =
        romContext->DataImports()->Import(MakeMeshImport(), &message);
    if (Expect(romImportResult.Succeeded,
               "configured ROM mesh import should succeed"))
        return 1;
    if (Expect(!romContext->WorkflowActions()
                    ->RunActiveWorkflowAction(&message),
               "configured ROM action should use infrastructure validation"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Active mesh or simulation prep node is required for ROM network build."),
               "configured ROM action should require a MITK mesh or simulation prep node"))
        return 1;

    auto multiphysicsContext =
        std::unique_ptr<xq::core::ApplicationContext>(
            xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *multiphysicsContext->WorkflowActions(),
        multiphysicsContext->WorkflowOperations());
    CancelPathProvider multiphysicsProvider;
    auto multiphysicsWindow =
        xq::CreateConfiguredMainWindow(*multiphysicsContext,
                                       &multiphysicsProvider);

    if (Expect(multiphysicsContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("multiphysics")),
               "configured MultiPhysics workflow should be selectable"))
        return 1;
    if (Expect(multiphysicsContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("multiphysics"),
                   QStringLiteral("configure-coupling"),
                   &message),
               "configured MultiPhysics workflow should select coupling configuration"))
        return 1;
    const auto multiphysicsImportResult =
        multiphysicsContext->DataImports()->Import(MakeMeshImport(),
                                                   &message);
    if (Expect(multiphysicsImportResult.Succeeded,
               "configured MultiPhysics mesh import should succeed"))
        return 1;
    if (Expect(!multiphysicsContext->WorkflowActions()
                    ->RunActiveWorkflowAction(&message),
               "configured MultiPhysics action should use infrastructure validation"))
        return 1;
    if (Expect(message == QStringLiteral(
                              "Active ROM or simulation prep node is required for multiphysics coupling."),
               "configured MultiPhysics action should require a MITK ROM or simulation prep node"))
        return 1;

    auto pythonApiContext =
        std::unique_ptr<xq::core::ApplicationContext>(
            xq::core::ApplicationContext::CreateDefault());
    xq::domain::RegisterDefaultWorkflowActionHandlers(
        *pythonApiContext->WorkflowActions(),
        pythonApiContext->WorkflowOperations());
    CancelPathProvider pythonApiProvider;
    auto pythonApiWindow =
        xq::CreateConfiguredMainWindow(*pythonApiContext,
                                       &pythonApiProvider);

    if (Expect(pythonApiContext->WorkflowSelection()->SelectWorkflow(
                   QStringLiteral("python-api")),
               "configured Python API workflow should be selectable"))
        return 1;
    if (Expect(pythonApiContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("python-api"),
                   QStringLiteral("open-python-console"),
                   &message),
               "configured Python API workflow should select console operation"))
        return 1;
    if (Expect(pythonApiContext->WorkflowActions()
                   ->RunActiveWorkflowAction(&message),
               "configured Python API action should use infrastructure diagnostic"))
        return 1;
    if (Expect(message.contains(QStringLiteral(
                       "Python API unavailable in this build")) &&
                   message.contains(QStringLiteral(
                       "C++ inspection service remains available")),
               "configured Python API action should report runtime availability"))
        return 1;
    if (Expect(pythonApiContext->WorkflowOperations()->SelectOperation(
                   QStringLiteral("python-api"),
                   QStringLiteral("export-api-snippet"),
                   &message),
               "configured Python API workflow should select snippet export"))
        return 1;
    if (Expect(pythonApiContext->WorkflowOperations()->SetParameterValue(
                   QStringLiteral("python-api"),
                   QStringLiteral("export-api-snippet"),
                   QStringLiteral("snippet-count"),
                   2,
                   &message),
               "configured Python API snippet count should be configurable"))
        return 1;
    if (Expect(pythonApiContext->WorkflowActions()
                   ->RunActiveWorkflowAction(&message),
               "configured Python API snippet action should use infrastructure behavior"))
        return 1;
    if (Expect(message.contains(QStringLiteral("Python API snippets (2)")) &&
                   message.contains(QStringLiteral("xq.version()")) &&
                   message.contains(QStringLiteral("xq.list_nodes()")) &&
                   !message.contains(QStringLiteral("xq.find_node(name)")),
               "configured Python API snippet action should export limited snippets"))
        return 1;

    return 0;
}
