#ifndef XQ_SERVICES_FLOW_BOUNDARY_CONDITION_SERVICE_H
#define XQ_SERVICES_FLOW_BOUNDARY_CONDITION_SERVICE_H

#include "core/NodeId.h"
#include "core/XQSimulationCase.h"
#include "core/command/XQCommand.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace xq {

class XQMesh;
class XQScene;

// Pure domain service that validates and binds a simulation case's boundary
// conditions against the authoritative mesh boundary-face list (XQMesh M4), and
// parses inlet flow-rate waveform files. Zero external dependencies (no Qt / VTK
// / ITK / solver / Python): only xq_core. Like the other services it computes
// results and (for scene insertion) returns a command; it never mutates the
// scene and holds no scene state. All quantities CGS.
class BoundaryConditionService {
public:
    enum class Status {
        Ok,
        MissingFace,    // a BC references a face id absent from the mesh
        MissingInlet,   // no inlet BC (InletFlowWaveform / PrescribedVelocity)
        MissingOutlet,  // no outlet BC (RCR / Resistance / PrescribedPressure)
        InvalidRcr,     // an RCR BC has rcr.size != 3 or a non-positive Rp/C/Rd
        EmptyWaveform,  // an inlet waveform BC has empty waveform or period <= 0
        NullScene,      // scene pointer was null (command path only)
    };

    struct Result {
        Status status;
        XQSimulationCase validatedCase; // meaningful only when status == Ok

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

    // Parses a two-column "t Q" flow file (whitespace-separated). Blank lines and
    // lines that do not start with a parseable number pair are skipped (so
    // comment / header lines are tolerated). Pure string parsing, no I/O.
    static std::vector<std::pair<double, double>> parseFlowFile(const std::string& text);

    // Validates `base`'s boundary conditions against `mesh`'s boundary faces and
    // returns the validated case (unchanged on success; the case already carries
    // its BCs). Checks: every BC.faceId exists in the mesh; at least one inlet and
    // one outlet role; RCR params are a positive triple; inlet waveform non-empty
    // with positive period. Returns a diagnostic status on failure (never throws).
    static Result validateAndBind(const XQSimulationCase& base, const XQMesh& mesh);

    // Builds the semantic command that writes `validated` back onto an existing
    // case node, advances contentRevision and invalidates downstream results.
    // Validation runs first; failure statuses pass through with a null command.
    static CommandResult bindCommand(XQScene* scene,
                                     const NodeId& caseNodeId,
                                     const XQSimulationCase& base,
                                     const XQMesh& mesh,
                                     const std::string& label = "Bind boundary conditions");
};

} // namespace xq

#endif // XQ_SERVICES_FLOW_BOUNDARY_CONDITION_SERVICE_H
