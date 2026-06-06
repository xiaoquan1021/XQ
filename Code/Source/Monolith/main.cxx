#include "Core/xq_ApplicationContext.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QSurfaceFormat>

#include <QVTKOpenGLNativeWidget.h>

#include <memory>

int main(int argc, char* argv[])
{
    QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("XQ"));
    QCoreApplication::setOrganizationName(QStringLiteral("XQ"));

    std::unique_ptr<xq::core::ApplicationContext> context(
        xq::core::ApplicationContext::CreateDefault());

    xq::presentation::MainWindow window(*context);
    window.show();

    return app.exec();
}
