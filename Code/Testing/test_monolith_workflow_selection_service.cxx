#include "Core/xq_ApplicationContext.h"
#include "Core/xq_WorkflowRegistry.h"
#include "Core/xq_WorkflowSelectionService.h"

#include <QCoreApplication>
#include <QObject>

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
    QCoreApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    auto* workflowSelection = context->WorkflowSelection();

    if (Expect(workflowSelection != nullptr,
               "ApplicationContext should expose WorkflowSelectionService"))
    {
        delete context;
        return 1;
    }

    const auto& workflows = xq::core::DefaultWorkflowRegistry();
    if (Expect(workflows.size() >= 2,
               "workflow registry should provide enough test workflows"))
    {
        delete context;
        return 1;
    }

    if (Expect(workflowSelection->SelectedWorkflowId() == workflows.front().Id,
               "default selected workflow should be first registry id"))
    {
        delete context;
        return 1;
    }

    int workflowChanges = 0;
    QString lastWorkflowId;
    QObject::connect(workflowSelection,
                     &xq::core::WorkflowSelectionService::WorkflowChanged,
                     [&workflowChanges,
                      &lastWorkflowId](const QString& workflowId) {
                         ++workflowChanges;
                         lastWorkflowId = workflowId;
                     });

    const QString secondWorkflowId = workflows.at(1).Id;
    if (Expect(workflowSelection->SelectWorkflow(secondWorkflowId),
               "known workflow id should be selectable"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowSelection->SelectedWorkflowId() == secondWorkflowId,
               "valid workflow selection should update selected id"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowChanges == 1,
               "valid workflow selection should emit one change"))
    {
        delete context;
        return 1;
    }
    if (Expect(lastWorkflowId == secondWorkflowId,
               "WorkflowChanged should emit selected workflow id"))
    {
        delete context;
        return 1;
    }

    if (Expect(workflowSelection->SelectWorkflow(secondWorkflowId),
               "re-selecting a known workflow should still succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowChanges == 1,
               "re-selecting current workflow should not emit a change"))
    {
        delete context;
        return 1;
    }

    if (Expect(!workflowSelection->SelectWorkflow(
                   QStringLiteral("missing-workflow")),
               "unknown workflow id should be rejected"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowSelection->SelectedWorkflowId() == secondWorkflowId,
               "invalid workflow selection should not mutate selected id"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowChanges == 1,
               "invalid workflow selection should not emit a change"))
    {
        delete context;
        return 1;
    }

    if (Expect(!workflowSelection->SelectWorkflow(QString()),
               "empty workflow id should be rejected"))
    {
        delete context;
        return 1;
    }
    if (Expect(workflowSelection->SelectedWorkflowId() == secondWorkflowId,
               "empty workflow id should not mutate selected id"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
