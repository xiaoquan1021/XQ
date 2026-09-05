#include "core/command/XQCommandStack.h"

#include <utility>

namespace xq {

bool XQCommandStack::push(std::unique_ptr<XQCommand> command)
{
    if (!command) {
        return false;
    }

    if (!command->execute()) {
        return false;
    }
    undo_stack_.push_back(std::move(command));
    redo_stack_.clear();
    return true;
}

bool XQCommandStack::can_undo() const
{
    return !undo_stack_.empty();
}

bool XQCommandStack::can_redo() const
{
    return !redo_stack_.empty();
}

bool XQCommandStack::undo()
{
    if (undo_stack_.empty()) {
        return false;
    }

    std::unique_ptr<XQCommand> command = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    command->undo();
    redo_stack_.push_back(std::move(command));
    return true;
}

bool XQCommandStack::redo()
{
    if (redo_stack_.empty()) {
        return false;
    }

    std::unique_ptr<XQCommand> command = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    if (!command->execute()) {
        // Put the command back: a failed redo must stay retryable, not vanish.
        redo_stack_.push_back(std::move(command));
        return false;
    }
    undo_stack_.push_back(std::move(command));
    return true;
}

void XQCommandStack::clear()
{
    undo_stack_.clear();
    redo_stack_.clear();
}

std::size_t XQCommandStack::undo_count() const
{
    return undo_stack_.size();
}

std::size_t XQCommandStack::redo_count() const
{
    return redo_stack_.size();
}

} // namespace xq
