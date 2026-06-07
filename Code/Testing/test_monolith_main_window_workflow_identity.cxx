#include "Core/xq_ApplicationContext.h"
#include "Core/xq_WorkflowRegistry.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QListWidget>
#include <QStackedWidget>
#include <QWidget>

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

QString WorkflowPageObjectName(const QString& workflowId)
{
    return QStringLiteral("xqWorkflowPage_%1").arg(workflowId);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);

    auto* navigation =
        window.findChild<QListWidget*>(QStringLiteral("xqWorkflowNavigation"));
    auto* pages =
        window.findChild<QStackedWidget*>(QStringLiteral("xqWorkflowPages"));

    if (Expect(navigation != nullptr,
               "MainWindow should expose workflow navigation"))
    {
        delete context;
        return 1;
    }
    if (Expect(pages != nullptr,
               "MainWindow should expose workflow pages"))
    {
        delete context;
        return 1;
    }

    const auto& workflows = xq::core::DefaultWorkflowRegistry();
    if (Expect(navigation->count() == static_cast<int>(workflows.size()),
               "workflow navigation count should match registry"))
    {
        delete context;
        return 1;
    }
    if (Expect(pages->count() == static_cast<int>(workflows.size()),
               "workflow page count should match registry"))
    {
        delete context;
        return 1;
    }
    if (Expect(navigation->currentRow() == 0 &&
                   pages->currentIndex() == 0,
               "startup selection should land on the first workflow"))
    {
        delete context;
        return 1;
    }

    for (int row = 0; row < static_cast<int>(workflows.size()); ++row)
    {
        const auto& workflow = workflows.at(row);
        auto* item = navigation->item(row);
        if (Expect(item != nullptr,
                   "workflow navigation item should exist"))
        {
            delete context;
            return 1;
        }
        if (Expect(item->text() == workflow.Title,
                   "workflow navigation item should keep registry title"))
        {
            delete context;
            return 1;
        }
        if (Expect(item->data(Qt::UserRole).toString() == workflow.Id,
                   "workflow navigation item should store registry id"))
        {
            delete context;
            return 1;
        }

        auto* page = pages->widget(row);
        if (Expect(page != nullptr, "workflow page should exist"))
        {
            delete context;
            return 1;
        }
        if (Expect(page->objectName() ==
                       WorkflowPageObjectName(workflow.Id),
                   "workflow page should expose a stable object name"))
        {
            delete context;
            return 1;
        }

        navigation->setCurrentRow(row);
        app.processEvents();
        if (Expect(pages->currentIndex() == row,
                   "workflow navigation should switch to matching page"))
        {
            delete context;
            return 1;
        }
    }

    delete context;
    return 0;
}
