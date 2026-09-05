#include "app/XQAppStartup.h"
#include "app/XQMainWindow.h"
#include "visualization/XQVolumeViewWidget.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QStringList>

namespace {

// Installs a Chinese-capable UI font. "Microsoft YaHei" already resolves by name
// on a normal Windows desktop, but to be robust across font-backend quirks we
// also register msyh.ttc explicitly via addApplicationFont and use whatever
// family name that yields. Falls back to the by-name font if the file is absent.
void installUiFont(QApplication& app)
{
    QString family = QStringLiteral("Microsoft YaHei");
    const int id = QFontDatabase::addApplicationFont(
        QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
    if (id >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty()) {
            family = families.first();
        }
    }
    app.setFont(QFont(family, 9));
}

} // namespace

int main(int argc, char** argv)
{
    xq::XQVolumeViewWidget::configureDefaultSurfaceFormat();

    QApplication app(argc, argv);

    // The theme + icons live in resources/xq_resources.qrc, compiled into the
    // xq_app_shell static library. Qt does not auto-register resources linked
    // from a static lib, so initialise them explicitly before use.
    Q_INIT_RESOURCE(xq_resources);

    // Default UI font: a Chinese-capable family so Han glyphs resolve (Segoe UI
    // has no CJK glyphs -> tofu boxes for Chinese). YaHei carries Latin glyphs
    // too, so English stays correct.
    installUiFont(app);

    // i18n note: the Chinese translator is owned and installed by XQMainWindow
    // (a single QTranslator instance, installed in its constructor for the
    // default-Chinese state and toggled by the View > Language menu). Keeping it
    // in one place avoids a second competing instance here that the window's
    // removeTranslator() could not take back -- which would strand the UI in
    // Chinese when the user picked English. The window is built before show()
    // below, so the first painted frame is already Chinese; no English flash.

    // Apply the bundled XQ light theme. Silently skips if unavailable.
    QFile styleFile(QStringLiteral(":/xq/xq.qss"));
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        styleFile.close();
    }

    // Project/asset bootstrap (optional argv[1] project path, lazy geometry)
    // lives in XQAppStartup so tests can drive it headlessly.
    xq::XQAppStartupState state;
    const xq::XQAppStartupStatus status =
        xq::initializeAppStartup(xq::parseAppStartupArguments(argc, argv), &state);
    if (status != xq::XQAppStartupStatus::Ok) {
        return 1;
    }

    xq::XQMainWindow window;
    xq::attachMainWindowWorkflow(&window, &state);
    window.show();

    return app.exec();
}
