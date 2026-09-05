#ifndef XQ_UI_CONTROLLERS_PATH_CONTROLLER_H
#define XQ_UI_CONTROLLERS_PATH_CONTROLLER_H

#include "core/GeometryTypes.h"
#include "core/NodeId.h"
#include "core/XQPath.h"

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

    // Builds the path command via PathService and pushes it on the stack. On any
    // service rejection returns Rejected and leaves the scene untouched.
    Status addPath(const AddPathIntent& intent);

private:
    XQScene* scene_;
    XQCommandStack* stack_;
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_PATH_CONTROLLER_H
