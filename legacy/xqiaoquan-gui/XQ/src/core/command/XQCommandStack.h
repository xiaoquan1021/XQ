#ifndef XQ_CORE_COMMAND_XQ_COMMAND_STACK_H
#define XQ_CORE_COMMAND_XQ_COMMAND_STACK_H

#include "core/command/XQCommand.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace xq {

// Undo/redo stack for scene mutations. push() executes a command and records it
// for undo only when execution succeeds; undo() reverts the most recent command
// and moves it onto the redo stack; redo() re-executes it. Any successful new
// push() clears the redo stack.
class XQCommandStack {
public:
    // Executes the command and pushes it onto the undo stack on success.
    bool push(std::unique_ptr<XQCommand> command);

    bool can_undo() const;
    bool can_redo() const;

    // Returns false when there is nothing to undo / redo.
    bool undo();
    bool redo();

    void clear();

    std::size_t undo_count() const;
    std::size_t redo_count() const;

private:
    std::vector<std::unique_ptr<XQCommand>> undo_stack_;
    std::vector<std::unique_ptr<XQCommand>> redo_stack_;
};

} // namespace xq

#endif // XQ_CORE_COMMAND_XQ_COMMAND_STACK_H
