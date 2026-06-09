#include "Core/xq_ApplicationContext.h"
#include "Core/xq_ScreenshotFilePathProvider.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

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

class FakeScreenshotPathProvider
    : public xq::core::ScreenshotFilePathProvider
{
public:
    QString NextPath;
    int Requests = 0;

    QString ScreenshotFilePath() override
    {
        ++Requests;
        return NextPath;
    }
};

QAction* FindAction(xq::presentation::MainWindow& window,
                    const QString& objectName)
{
    return window.findChild<QAction*>(objectName);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    FakeScreenshotPathProvider provider;
    provider.NextPath =
        QDir(tempDir.path()).filePath(QStringLiteral("xq-screenshot.png"));
    window.SetScreenshotFilePathProvider(&provider);
    window.resize(960, 640);
    window.show();
    app.processEvents();

    auto* screenshotAction =
        FindAction(window, QStringLiteral("xqScreenshotAction"));
    if (Expect(screenshotAction != nullptr,
               "Screenshot action should exist"))
    {
        delete context;
        return 1;
    }

    QStringList diagnostics;
    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&diagnostics](const QString& message) {
                         diagnostics.append(message);
                     });

    screenshotAction->trigger();
    app.processEvents();

    QFile screenshot(provider.NextPath);
    if (Expect(provider.Requests == 1,
               "Screenshot action should request an output file path"))
    {
        delete context;
        return 1;
    }
    if (Expect(screenshot.exists() && screenshot.size() > 0,
               "Screenshot action should write a non-empty PNG file"))
    {
        delete context;
        return 1;
    }
    if (Expect(!diagnostics.contains(QStringLiteral(
                   "Screenshot is not available in Windows monolith v1.")),
               "Screenshot action should no longer report unavailable"))
    {
        delete context;
        return 1;
    }
    if (Expect(diagnostics.contains(QStringLiteral("Screenshot saved.")),
               "Screenshot action should report a saved diagnostic"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
