#include <services/flow/FlowInputAssembler.h>

#include <cmath>
#include <cstdio>
#include <limits>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

#define CHECK(condition)                  \
    do {                                  \
        if (!(condition)) {               \
            return fail(#condition, __LINE__); \
        }                                 \
    } while (false)

bool close(double left, double right, double tolerance = 1.0e-12)
{
    return std::fabs(left - right) <= tolerance;
}

xq::VesselProfileV1 makeProfile(double lengthMm = 50.0,
                                double firstAreaMm2 = 50.0,
                                double lastAreaMm2 = 100.0)
{
    xq::VesselProfileV1 profile;
    profile.coordinateSystem = xq::VesselProfileCoordinateSystem::LPS;
    profile.lengthUnit = xq::VesselProfileLengthUnit::Millimeter;
    profile.areaUnit = xq::VesselProfileAreaUnit::SquareMillimeter;
    profile.frameOfReferenceId = "1.2.840.shell.flow";
    profile.sourcePathNode = xq::NodeId(10);
    profile.sourceEvidenceNodes = {xq::NodeId(20)};
    profile.derivationStamp.algorithmId = "xq.flow-smoke.test-profile";
    profile.derivationStamp.algorithmVersion = "1";
    profile.derivationStamp.parameterSummary = "nonuniform=true";
    profile.derivationStamp.inputs.push_back({xq::NodeId(10), 3, std::nullopt, ""});
    profile.derivationStamp.inputs.push_back({xq::NodeId(20), 5, std::nullopt, ""});

    const double fractions[] = {0.0, 0.18, 0.55, 1.0};
    for (std::size_t i = 0; i < 4; ++i) {
        xq::VesselProfileSample sample;
        sample.sampleId = xq::VesselSampleId(100 + i);
        sample.arcLengthMm = 7.0 + lengthMm * fractions[i];
        sample.positionMm = {sample.arcLengthMm, 1.0, 2.0};
        sample.unitTangent = {1.0, 0.0, 0.0};
        sample.areaMm2 = firstAreaMm2
            + (lastAreaMm2 - firstAreaMm2) * fractions[i];
        sample.evidenceKind = xq::VesselEvidenceKind::MeasuredContour;
        sample.quality = xq::VesselSampleQuality::Accepted;
        sample.sourceEvidenceNode = xq::NodeId(20);
        profile.samples.push_back(sample);
    }
    return profile;
}

} // namespace

