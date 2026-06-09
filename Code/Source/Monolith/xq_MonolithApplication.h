#ifndef XQ_MONOLITHAPPLICATION_H
#define XQ_MONOLITHAPPLICATION_H

#include <memory>

class QWidget;
class QmitkStdMultiWidget;

namespace xq::core
{
class ApplicationContext;
class FileImportPathProvider;
}

namespace xq::infrastructure
{
class MitkFileDataImportCommand;
class MitkRenderRefreshService;
}

namespace xq::presentation
{
class MainWindow;
class QtFileImportPathProvider;
}

namespace xq
{

struct ConfiguredMainWindow
{
    ~ConfiguredMainWindow();

    std::unique_ptr<xq::presentation::QtFileImportPathProvider>
        OwnedPathProvider;
    std::unique_ptr<xq::infrastructure::MitkRenderRefreshService>
        RenderRefresh;
    std::unique_ptr<xq::infrastructure::MitkFileDataImportCommand>
        ImportCommand;
    std::unique_ptr<xq::presentation::MainWindow> Window;
};

std::unique_ptr<ConfiguredMainWindow> CreateConfiguredMainWindow(
    xq::core::ApplicationContext& context,
    xq::core::FileImportPathProvider* pathProvider = nullptr);

QWidget* CreateMitkImageNavigator(QmitkStdMultiWidget& multiWidget,
                                  QWidget* parent = nullptr);

} // namespace xq

#endif // XQ_MONOLITHAPPLICATION_H
