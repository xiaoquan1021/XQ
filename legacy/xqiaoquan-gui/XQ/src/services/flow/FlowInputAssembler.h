#ifndef XQ_SERVICES_FLOW_FLOW_INPUT_ASSEMBLER_H
#define XQ_SERVICES_FLOW_FLOW_INPUT_ASSEMBLER_H

#include "core/XQFlowSmoke.h"
#include "core/XQScaleSlot.h"
#include "core/XQVesselProfile.h"
#include "services/flow/FlowSolver1D.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace xq {

struct FlowSmokeProtocolV1 {
    std::string id;
    unsigned int version = 0;
    std::string label;
    std::size_t stationCount = 0;
    double minimumLengthMm = 0.0;
    double maximumLengthMm = 0.0;
    double minimumAreaMm2 = 0.0;
    double maximumAreaMm2 = 0.0;
    FluidProperties fluid;
    std::vector<std::pair<double, double>> inletWaveform;
    double period = 0.0;
    std::vector<double> rcr;
    int numTimeSteps = 0;
    double dt = 0.0;
    int numCycles = 0;
    int maxRecordedFrames = 0;

    static FlowSmokeProtocolV1 engineeringSmoke();
    FlowSmokeProtocolStamp stamp() const;
};

class FlowInputAssembler {
public:
    static constexpr const char* AssemblerId = "xq-flow-input-assembler";
    static constexpr const char* AssemblerVersion = "1";

    enum class Status {
        Ok,
        InvalidProtocol,
        InvalidProfile,
        UnsupportedScaleForSmoke,
        GeometryOutOfEnvelope,
        InvalidResampling
    };

    struct Result {
        Status status = Status::InvalidProfile;
        VesselProfileValidationResult profileValidation;
        FlowSolver1D::SolverInput solverInput;
        FlowUnitConversionRecord conversion;
        std::vector<FlowStationSourceMapping> stationMap;

        bool ok() const { return status == Status::Ok; }
    };

    static Result assemble(const VesselProfileV1& profile,
                           ScaleSlot scaleSlot,
                           const FlowSmokeProtocolV1& protocol);
};

} // namespace xq

#endif // XQ_SERVICES_FLOW_FLOW_INPUT_ASSEMBLER_H