int main()
{
    const xq::FlowSmokeProtocolV1 protocol =
        xq::FlowSmokeProtocolV1::engineeringSmoke();
    CHECK(protocol.id == "engineering-smoke-v1");
    CHECK(protocol.label == "L0 geometry smoke");
    CHECK(protocol.stationCount == 11);
    CHECK(protocol.dt == 1.0e-4);
    CHECK(protocol.numTimeSteps == 200);
    CHECK(protocol.numCycles == 2);

    const xq::VesselProfileV1 profile = makeProfile();
    const xq::FlowInputAssembler::Result assembled =
        xq::FlowInputAssembler::assemble(
            profile, xq::ScaleSlot::Organ, protocol);
    CHECK(assembled.ok());
    CHECK(assembled.solverInput.arcLength.size() == 11);
    CHECK(assembled.solverInput.area0.size() == 11);
    CHECK(assembled.stationMap.size() == 11);
    CHECK(close(assembled.solverInput.arcLength.front(), 0.0));
    CHECK(close(assembled.solverInput.arcLength.back(), 5.0));
    CHECK(close(assembled.solverInput.area0.front(), 0.5));
    CHECK(close(assembled.solverInput.area0.back(), 1.0));
    for (std::size_t i = 0; i < 11; ++i) {
        CHECK(close(assembled.solverInput.arcLength[i], 0.5 * i));
        CHECK(assembled.stationMap[i].solverStationIndex == i);
        CHECK(close(assembled.stationMap[i].solverArcLengthCm, 0.5 * i));
        CHECK(close(assembled.stationMap[i].leftWeight
                        + assembled.stationMap[i].rightWeight,
                    1.0));
        CHECK(assembled.stationMap[i].leftSampleId.is_valid());
        CHECK(assembled.stationMap[i].rightSampleId.is_valid());
    }
    CHECK(assembled.stationMap.front().leftSampleId == xq::VesselSampleId(100));
    CHECK(assembled.stationMap.back().leftSampleId == xq::VesselSampleId(103));
    CHECK(assembled.conversion.lengthScale == 0.1);
    CHECK(assembled.conversion.areaScale == 0.01);

    // The service receives a const Profile and must not rewrite its canonical
    // patient-space coordinates or units while producing local CGS stations.
    CHECK(profile.samples.front().arcLengthMm == 7.0);
    CHECK(profile.samples.back().arcLengthMm == 57.0);
    CHECK(profile.samples.front().areaMm2 == 50.0);

    CHECK(xq::FlowInputAssembler::assemble(
              profile, xq::ScaleSlot::Micro, protocol).status
          == xq::FlowInputAssembler::Status::UnsupportedScaleForSmoke);
    CHECK(xq::FlowInputAssembler::assemble(
              profile, xq::ScaleSlot::Cell, protocol).status
          == xq::FlowInputAssembler::Status::UnsupportedScaleForSmoke);

    xq::FlowSmokeProtocolV1 changedProtocol = protocol;
    changedProtocol.numTimeSteps = 201;
    CHECK(xq::FlowInputAssembler::assemble(
              profile, xq::ScaleSlot::Organ, changedProtocol).status
          == xq::FlowInputAssembler::Status::InvalidProtocol);

    xq::VesselProfileV1 invalid = profile;
    invalid.contractVersion = 99;
    CHECK(xq::FlowInputAssembler::assemble(
              invalid, xq::ScaleSlot::Organ, protocol).status
          == xq::FlowInputAssembler::Status::InvalidProfile);
    invalid = profile;
    invalid.coordinateSystem = xq::VesselProfileCoordinateSystem::Unknown;
    CHECK(xq::FlowInputAssembler::assemble(
              invalid, xq::ScaleSlot::Organ, protocol).status
          == xq::FlowInputAssembler::Status::InvalidProfile);
    invalid = profile;
    invalid.lengthUnit = xq::VesselProfileLengthUnit::Unknown;
    CHECK(xq::FlowInputAssembler::assemble(
              invalid, xq::ScaleSlot::Organ, protocol).status
          == xq::FlowInputAssembler::Status::InvalidProfile);
    invalid = profile;
    invalid.areaUnit = xq::VesselProfileAreaUnit::Unknown;
    CHECK(xq::FlowInputAssembler::assemble(
              invalid, xq::ScaleSlot::Organ, protocol).status
          == xq::FlowInputAssembler::Status::InvalidProfile);
    invalid = profile;
    invalid.samples[1].areaMm2 = -1.0;
    CHECK(xq::FlowInputAssembler::assemble(
              invalid, xq::ScaleSlot::Organ, protocol).status
          == xq::FlowInputAssembler::Status::InvalidProfile);
    invalid = profile;
    invalid.samples[1].arcLengthMm = invalid.samples[0].arcLengthMm;
    CHECK(xq::FlowInputAssembler::assemble(
              invalid, xq::ScaleSlot::Organ, protocol).status
          == xq::FlowInputAssembler::Status::InvalidProfile);
    invalid = profile;
    invalid.samples[1].areaMm2 = std::numeric_limits<double>::quiet_NaN();
    CHECK(xq::FlowInputAssembler::assemble(
              invalid, xq::ScaleSlot::Organ, protocol).status
          == xq::FlowInputAssembler::Status::InvalidProfile);

    CHECK(xq::FlowInputAssembler::assemble(
              makeProfile(49.999), xq::ScaleSlot::Organ, protocol).status
          == xq::FlowInputAssembler::Status::GeometryOutOfEnvelope);
    CHECK(xq::FlowInputAssembler::assemble(
              makeProfile(500.001), xq::ScaleSlot::Organ, protocol).status
          == xq::FlowInputAssembler::Status::GeometryOutOfEnvelope);
    CHECK(xq::FlowInputAssembler::assemble(
              makeProfile(50.0, 49.999, 100.0),
              xq::ScaleSlot::Organ, protocol).status
          == xq::FlowInputAssembler::Status::GeometryOutOfEnvelope);
    CHECK(xq::FlowInputAssembler::assemble(
              makeProfile(50.0, 100.0, 2000.001),
              xq::ScaleSlot::Organ, protocol).status
          == xq::FlowInputAssembler::Status::GeometryOutOfEnvelope);

    return 0;
}
