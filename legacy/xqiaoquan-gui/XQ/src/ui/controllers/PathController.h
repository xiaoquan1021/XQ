#ifndef XQ_UI_CONTROLLERS_PATH_CONTROLLER_H
#define XQ_UI_CONTROLLERS_PATH_CONTROLLER_H

#include "core/GeometryTypes.h"
#include "core/NodeId.h"
#include "core/XQPath.h"
#include "core/command/XQCommand.h"
#include "core/command/XQCommandStack.h"

#include <memory>
#include <string>
#include <vector>

namespace xq {

class XQScene;
class XQCommandStack;

// Thin UI-facing controller for the path stage. It collects the user's intent
// (control points + name + source image), asks PathService for a command, and
// commits it through the command stack. It never implements path algorithms and
// never mutates the scene directly -- every scene change goes through
// stack->push (the same discipline the services follow).
//
// Pure C++: depends only on core + services (no Qt / VTK), so the whole
// click -> service -> command -> scene chain is headless-testable.
class PathController {
public:
    PathController(XQScene* scene, XQCommandStack* stack);

    // What the path widget gathers before asking to create a path.
    struct AddPathIntent {
        NodeId newPathId;                        // id the caller allocates
        std::string name;
        NodeId sourceImageNode;                  // image the path derives from
        std::vector<PathControlPoint> controlPoints;
        double spacing = 1.0;                    // sample spacing along the path
    };

    enum class Status {
        Ok,
        Rejected,    // PathService rejected the intent (no scene change)
        NullScene,   // controller has no scene / stack
    };

    // A computed-but-not-committed command: prepare*() runs the service work
    // (safe on a worker thread -- pure computation over the copied intent) and
    // the caller pushes the command on the GUI thread.
    struct PreparedCommand {
        Status status = Status::Rejected;
        std::unique_ptr<XQCommand> command; // null unless status == Ok

        bool ok() const { return status == Status::Ok && command != nullptr; }
    };
    PreparedCommand prepareAddPath(const AddPathIntent& intent);

    // Pushes a prepared command on the stack (GUI-thread half of prepare*).
    bool commitPrepared(std::unique_ptr<XQCommand> command)
    {
        return stack_ != nullptr && command != nullptr
            && stack_->push(std::move(command));
    }

    // Builds the path command via PathService and pushes it on the stack. On any
    // service rejection returns Rejected and leaves the scene untouched.
    // Headless/test convenience: pushes directly to the stack and does NOT fire
    // the session sceneChanged callback; GUI code must go through the async runner
    // / session gateway. No GUI caller consumes this today (tests only).
    Status addPath(const AddPathIntent& intent);

private:
    XQScene* scene_;
    XQCommandStack* stack_;
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_PATH_CONTROLLER_H
