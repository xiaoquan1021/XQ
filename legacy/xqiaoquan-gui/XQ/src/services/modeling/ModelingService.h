#ifndef XQ_SERVICES_MODELING_MODELING_SERVICE_H
#define XQ_SERVICES_MODELING_MODELING_SERVICE_H

#include "core/NodeId.h"
#include "core/command/XQCommand.h"
#include "services/modeling/ContourLoftInputBuilder.h"

#include <memory>
#include <string>

namespace xq {

class XQSurfaceModel;
class XQScene;

// Pure domain service for building surface models from lofted contours and for
// capping the open ends of a vessel model. Like the M1/M2 services it computes
// results and (for scene insertion) returns a command; it never mutates the
// scene and holds no scene state. The triangulation is XQ's own code with zero
// external dependencies (no VTK/OCCT/Qt). Loft / cap semantics follow
// SimVascular / MITK (adjacent-ring stitching; centroid fan capping; boundary
// edges = edges used by exactly one triangle).
//
// Model-producing methods return a Result carrying a status and a shared model
// (null on validation failure) instead of throwing.
class ModelingService {
public:
    enum class Status {
        Ok,
        NotEnoughRings,        // loft input has fewer than 2 rings
        InconsistentRings,     // rings disagree on point count, or count < 3
        InvalidModel,          // model has no usable triangle geometry
        AlreadyClosed,         // cap requested but the model has no open boundary
        NullScene,             // scene pointer was null
    };

    struct Result {
        Status status;
        std::shared_ptr<XQSurfaceModel> model; // null unless status == Ok

        bool ok() const
        {
            return status == Status::Ok;
        }
    };

    struct CommandResult {
        Status status;
        std::unique_ptr<XQCommand> command; // null unless status == Ok

        bool ok() const
        {
            return status == Status::Ok;
        }
    };

    struct CapOptions {
        // Stable face ids assigned to the generated caps. Defaults match the
        // wall face id (1) used by loftSurface: wall=1, inlet=2, outlet=3.
        int inletFaceId = 2;
        int outletFaceId = 3;
    };

    // Stable wall face id assigned by loftSurface.
    static int wallFaceId();

    // Stitches adjacent rings into a triangle strip (each ring N points; each
    // adjacent pair contributes 2*N triangles), producing an open tube surface
    // with a single Wall ModelFace (id == wallFaceId()). The model is marked
    // Generated and bound to input.sourceContourGroup. The ends are left open
    // for capModel.
    static Result loftSurface(const XQContourLoftInput& input);

    // Caps every open boundary loop of the model (a boundary edge is one used by
    // exactly one triangle) by centroid-fan triangulation, closing the surface.
    // The loops are ordered by ascending centroid position along the model's
    // first principal extent so the inlet (first) and outlet (last) are stable;
    // each gets an Inlet/Outlet ModelFace with a stable face id and cap id.
    // Returns a new model; the input is not mutated.
    static Result capModel(const XQSurfaceModel& model, const CapOptions& options);

    // Builds the command that inserts a model node, linking it as derived from
    // the source contour group when one is bound (AddNodeWithSourceRelation),
    // else a plain AddNodeCommand. The caller assigns newModelId.
    static CommandResult loftSurfaceCommand(XQScene* scene,
                                            const NodeId& newModelId,
                                            const std::string& name,
                                            const XQContourLoftInput& input);

    // Same as loftSurfaceCommand but inserts an already-built (e.g. capped)
    // model, preserving its source contour-group binding.
    static CommandResult createModelNodeCommand(XQScene* scene,
                                                const NodeId& newModelId,
                                                const std::string& name,
                                                const std::shared_ptr<XQSurfaceModel>& model);
};

} // namespace xq

#endif // XQ_SERVICES_MODELING_MODELING_SERVICE_H
