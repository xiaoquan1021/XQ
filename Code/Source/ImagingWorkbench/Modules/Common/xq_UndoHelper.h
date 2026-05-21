#pragma once

#include <xqModuleCommonExports.h>
#include <mitkOperation.h>
#include <mitkOperationActor.h>
#include <string>

class XQMODULECOMMON_EXPORT xq_UndoHelper
{
public:
    // Submit an undoable operation pair to the global undo stack
    static void SubmitUndoableOperation(
        mitk::OperationActor* actor,
        mitk::Operation* doOp,
        mitk::Operation* undoOp,
        const std::string& description);

    // Register already-executed operation for undo (does NOT re-execute doOp)
    static void RegisterUndoableOperation(
        mitk::OperationActor* actor,
        mitk::Operation* doOp,
        mitk::Operation* undoOp,
        const std::string& description);

    // Check if undo/redo is available
    static bool CanUndo();
    static bool CanRedo();

    // Clear undo history
    static void ClearHistory();

private:
    xq_UndoHelper() = delete;
};
