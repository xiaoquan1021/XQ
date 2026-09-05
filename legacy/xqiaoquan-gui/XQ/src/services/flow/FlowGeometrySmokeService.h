#ifndef XQ_SERVICES_FLOW_FLOW_GEOMETRY_SMOKE_SERVICE_H
#define XQ_SERVICES_FLOW_FLOW_GEOMETRY_SMOKE_SERVICE_H

#include "core/NodeId.h"
#include "core/XQDerivationStamp.h"
#include "core/XQFlowResult.h"
#include "core/XQScaleSlot.h"
#include "core/XQSimulationCase.h"
#include "core/XQVesselProfile.h"
#include "services/flow/FlowInputAssembler.h"

namespace xq {

class FlowGeometrySmokeService {
public:
    static constexpr const char* SolverId = "xq-flow-solver-1d";
    static constexpr const char* SolverVersion = "1";

    enum class Status {
        Ok,
        InvalidRequest,
        AssemblyFailed,
        SolverFailed,
        InvalidResult
    };

    struct Request {
        VesselProfileV1 profile;
        ScaleSlot scaleSlot = ScaleSlot::Organ;
        NodeId sourceProfileNode;
        ContentRevision sourceProfileRevision = 0;
        NodeId caseNode;
        NodeId resultNode;
        FlowSmokeProtocolV1 protocol = FlowSmokeProtocolV1::engineeringSmoke();
    };

    struct Result {
        Status status = Status::InvalidRequest;
        FlowInputAssembler::Status assemblyStatus =
            FlowInputAssembler::Status::InvalidProfile;
        VesselProfileValidationResult profileValidation;
        FlowSolver1D::Status solverStatus = FlowSolver1D::Status::EmptyCenterline;
        XQSimulationCase simulationCase;
        XQFlowResult flowResult;

        bool ok() const { return status == Status::Ok; }
    };

    static Result run(const Request& request);
};

} // namespace xq

#endif // XQ_SERVICES_FLOW_FLOW_GEOMETRY_SMOKE_SERVICE_H
