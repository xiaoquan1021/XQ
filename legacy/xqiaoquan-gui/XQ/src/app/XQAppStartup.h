#ifndef XQ_APP_XQ_APP_STARTUP_H
#define XQ_APP_XQ_APP_STARTUP_H

#include "core/XQProject.h"
#include "core/command/XQCommandStack.h"

#include <string>

namespace xq {

class XQMainWindow;

enum class XQAppStartupStatus {
    Ok,
    ProjectOpenFailed,
    ProjectLoadFailed
};

struct XQAppStartupConfig {
    std::string projectPath;
};

struct XQAppStartupState {
    XQProject project;
    XQCommandStack commandStack;
    std::string assetRootDir;
    std::string projectFilePath;
};

XQAppStartupConfig parseAppStartupArguments(int argc, char** argv);
XQAppStartupStatus initializeAppStartup(const XQAppStartupConfig& config,
                                        XQAppStartupState* state);
void attachMainWindowWorkflow(XQMainWindow* window, XQAppStartupState* state);
int runXQApp(int argc, char** argv);

} // namespace xq

#endif // XQ_APP_XQ_APP_STARTUP_H
