#include "xq_UndoHelper.h"

#include <mitkUndoController.h>
#include <mitkOperationEvent.h>

void xq_UndoHelper::SubmitUndoableOperation(
    mitk::OperationActor* actor,
    mitk::Operation* doOp,
    mitk::Operation* undoOp,
    const std::string& description)
{
    auto* model = mitk::UndoController::GetCurrentUndoModel();
    if (model)
    {
        auto* event = new mitk::OperationEvent(actor, doOp, undoOp, description);
        model->SetOperationEvent(event);
    }
    actor->ExecuteOperation(doOp);
}

void xq_UndoHelper::RegisterUndoableOperation(
    mitk::OperationActor* actor,
    mitk::Operation* doOp,
    mitk::Operation* undoOp,
    const std::string& description)
{
    auto* model = mitk::UndoController::GetCurrentUndoModel();
    if (model)
    {
        auto* event = new mitk::OperationEvent(actor, doOp, undoOp, description);
        model->SetOperationEvent(event);
    }
}

bool xq_UndoHelper::CanUndo()
{
    auto* model = mitk::UndoController::GetCurrentUndoModel();
    return model != nullptr;
}

bool xq_UndoHelper::CanRedo()
{
    auto* model = mitk::UndoController::GetCurrentUndoModel();
    return model && !model->RedoListEmpty();
}

void xq_UndoHelper::ClearHistory()
{
    if (auto* model = mitk::UndoController::GetCurrentUndoModel())
        model->Clear();
}
