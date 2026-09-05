#ifndef XQ_CORE_COMMAND_XQ_COMMAND_H
#define XQ_CORE_COMMAND_XQ_COMMAND_H

#include <string>

namespace xq {

// Abstract scene mutation. A service computes "how the scene should change" and
// returns one of these; execute() applies it, undo() restores the prior state.
// All scene changes go through the command stack (see command-and-scene.md).
class XQCommand {
public:
    virtual ~XQCommand() = default;

    virtual bool execute() = 0;
    virtual void undo() = 0;
    virtual std::string label() const = 0;
};

} // namespace xq

#endif // XQ_CORE_COMMAND_XQ_COMMAND_H
