#include "Core/xq_ApplicationContext.h"

#include <QCoreApplication>
#include <QObject>

#include <mitkDataNode.h>

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

bool SameNode(const mitk::DataNode::Pointer& lhs,
              const mitk::DataNode::Pointer& rhs)
{
    return lhs.GetPointer() == rhs.GetPointer();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    int selectionSignals = 0;
    mitk::DataNode::Pointer lastSelectedNode;

    QObject::connect(context,
                     &xq::core::ApplicationContext::SelectionChanged,
                     [&selectionSignals,
                      &lastSelectedNode](mitk::DataNode::Pointer node) {
                         ++selectionSignals;
                         lastSelectedNode = node;
                     });

    if (Expect(context->ActiveNode().IsNull(),
               "new ApplicationContext should not have an active node"))
    {
        delete context;
        return 1;
    }

    auto node = mitk::DataNode::New();
    context->SetActiveNode(node);

    if (Expect(SameNode(context->ActiveNode(), node),
               "SetActiveNode should store the selected node"))
    {
        delete context;
        return 1;
    }
    if (Expect(selectionSignals == 1,
               "SetActiveNode should emit one selection signal"))
    {
        delete context;
        return 1;
    }
    if (Expect(SameNode(lastSelectedNode, node),
               "SelectionChanged should emit the selected node"))
    {
        delete context;
        return 1;
    }

    context->SetActiveNode(node);
    if (Expect(selectionSignals == 1,
               "setting the same active node should be a no-op"))
    {
        delete context;
        return 1;
    }

    context->ClearActiveNode();
    if (Expect(context->ActiveNode().IsNull(),
               "ClearActiveNode should clear the selected node"))
    {
        delete context;
        return 1;
    }
    if (Expect(selectionSignals == 2,
               "ClearActiveNode should emit one selection signal"))
    {
        delete context;
        return 1;
    }
    if (Expect(lastSelectedNode.IsNull(),
               "SelectionChanged should emit null when selection is cleared"))
    {
        delete context;
        return 1;
    }

    context->ClearActiveNode();
    if (Expect(selectionSignals == 2,
               "clearing an empty selection should be a no-op"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
