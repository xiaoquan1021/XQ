#include "Core/xq_ApplicationContext.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QMenu>
#include <QTextEdit>

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

QAction* FindAction(xq::presentation::MainWindow& window,
                    const QString& objectName)
{
    return window.findChild<QAction*>(objectName);
}

QMenu* FindMenu(xq::presentation::MainWindow& window,
                const QString& objectName)
{
    return window.findChild<QMenu*>(objectName);
}

QDialog* FindDialog(xq::presentation::MainWindow& window,
                    const QString& objectName)
{
    return window.findChild<QDialog*>(objectName);
}

bool MenuContainsAction(QMenu* menu, QAction* action)
{
    return menu && action && menu->actions().contains(action);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    window.show();
    app.processEvents();

    auto* helpMenu = FindMenu(window, QStringLiteral("HelpMenu"));
    auto* welcomeAction =
        FindAction(window, QStringLiteral("xqWelcomeAction"));
    auto* aboutAction =
        FindAction(window, QStringLiteral("xqAboutAction"));

    if (Expect(helpMenu != nullptr &&
                   helpMenu->title() == QStringLiteral("&Help"),
               "Workbench menu bar should expose the original Help menu"))
    {
        delete context;
        return 1;
    }
    if (Expect(welcomeAction != nullptr &&
                   welcomeAction->text().remove(QLatin1Char('&')) ==
                       QStringLiteral("Welcome") &&
                   MenuContainsAction(helpMenu, welcomeAction),
               "Help menu should expose the original Welcome action"))
    {
        delete context;
        return 1;
    }
    if (Expect(aboutAction != nullptr &&
                   aboutAction->text().remove(QLatin1Char('&')) ==
                       QStringLiteral("About XQ") &&
                   MenuContainsAction(helpMenu, aboutAction),
               "Help menu should expose the original About XQ action"))
    {
        delete context;
        return 1;
    }

    welcomeAction->trigger();
    app.processEvents();
    auto* welcomeDialog =
        FindDialog(window, QStringLiteral("xqWelcomeDialog"));
    auto* welcomeText = welcomeDialog
                            ? welcomeDialog->findChild<QLabel*>(
                                  QStringLiteral("xqWelcomeText"))
                            : nullptr;
    if (Expect(welcomeDialog != nullptr &&
                   welcomeDialog->windowTitle() ==
                       QStringLiteral("Welcome to XQ") &&
                   welcomeText != nullptr &&
                   welcomeText->text().contains(
                       QStringLiteral("cardiovascular analysis"),
                       Qt::CaseInsensitive),
               "Welcome action should open the monolith Welcome dialog"))
    {
        delete context;
        return 1;
    }

    aboutAction->trigger();
    app.processEvents();
    auto* aboutDialog =
        FindDialog(window, QStringLiteral("xqAboutDialog"));
    auto* aboutCaption = aboutDialog
                             ? aboutDialog->findChild<QLabel*>(
                                   QStringLiteral("xqAboutCaption"))
                             : nullptr;
    auto* aboutVersion = aboutDialog
                             ? aboutDialog->findChild<QLabel*>(
                                   QStringLiteral("xqAboutVersion"))
                             : nullptr;
    auto* aboutDescription = aboutDialog
                                 ? aboutDialog->findChild<QTextEdit*>(
                                       QStringLiteral("xqAboutDescription"))
                                 : nullptr;
    if (Expect(aboutDialog != nullptr &&
                   aboutDialog->windowTitle() ==
                       QStringLiteral("About XQ") &&
                   aboutCaption != nullptr &&
                   aboutCaption->text() == QStringLiteral("XQ") &&
                   aboutVersion != nullptr &&
                   aboutVersion->text().contains(
                       QStringLiteral("Version")) &&
                   aboutDescription != nullptr &&
                   aboutDescription->toPlainText().contains(
                       QStringLiteral("Cardiovascular Analysis")) &&
                   aboutDescription->toPlainText().contains(
                       QStringLiteral("Qt")),
               "About action should open the monolith About XQ dialog"))
    {
        delete context;
        return 1;
    }

    welcomeAction->trigger();
    aboutAction->trigger();
    app.processEvents();
    if (Expect(window.findChildren<QDialog*>(
                   QStringLiteral("xqWelcomeDialog")).size() == 1 &&
                   window.findChildren<QDialog*>(
                       QStringLiteral("xqAboutDialog")).size() == 1,
               "Help actions should reuse existing Help dialogs"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
