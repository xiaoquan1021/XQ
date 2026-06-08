#include "xq_MonolithApplication.h"

#include "Core/xq_DataImportCommand.h"
#include "Infrastructure/xq_MitkFileDataImportCommand.h"
#include "Infrastructure/xq_MitkRenderRefreshService.h"
#include "Infrastructure/xq_ImagePreprocessingWorkflowActionHandler.h"
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
    configured->Window =
        std::make_unique<xq::presentation::MainWindow>(context);
    configured->Window->SetDataImportCommand(
        configured->ImportCommand.get());
    return configured;
}

} // namespace xq
