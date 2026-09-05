#include "xq_MitkApp.h"

#include <ctkPluginFrameworkLauncher.h>

#include <QDir>
#include <QStringList>
#include <cstdlib>

xq_MitkApp::xq_MitkApp(int argc, char** argv)
    : mitk::BaseApplication(argc, argv)
{
}

xq_MitkApp::~xq_MitkApp()
{
}

void xq_MitkApp::initializeLibraryPaths()
{
    // Read plugin paths from XQ_PLUGIN_PATH environment variable
    const char* pluginPathEnv = getenv("XQ_PLUGIN_PATH");
    if (pluginPathEnv != nullptr)
    {
        QString pluginPaths(pluginPathEnv);

#ifdef _WIN32
        QStringList pathList = pluginPaths.split(";", Qt::SkipEmptyParts);
#else
        QStringList pathList = pluginPaths.split(":", Qt::SkipEmptyParts);
#endif

        for (const QString& path : pathList)
        {
            ctkPluginFrameworkLauncher::addSearchPath(path);
        }
    }
    else
    {
        // Default search paths for plugin libraries
        ctkPluginFrameworkLauncher::addSearchPath("plugins");

#ifdef _WIN32
        ctkPluginFrameworkLauncher::addSearchPath("bin/plugins");
#else
        ctkPluginFrameworkLauncher::addSearchPath("lib/plugins");
#endif

#ifdef __APPLE__
        ctkPluginFrameworkLauncher::addSearchPath("../../plugins");
#endif
    }
}
