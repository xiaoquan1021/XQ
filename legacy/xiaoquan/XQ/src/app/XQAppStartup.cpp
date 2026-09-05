#include "app/XQAppStartup.h"

#include "app/XQMainWindow.h"
#include "io/project/XQProjectReader.h"

#include <QApplication>

#include <filesystem>
#include <utility>

namespace xq {
namespace {

std::string assetRootDirForProjectPath(const std::string& projectFilePath)
{
    std::filesystem::path path(projectFilePath);
    return (path.parent_path() / (path.stem().string() + ".assets")).string();
}

} // namespace

XQAppStartupConfig parseAppStartupArguments(int argc, char** argv)
{
    XQAppStartupConfig config;
    if (argc > 1 && argv != nullptr && argv[1] != nullptr) {
        config.projectPath = argv[1];
    }
    return config;
}

XQAppStartupStatus initializeAppStartup(const XQAppStartupConfig& config,
                                        XQAppStartupState* state)
{
    if (state == nullptr) {
        return XQAppStartupStatus::ProjectOpenFailed;
    }

    state->project = XQProject();
    state->commandStack.clear();
    state->assetRootDir.clear();

    if (config.projectPath.empty()) {
        return state->project.open() == XQProject::LifecycleResult::Ok
            ? XQAppStartupStatus::Ok
            : XQAppStartupStatus::ProjectOpenFailed;
    }

    XQProjectReadResult result;
    XQProjectReadOptions options;
    options.lazyGeometry = true;
    if (XQProjectReader::load(config.projectPath, &result, options)
        != XQProjectReader::Status::Ok) {
        return XQAppStartupStatus::ProjectLoadFailed;
    }

    state->project = std::move(result.project);
    state->assetRootDir = assetRootDirForProjectPath(config.projectPath);
    return XQAppStartupStatus::Ok;
}

void attachMainWindowWorkflow(XQMainWindow* window, XQAppStartupState* state)
{
    if (window == nullptr || state == nullptr) {
        return;
    }

    window->attachWorkflow(&state->project.scene(), &state->commandStack);
    if (!state->assetRootDir.empty()) {
        window->attachGeometryResources(&state->project.assetRegistry(), state->assetRootDir);
    }
}

int runXQApp(int argc, char** argv)
{
    QApplication app(argc, argv);

    XQAppStartupState state;
    const XQAppStartupStatus status =
        initializeAppStartup(parseAppStartupArguments(argc, argv), &state);
    if (status != XQAppStartupStatus::Ok) {
        return 1;
    }

    XQMainWindow window;
    attachMainWindowWorkflow(&window, &state);
    window.show();

    return app.exec();
}

} // namespace xq
