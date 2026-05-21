#include "xq_Main.h"
#include "xq_MitkApp.h"

#include <QStringList>
#include <QVariant>

int xq_Main::Launch(int argc, char* argv[],
                    bool useProvisioningFile, bool useWorkbench)
{
    xq_MitkApp app(argc, argv);

    app.setSingleMode(true);
    app.setApplicationName("XQ");
    app.setOrganizationName("XQ");
    static_cast<mitk::BaseApplication&>(app).setProperty(
        mitk::BaseApplication::PROP_APPLICATION,
        QVariant(QString("org.xq.qt.application")));

    if (useWorkbench)
    {
        // Use explicit QVariant to avoid ambiguity with QCoreApplication::setProperty
        static_cast<mitk::BaseApplication&>(app).setProperty(
            mitk::BaseApplication::PROP_PRODUCT,
            QVariant(QString("org.xq.core.application.xqworkbench")));
    }

    app.setPreloadLibraries(QStringList() << "liborg_mitk_gui_qt_ext");

    return app.run();
}
