#include "xq_MonolithApplication.h"

#include <QApplication>
#include <QFile>
#include <QIcon>

#include <iostream>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setStyleSheet(QString());
    app.setWindowIcon(QIcon());

    QString errorMessage;
    if (Expect(xq::ApplyXqWorkbenchTheme(app, &errorMessage),
               "Monolith app should load the original XQ Workbench theme"))
        return 1;
    if (Expect(errorMessage.isEmpty(),
               "Theme loading should not report an error on success"))
        return 1;

    QFile styleFile(QStringLiteral(":/xq/xq.qss"));
    if (Expect(styleFile.exists(),
               "Original XQ Workbench stylesheet should be embedded as a Qt resource"))
        return 1;
    if (Expect(styleFile.open(QIODevice::ReadOnly | QIODevice::Text),
               "Embedded XQ stylesheet should be readable"))
        return 1;

    const QString styleSheet = QString::fromUtf8(styleFile.readAll());
    if (Expect(styleSheet.contains(QStringLiteral("QDockWidget::title")),
               "Embedded XQ stylesheet should include Workbench dock styling"))
        return 1;
    if (Expect(styleSheet.contains(QStringLiteral("QToolBar#mainActionsToolBar")),
               "Embedded XQ stylesheet should include original toolbar styling"))
        return 1;

    if (Expect(app.styleSheet().contains(QStringLiteral("QDockWidget::title")),
               "Applied QApplication stylesheet should include Workbench dock styling"))
        return 1;
    if (Expect(!QIcon(QStringLiteral(":/xq/icon.png")).isNull(),
               "Original XQ application icon should be embedded"))
        return 1;
    if (Expect(!QIcon(QStringLiteral(":/xq/tool-path.svg")).isNull(),
               "Original XQ workflow toolbar icons should be embedded"))
        return 1;
    if (Expect(!app.windowIcon().isNull(),
               "Applied Workbench theme should set the application icon"))
        return 1;

    return 0;
}
