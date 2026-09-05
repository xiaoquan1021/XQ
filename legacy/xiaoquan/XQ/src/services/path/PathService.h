#ifndef XQ_SERVICES_PATH_PATH_SERVICE_H
#define XQ_SERVICES_PATH_PATH_SERVICE_H

#include "core/GeometryTypes.h"
#include "core/NodeId.h"
#include "core/XQPath.h"
#include "core/command/XQCommand.h"

#include <memory>
#include <string>
#include <vector>

namespace xq {

class XQDataNode;
class XQScene;

// Pure domain service for blood-vessel paths. Computes "how the scene should
// change" and returns a command (one of the M0 scene commands); it never
// mutates the scene and holds no scene state. The caller owns the scene and the
// command stack that executes / undoes the returned command.
//
// Validation failures return a Status with no command (Result::command == null)
// instead of throwing, so errors do not propagate exceptions through UI code.
class PathService {
public:
    enum class Status {
        Ok,
        NotEnoughControlPoints, // create / delete leaves fewer than 2 points
        InvalidSpacing,         // sample spacing must be > 0
        NotAPathNode,           // edited node is not a Path node with an XQPathPayload
        IndexOutOfRange,        // move / delete index, or insert index > size
        ResampleFailed,         // XQPath::resample reported a failure
        NullScene,              // scene pointer was null
    };

    struct Result {
        Status status;
        std::unique_ptr<XQCommand> command; // null unless status == Ok

        bool ok() const
        {
            return status == Status::Ok;
        }
    };

    // Creates a new path node derived from the source image. The caller assigns
    // newPathId (PathService holds no id allocator). The returned command, when
    // executed, inserts the node and links it as derived from sourceImageNodeId.
    static Result createPathCommand(XQScene* scene,
                                    const NodeId& newPathId,
                                    const std::string& name,
                                    const NodeId& sourceImageNodeId,
                                    const std::vector<PathControlPoint>& controlPoints,
                                    double spacing);

    // Edit commands copy the node's existing XQPath payload, change one thing,
    // re-resample, and return a ReplacePayloadCommand that preserves the path id
    // and source image binding.
    static Result moveControlPointCommand(XQScene* scene,
                                          const XQDataNode& node,
                                          std::size_t index,
                                          const Point3& newPosition,
                                          double spacing);

    static Result insertControlPointCommand(XQScene* scene,
                                            const XQDataNode& node,
                                            std::size_t index,
                                            const Point3& point,
                                            double spacing);

    static Result deleteControlPointCommand(XQScene* scene,
                                            const XQDataNode& node,
                                            std::size_t index,
                                            double spacing);

    static Result resamplePathCommand(XQScene* scene,
                                      const XQDataNode& node,
                                      double spacing);
};

} // namespace xq

#endif // XQ_SERVICES_PATH_PATH_SERVICE_H
