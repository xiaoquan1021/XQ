#include "services/flow/FlowInputAssembler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace xq {
namespace {

constexpr double kLengthMmToCm = 0.1;
constexpr double kAreaMm2ToCm2 = 0.01;

bool same_protocol(const FlowSmokeProtocolV1& left,
                   const FlowSmokeProtocolV1& right)
{
    return left.id == right.id
        && left.version == right.version
        && left.label == right.label
        && left.stationCount == right.stationCount
        && left.minimumLengthMm == right.minimumLengthMm
        && left.maximumLengthMm == right.maximumLengthMm
        && left.minimumAreaMm2 == right.minimumAreaMm2
        && left.maximumAreaMm2 == right.maximumAreaMm2
        && left.fluid.density == right.fluid.density
        && left.fluid.viscosity == right.fluid.viscosity
        && left.inletWaveform == right.inletWaveform
        && left.period == right.period
        && left.rcr == right.rcr
        && left.numTimeSteps == right.numTimeSteps
        && left.dt == right.dt
        && left.numCycles == right.numCycles
        && left.maxRecordedFrames == right.maxRecordedFrames;
}

bool finite_solver_input(const FlowSolver1D::SolverInput& input)
{
    if (!std::isfinite(input.fluid.density) || input.fluid.density <= 0.0
        || !std::isfinite(input.fluid.viscosity) || input.fluid.viscosity <= 0.0
        || !std::isfinite(input.period) || input.period <= 0.0
        || !std::isfinite(input.dt) || input.dt <= 0.0) {
        return false;
    }
    for (double value : input.arcLength) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    for (double value : input.area0) {
        if (!std::isfinite(value) || value <= 0.0) {
            return false;
        }
    }
    for (const std::pair<double, double>& sample : input.inletWaveform) {
        if (!std::isfinite(sample.first) || !std::isfinite(sample.second)) {
            return false;
        }
    }
    for (double value : input.rcr) {
        if (!std::isfinite(value) || value <= 0.0) {
            return false;
        }
    }
    return true;
}

bool normalize_interpolated_envelope(double minimum,
                                     double maximum,
                                     double* value)
{
    if (value == nullptr || !std::isfinite(*value)) {
        return false;
    }
    const double magnitude = (std::max)(
        1.0, (std::max)(std::fabs(minimum), std::fabs(maximum)));
    const double tolerance = 64.0 * std::numeric_limits<double>::epsilon()
        * magnitude;
    if (*value < minimum) {
        if (minimum - *value > tolerance) {
            return false;
        }
        *value = minimum;
    } else if (*value > maximum) {
        if (*value - maximum > tolerance) {
            return false;
        }
        *value = maximum;
    }
    return true;
}

} // namespace

FlowSmokeProtocolV1 FlowSmokeProtocolV1::engineeringSmoke()
{
    FlowSmokeProtocolV1 protocol;
    protocol.id = "engineering-smoke-v1";
    protocol.version = 1;
    protocol.label = "L0 geometry smoke";
    protocol.stationCount = 11;
    protocol.minimumLengthMm = 50.0;
    protocol.maximumLengthMm = 500.0;
    protocol.minimumAreaMm2 = 50.0;
    protocol.maximumAreaMm2 = 2000.0;
    protocol.fluid = FluidProperties{};
    protocol.inletWaveform = {{0.0, 3.0}, {0.5, 8.0}, {1.0, 3.0}};
    protocol.period = 1.0;
    protocol.rcr = {106.0, 0.00068483, 1784.0};
    protocol.numTimeSteps = 200;
    protocol.dt = 1.0e-4;
    protocol.numCycles = 2;
    protocol.maxRecordedFrames = 200;
    return protocol;
}

FlowSmokeProtocolStamp FlowSmokeProtocolV1::stamp() const
{
    FlowSmokeProtocolStamp value;
    value.id = id;
    value.version = version;
    value.label = label;
    value.stationCount = stationCount;
    value.dtSeconds = dt;
    value.numTimeSteps = numTimeSteps;
    value.numCycles = numCycles;
    return value;
}

