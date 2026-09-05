#include <services/flow/FlowGeometrySmokeService.h>

#include <cmath>
#include <cstdio>

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

xq::VesselProfileV1 makeProfile(double lengthMm, double areaMm2)
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
    profile.derivationStamp.parameterSummary = "boundary=true";
    profile.derivationStamp.inputs.push_back({xq::NodeId(10), 3, std::nullopt, ""});
    profile.derivationStamp.inputs.push_back({xq::NodeId(20), 5, std::nullopt, ""});
    const double fractions[] = {0.0, 0.13, 0.61, 1.0};
    for (std::size_t i = 0; i < 4; ++i) {
        xq::VesselProfileSample sample;
        sample.sampleId = xq::VesselSampleId(200 + i);
        sample.arcLengthMm = lengthMm * fractions[i];
        sample.positionMm = {sample.arcLengthMm, 0.0, 0.0};
        sample.unitTangent = {1.0, 0.0, 0.0};
        sample.areaMm2 = areaMm2;
        sample.evidenceKind = xq::VesselEvidenceKind::MeasuredContour;
        sample.quality = xq::VesselSampleQuality::Accepted;
        sample.sourceEvidenceNode = xq::NodeId(20);
        profile.samples.push_back(sample);
    }
    return profile;
}

int runBoundary(double lengthMm, double areaMm2,
                xq::NodeId caseNode, xq::NodeId resultNode)
{
    xq::FlowGeometrySmokeService::Request request;
    request.profile = makeProfile(lengthMm, areaMm2);
    request.scaleSlot = xq::ScaleSlot::Organ;
    request.sourceProfileNode = xq::NodeId(30);
    request.sourceProfileRevision = 7;
    request.caseNode = caseNode;
    request.resultNode = resultNode;

    const xq::FlowGeometrySmokeService::Result result =
        xq::FlowGeometrySmokeService::run(request);
    if (!result.ok()) {
        std::fprintf(stderr, "smoke status=%d assembly=%d solver=%d\n",
                     static_cast<int>(result.status),
                     static_cast<int>(result.assemblyStatus),
                     static_cast<int>(result.solverStatus));
    }
    CHECK(result.ok());
    CHECK(result.solverStatus == xq::FlowSolver1D::Status::Ok);
    CHECK(result.simulationCase.id() == caseNode);
    CHECK(result.simulationCase.romSettings().vesselProfileNode
          == request.sourceProfileNode);
    CHECK(result.simulationCase.hasFlowSmokeProvenance());
    const xq::FlowSmokeCaseProvenance& caseProvenance =
        result.simulationCase.flowSmokeProvenance();
    CHECK(caseProvenance.protocol.id == "engineering-smoke-v1");
    CHECK(caseProvenance.protocol.label == "L0 geometry smoke");
    CHECK(caseProvenance.protocol.stationCount == 11);
    CHECK(caseProvenance.sourceVesselProfileNode == request.sourceProfileNode);
    CHECK(caseProvenance.sourceVesselProfileRevision == 7);
    CHECK(caseProvenance.stationMap.size() == 11);
    CHECK(caseProvenance.conversion.lengthScale == 0.1);
    CHECK(caseProvenance.conversion.areaScale == 0.01);

    const xq::XQFlowResult& flow = result.flowResult;
    CHECK(flow.hasSourceCaseNode());
    CHECK(flow.sourceCaseNode() == caseNode);
    CHECK(flow.hasFlowSmokeProvenance());
    CHECK(flow.flowSmokeProvenance().sourceVesselProfileNode
          == request.sourceProfileNode);
    CHECK(flow.flowSmokeProvenance().sourceVesselProfileRevision == 7);
    CHECK(flow.flowSmokeProvenance().solverId == "xq-flow-solver-1d");
    CHECK(flow.isConsistent());
    CHECK(flow.converged());
    CHECK(flow.segments().size() == 10);
    CHECK(flow.times().size() == 200);
    CHECK(std::isfinite(flow.maxCfl()));
    CHECK(flow.maxCfl() <= 0.9);
    for (std::size_t segment = 0; segment < flow.segments().size(); ++segment) {
        CHECK(flow.flowQ()[segment].size() == flow.times().size());
        for (std::size_t time = 0; time < flow.times().size(); ++time) {
            CHECK(std::isfinite(flow.flowQ()[segment][time]));
            CHECK(std::isfinite(flow.pressureP()[segment][time]));
            CHECK(std::isfinite(flow.areaA()[segment][time]));
            CHECK(flow.areaA()[segment][time] > 0.0);
        }
    }
    return 0;
}

} // namespace

int main()
{
    // Both inclusive geometry-envelope corners execute the production solver.
    CHECK(runBoundary(50.0, 50.0, xq::NodeId(40), xq::NodeId(41)) == 0);
    CHECK(runBoundary(500.0, 2000.0, xq::NodeId(42), xq::NodeId(43)) == 0);

    xq::FlowGeometrySmokeService::Request invalid;
    invalid.profile = makeProfile(50.0, 50.0);
    invalid.sourceProfileNode = xq::NodeId(30);
    invalid.caseNode = xq::NodeId(40);
    invalid.resultNode = xq::NodeId(40);
    CHECK(xq::FlowGeometrySmokeService::run(invalid).status
          == xq::FlowGeometrySmokeService::Status::InvalidRequest);

    invalid.caseNode = xq::NodeId(40);
    invalid.resultNode = xq::NodeId(41);
    invalid.scaleSlot = xq::ScaleSlot::Micro;
    const xq::FlowGeometrySmokeService::Result unsupported =
        xq::FlowGeometrySmokeService::run(invalid);
    CHECK(unsupported.status
          == xq::FlowGeometrySmokeService::Status::AssemblyFailed);
    CHECK(unsupported.assemblyStatus
          == xq::FlowInputAssembler::Status::UnsupportedScaleForSmoke);
    return 0;
}
