#include "xq_MonolithApplication.h"

#include "Core/xq_DataImportCommand.h"
#include "Infrastructure/xq_MitkFileDataImportCommand.h"
#include "Infrastructure/xq_MitkRenderRefreshService.h"
#include "Infrastructure/xq_ImagePreprocessingWorkflowActionHandler.h"
#include "Infrastructure/xq_PathWorkflowActionHandler.h"
#include "Infrastructure/xq_SegmentationWorkflowActionHandler.h"
#include "Infrastructure/xq_ModelingWorkflowActionHandler.h"
#include "Infrastructure/xq_MeshingWorkflowActionHandler.h"
#include "Infrastructure/xq_FlowSimulationWorkflowActionHandler.h"
#include "Infrastructure/xq_RomSimulationWorkflowActionHandler.h"
#include "Infrastructure/xq_MultiPhysicsWorkflowActionHandler.h"
#include "Infrastructure/xq_PythonApiWorkflowActionHandler.h"
#include "Presentation/xq_MainWindow.h"
#include "Presentation/xq_QtFileImportPathProvider.h"

namespace xq
{

ConfiguredMainWindow::~ConfiguredMainWindow() = default;

std::unique_ptr<ConfiguredMainWindow> CreateConfiguredMainWindow(
    xq::core::ApplicationContext& context,
    xq::core::FileImportPathProvider* pathProvider)
{
    auto configured = std::make_unique<ConfiguredMainWindow>();
    if (!pathProvider)
    {
        configured->OwnedPathProvider =
            std::make_unique<xq::presentation::QtFileImportPathProvider>();
        pathProvider = configured->OwnedPathProvider.get();
    }

    configured->RenderRefresh =
        std::make_unique<xq::infrastructure::MitkRenderRefreshService>();
    configured->ImportCommand =
        std::make_unique<xq::infrastructure::MitkFileDataImportCommand>(
            pathProvider,
            nullptr,
            configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicImagePreprocessingWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicPathWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicSegmentationWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicModelingWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicMeshingWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicFlowSimulationWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicRomSimulationWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicMultiPhysicsWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicPythonApiWorkflowActionHandler(context);
    configured->Window =
        std::make_unique<xq::presentation::MainWindow>(context);
    configured->Window->SetDataImportCommand(
        configured->ImportCommand.get());
    return configured;
}

} // namespace xq