FlowInputAssembler::Result FlowInputAssembler::assemble(
    const VesselProfileV1& profile,
    ScaleSlot scaleSlot,
    const FlowSmokeProtocolV1& protocol)
{
    Result result;
    const FlowSmokeProtocolV1 canonical = FlowSmokeProtocolV1::engineeringSmoke();
    if (!same_protocol(protocol, canonical)) {
        result.status = Status::InvalidProtocol;
        return result;
    }
    if (scaleSlot != ScaleSlot::Organ) {
        result.status = Status::UnsupportedScaleForSmoke;
        return result;
    }

    result.profileValidation = VesselProfileValidator::validate(profile);
    if (!result.profileValidation.ok()) {
        result.status = Status::InvalidProfile;
        return result;
    }

    const std::vector<VesselProfileSample>& samples = profile.samples;
    const double firstArcMm = samples.front().arcLengthMm;
    const double lengthMm = samples.back().arcLengthMm - firstArcMm;
    if (lengthMm < protocol.minimumLengthMm
        || lengthMm > protocol.maximumLengthMm) {
        result.status = Status::GeometryOutOfEnvelope;
        return result;
    }
    for (const VesselProfileSample& sample : samples) {
        if (sample.areaMm2 < protocol.minimumAreaMm2
            || sample.areaMm2 > protocol.maximumAreaMm2) {
            result.status = Status::GeometryOutOfEnvelope;
            return result;
        }
    }

    result.conversion.sourceLengthUnit = FlowLengthUnit::Millimeter;
    result.conversion.sourceAreaUnit = FlowAreaUnit::SquareMillimeter;
    result.conversion.targetLengthUnit = FlowLengthUnit::Centimeter;
    result.conversion.targetAreaUnit = FlowAreaUnit::SquareCentimeter;
    result.conversion.lengthScale = kLengthMmToCm;
    result.conversion.areaScale = kAreaMm2ToCm2;

    result.solverInput.arcLength.reserve(protocol.stationCount);
    result.solverInput.area0.reserve(protocol.stationCount);
    result.stationMap.reserve(protocol.stationCount);

    for (std::size_t station = 0; station < protocol.stationCount; ++station) {
        const double fraction = static_cast<double>(station)
            / static_cast<double>(protocol.stationCount - 1);
        const double targetArcMm = firstArcMm + lengthMm * fraction;

        std::size_t right = static_cast<std::size_t>(
            std::lower_bound(
                samples.begin(), samples.end(), targetArcMm,
                [](const VesselProfileSample& sample, double arc) {
                    return sample.arcLengthMm < arc;
                })
            - samples.begin());
        std::size_t left = right;
        double leftWeight = 1.0;
        double rightWeight = 0.0;
        if (right >= samples.size()) {
            left = samples.size() - 1;
            right = left;
        } else if (samples[right].arcLengthMm != targetArcMm && right > 0) {
            left = right - 1;
            const double interval = samples[right].arcLengthMm
                - samples[left].arcLengthMm;
            if (!(interval > 0.0) || !std::isfinite(interval)) {
                result.status = Status::InvalidResampling;
                return result;
            }
            rightWeight = (targetArcMm - samples[left].arcLengthMm) / interval;
            leftWeight = 1.0 - rightWeight;
        }

        double areaMm2 = samples[left].areaMm2 * leftWeight
            + samples[right].areaMm2 * rightWeight;
        if (!normalize_interpolated_envelope(
                protocol.minimumAreaMm2,
                protocol.maximumAreaMm2,
                &areaMm2)) {
            result.status = Status::GeometryOutOfEnvelope;
            return result;
        }

        const double localArcCm = (targetArcMm - firstArcMm) * kLengthMmToCm;
        result.solverInput.arcLength.push_back(localArcCm);
        result.solverInput.area0.push_back(areaMm2 * kAreaMm2ToCm2);

        FlowStationSourceMapping mapping;
        mapping.solverStationIndex = station;
        mapping.solverArcLengthCm = localArcCm;
        mapping.leftSampleId = samples[left].sampleId;
        mapping.rightSampleId = samples[right].sampleId;
        mapping.leftWeight = leftWeight;
        mapping.rightWeight = rightWeight;
        result.stationMap.push_back(mapping);
    }

    result.solverInput.fluid = protocol.fluid;
    result.solverInput.inletWaveform = protocol.inletWaveform;
    result.solverInput.period = protocol.period;
    result.solverInput.rcr = protocol.rcr;
    result.solverInput.numTimeSteps = protocol.numTimeSteps;
    result.solverInput.dt = protocol.dt;
    result.solverInput.numCycles = protocol.numCycles;
    result.solverInput.maxRecordedFrames = protocol.maxRecordedFrames;

    if (result.solverInput.arcLength.size() != protocol.stationCount
        || result.stationMap.size() != protocol.stationCount
        || !finite_solver_input(result.solverInput)) {
        result.status = Status::InvalidResampling;
        return result;
    }
    for (std::size_t i = 1; i < result.solverInput.arcLength.size(); ++i) {
        if (!(result.solverInput.arcLength[i]
              > result.solverInput.arcLength[i - 1])) {
            result.status = Status::InvalidResampling;
            return result;
        }
    }

    result.status = Status::Ok;
    return result;
}

} // namespace xq
