#ifndef XQ_UI_CONTROLLERS_MODELING_CONTROLLER_H
#define XQ_UI_CONTROLLERS_MODELING_CONTROLLER_H

#include "core/NodeId.h"
#include "core/XQContourGroup.h"
#include "core/command/XQCommand.h"
#include "core/command/XQCommandStack.h"
#include "services/modeling/ContourLoftInputBuilder.h"
#include "services/modeling/ModelingService.h"

#include <memory>
#include <string>

namespace xq {

class XQScene;
class XQCommandStack;

// Thin UI-facing controller for the modeling stage. It takes the chosen contour
// group, prepares the loft input (ContourLoftInputBuilder), lofts the surface
// and (optionally) caps the open ends through ModelingService, then commits the
// model-insertion command through the command stack. It implements no geometry
// itself and never mutates the scene directly.
//
// The contour group is passed as a value on the intent rather than read from a
// scene payload: contour groups have no payload type in the current core (they
// are produced by the reader / segmentation stage), so the controller takes the
// prepared domain object. The contour-group node id is carried separately so the
// lofted model binds back to it as source in the tree.
//
// Pure C++: depends only on core + services (no Qt / VTK).
class ModelingController {
public:
    ModelingController(XQScene* scene, XQCommandStack* stack);

    enum class Status {
        Ok,
        Rejected,    // loft / cap / command construction failed (no scene change)
        NullScene,
    };

    // A computed-but-not-committed command: prepare*() runs the service work
    // (safe on a worker thread -- pure computation over the copied intent) and
    // the caller pushes the command on the GUI thread.
    struct PreparedCommand {
        Status status = Status::Rejected;
        std::unique_ptr<XQCommand> command; // null unless status == Ok

        bool ok() const { return status == Status::Ok && command != nullptr; }
    };

    // Pushes a prepared command on the stack (GUI-thread half of prepare*).
    bool commitPrepared(std::unique_ptr<XQCommand> command)
    {
        return stack_ != nullptr && command != nullptr
            && stack_->push(std::move(command));
    }

    struct LoftIntent {
        NodeId newModelId;
        std::string name;
        NodeId contourGroupNode;          // source binding for the model
        XQContourGroup contourGroup;      // the rings to loft
        ContourLoftInputBuilder::Options loftOptions; // pointsPerContour = 0 -> largest
        bool capEnds = true;              // cap the open ends into a closed surface
        ModelingService::CapOptions capOptions;
    };

    // Lofts (and optionally caps) the contour group into a surface model and
    // pushes the model-insertion command. Returns Rejected on any service
    // failure, leaving the scene untouched.
    PreparedCommand prepareLoft(const LoftIntent& intent);
    Status loft(const LoftIntent& intent);

private:
    XQScene* scene_;
    XQCommandStack* stack_;
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_MODELING_CONTROLLER_H
