#include <mitkBaseApplication.h>
#include "xq_StartupSafety.h"
#include <QVariant>

int main(int argc, char* argv[])
{
    xq::startup::BackupUnsafeWorkbenchState();
    mitk::BaseApplication app(argc, argv);
    app.setApplicationName("XQ");
    app.setOrganizationName("XQ");
    app.setProperty(mitk::BaseApplication::PROP_PRODUCT, "org.xq.core.application.xqworkbench");
    app.setProperty(mitk::BaseApplication::PROP_APPLICATION, "org.xq.qt.application");
    return app.run();
}
