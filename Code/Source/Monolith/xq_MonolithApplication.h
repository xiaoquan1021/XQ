#ifndef XQ_MONOLITHAPPLICATION_H
#define XQ_MONOLITHAPPLICATION_H

#include <memory>

class QWidget;
class QmitkStdMultiWidget;
class QApplication;
class QString;

namespace xq::core
{
class ApplicationContext;
class FileImportPathProvider;
}

namespace xq::infrastructure
{
class DicomImportCommand;
class MitkFileDataImportCommand;
class MitkRenderRefreshService;
class MitkSceneExportService;
}

namespace xq::presentation
{
class MainWindow;
class QtDicomImportPathProvider;
class QtFileImportPathProvider;
class QtProjectFilePathProvider;
class QtSceneFilePathProvider;
class QtScreenshotFilePathProvider;
}

namespace xq
{

struct ConfiguredMainWindow
{
    ~ConfiguredMainWindow();

    std::unique_ptr<xq::presentation::QtFileImportPathProvider>
        OwnedPathProvider;
    std::unique_ptr<xq::presentation::QtDicomImportPathProvider>
        OwnedDicomPathProvider;
    std::unique_ptr<xq::presentation::QtProjectFilePathProvider>
        OwnedProjectPathProvider;
    std::unique_ptr<xq::presentation::QtSceneFilePathProvider>
        OwnedScenePathProvider;
    std::unique_ptr<xq::presentation::QtScreenshotFilePathProvider>
        OwnedScreenshotPathProvider;
    std::unique_ptr<xq::infrastructure::MitkRenderRefreshService>
        RenderRefresh;
    std::unique_ptr<xq::infrastructure::MitkFileDataImportCommand>
        ImportCommand;
    std::unique_ptr<xq::infrastructure::DicomImportCommand>
        DicomImportCommand;
    std::unique_ptr<xq::infrastructure::MitkSceneExportService>
        SceneExport;
    std::unique_ptr<xq::presentation::MainWindow> Window;
};

std::unique_ptr<ConfiguredMainWindow> CreateConfiguredMainWindow(
    xq::core::ApplicationContext& context,
    xq::core::FileImportPathProvider* pathProvider = nullptr);

QWidget* CreateMitkImageNavigator(QmitkStdMultiWidget& multiWidget,
                                  QWidget* parent = nullptr);

bool ApplyXqWorkbenchTheme(QApplication& application,
                           QString* errorMessage = nullptr);

} // namespace xq

#endif // XQ_MONOLITHAPPLICATION_H
