#ifndef XQ_UI_CONTROLLERS_MESHING_CONTROLLER_H
#define XQ_UI_CONTROLLERS_MESHING_CONTROLLER_H

#include "core/NodeId.h"
#include "core/command/XQCommand.h"
#include "core/command/XQCommandStack.h"
#include "services/meshing/SurfaceMeshService.h"
#include "services/meshing/VolumeMeshService.h"

#include <memory>
#include <string>

namespace xq {

class XQScene;
class XQCommandStack;
class ITetMesher;

// Thin UI-facing controller for the meshing stage. It reads the chosen surface
// model from its scene node payload, asks SurfaceMeshService / VolumeMeshService
// for a mesh-insertion command, and commits it through the command stack. It
// implements no meshing itself and never mutates the scene directly.
//
// Unlike the contour group, the surface model has a payload type, so the
// controller reads the model straight from its node (modelNode) rather than
// taking it on the intent -- keeping the shell thin and the source binding
// authoritative.
//
// Pure C++: depends only on core + services (no Qt / VTK).
class MeshingController {
public:
    MeshingController(XQScene* scene, XQCommandStack* stack, ITetMesher* mesher = nullptr);

    enum class Status {
        Ok,
        Rejected,        // the service rejected the input (no scene change)
        ModelNotFound,   // modelNode is missing or carries no surface model
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

    struct SurfaceMeshIntent {
        NodeId newMeshId;
        std::string name;
        NodeId modelNode;                // surface-model node to mesh
        SurfaceMeshService::Params params;
    };
    PreparedCommand prepareSurfaceMesh(const SurfaceMeshIntent& intent);
    Status buildSurfaceMesh(const SurfaceMeshIntent& intent);

    struct VolumeMeshIntent {
        NodeId newMeshId;
        std::string name;
        NodeId modelNode;                // surface-model node supplying the closed surface
        NodeId sourceNode;               // tree parent of the volume mesh (e.g. the surface-mesh node)
        VolumeMeshService::Params params;
    };
    PreparedCommand prepareVolumeMesh(const VolumeMeshIntent& intent);
    Status buildVolumeMesh(const VolumeMeshIntent& intent);

private:
    XQScene* scene_;
    XQCommandStack* stack_;
    ITetMesher* mesher_;   // borrowed (owned by the app shell); null = star fallback
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_MESHING_CONTROLLER_H
