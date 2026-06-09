#include "Core/xq_ApplicationContext.h"
#include "Core/xq_WorkflowRegistry.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Presentation/xq_MainWindow.h"

#include <QApplication>
#include <QAction>
#include <QListWidget>
#include <QSize>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>

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

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    window.show();
    app.processEvents();

    auto* navigation =
        window.findChild<QListWidget*>(QStringLiteral("xqWorkflowNavigation"));
    auto* pages =
        window.findChild<QStackedWidget*>(QStringLiteral("xqWorkflowPages"));
    auto* workflowSelection = context->WorkflowSelection();
    auto* viewToolbar =
        window.findChild<QToolBar*>(QStringLiteral("xqViewToolBar"));

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
    if (Expect(workflowSelection != nullptr,
               "ApplicationContext should expose workflow selection"))
    {
        delete context;
        return 1;
    }
    if (Expect(viewToolbar != nullptr,
               "Workbench shell should expose the original XQ views toolbar"))
    {
        delete context;
        return 1;
    }

    const auto& workflows = xq::core::DefaultWorkflowRegistry();
    if (Expect(workflows.size() >= 3,
               "workflow registry should provide enough test workflows"))
    {
        delete context;
        return 1;
    }

    if (Expect(workflowSelection->SelectedWorkflowId() == workflows.front().Id,
               "startup Core workflow selection should match first workflow"))
    {
        delete context;
        return 1;
    }
    if (Expect(navigation->currentRow() == 0 && pages->currentIndex() == 0,
               "startup UI workflow selection should match first workflow"))
    {
        delete context;
        return 1;
    }

    navigation->setCurrentRow(1);
    app.processEvents();
    if (Expect(workflowSelection->SelectedWorkflowId() == workflows.at(1).Id,
               "navigation changes should update Core selected workflow id"))
    {
        delete context;
        return 1;
    }
    if (Expect(pages->currentIndex() == 1,
               "navigation changes should keep page stack in sync"))
    {
        delete context;
        return 1;
    }

    if (Expect(workflowSelection->SelectWorkflow(workflows.at(2).Id),
               "programmatic Core workflow selection should accept known id"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(navigation->currentRow() == 2,
               "programmatic workflow selection should update navigation row"))
    {
        delete context;
        return 1;
    }
    if (Expect(pages->currentIndex() == 2,
               "programmatic workflow selection should update page stack"))
    {
        delete context;
        return 1;
    }

    if (Expect(!workflowSelection->SelectWorkflow(
                   QStringLiteral("missing-workflow")),
               "programmatic invalid workflow selection should be rejected"))
    {
        delete context;
        return 1;
    }
    app.processEvents();
    if (Expect(navigation->currentRow() == 2 && pages->currentIndex() == 2,
               "invalid workflow selection should leave UI state unchanged"))
    {
        delete context;
        return 1;
    }

    struct ToolbarExpectation
    {
        const char* WorkflowId;
        const char* ActionName;
    };
    const ToolbarExpectation toolbarExpectations[] = {
        {"image-preprocessing", "xqToolAction_image-preprocessing"},
        {"path", "xqToolAction_path"},
        {"segmentation-2d", "xqToolAction_segmentation-2d"},
        {"segmentation-3d", "xqToolAction_segmentation-3d"},
        {"modeling", "xqToolAction_modeling"},
        {"meshing", "xqToolAction_meshing"},
        {"flow-simulation", "xqToolAction_flow-simulation"},
    };
    for (const auto& expectation : toolbarExpectations)
    {
        const QString workflowId = QString::fromLatin1(
            expectation.WorkflowId);
        auto* action = window.findChild<QAction*>(
            QString::fromLatin1(expectation.ActionName));
        if (Expect(action != nullptr,
                   "Workbench views toolbar should expose workflow actions"))
        {
            delete context;
            return 1;
        }
        if (Expect(viewToolbar->actions().contains(action),
                   "Workflow action should belong to xqViewToolBar"))
        {
            delete context;
            return 1;
        }
        if (Expect(!action->icon().isNull(),
                   "Workflow toolbar actions should use original XQ SVG icons"))
        {
            delete context;
            return 1;
        }
        auto* button = qobject_cast<QToolButton*>(
            viewToolbar->widgetForAction(action));
        if (Expect(button != nullptr,
                   "Workflow toolbar action should own a visible Workbench tool button"))
        {
            delete context;
            return 1;
        }
        if (Expect(button->objectName() ==
                       QStringLiteral("xqToolButton_%1").arg(workflowId),
                   "Workflow tool buttons should have stable Workbench object names"))
        {
            delete context;
            return 1;
        }
        if (Expect(button->toolButtonStyle() == Qt::ToolButtonTextUnderIcon,
                   "Workflow tool buttons should use icon-over-text Workbench layout"))
        {
            delete context;
            return 1;
        }
        if (Expect(button->iconSize() == QSize(36, 36),
                   "Workflow tool buttons should restore original 36px XQ icons"))
        {
            delete context;
            return 1;
        }
        if (Expect(button->minimumWidth() >= 76 &&
                       button->maximumWidth() <= 96,
                   "Workflow tool buttons should use compact fixed widths"))
        {
            delete context;
            return 1;
        }
        const QRect buttonRect = viewToolbar->actionGeometry(action);
        if (Expect(buttonRect.isValid() &&
                       buttonRect.right() <= viewToolbar->width(),
                   "Workflow toolbar buttons should fit within the default Workbench window width"))
        {
            delete context;
            return 1;
        }

        action->trigger();
        app.processEvents();
        if (Expect(workflowSelection->SelectedWorkflowId() ==
                       workflowId,
                   "Workflow toolbar action should update Core selection"))
        {
            delete context;
            return 1;
        }
    }

    delete context;
    return 0;
}
