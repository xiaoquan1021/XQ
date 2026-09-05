#include "services/flow/FlowGeometrySmokeService.h"

#include <cmath>
#include <cstddef>

namespace xq {
namespace {

bool finite_flow_result(const XQFlowResult& flow)
{
    if (flow.times().empty() || flow.segments().empty()
        || !flow.isConsistent() || !flow.converged()
        || !std::isfinite(flow.maxCfl())) {
        return false;
    }
    for (double value : flow.times()) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    for (const FlowSegment& segment : flow.segments()) {
        if (!std::isfinite(segment.arcLengthStart)
            || !std::isfinite(segment.arcLengthEnd)
            || !(segment.arcLengthEnd > segment.arcLengthStart)) {
            return false;
        }
    }
    const std::vector<std::vector<double>>* matrices[] = {
        &flow.flowQ(), &flow.pressureP(), &flow.areaA()};
    for (const std::vector<std::vector<double>>* matrix : matrices) {
        for (const std::vector<double>& row : *matrix) {
            for (double value : row) {
                if (!std::isfinite(value)) {
                    return false;
                }
            }
        }
    }
    for (const std::vector<double>& row : flow.areaA()) {
        for (double value : row) {
            if (!(value > 0.0)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

FlowGeometrySmokeService::Result FlowGeometrySmokeService::run(
    const Request& request)
{
    Result result;
    if (!request.sourceProfileNode.is_valid()
        || !request.caseNode.is_valid()
        || !request.resultNode.is_valid()
        || request.caseNode == request.resultNode
        || request.sourceProfileNode == request.caseNode
        || request.sourceProfileNode == request.resultNode) {
        return result;
    }

    FlowInputAssembler::Result assembled = FlowInputAssembler::assemble(
        request.profile, request.scaleSlot, request.protocol);
    result.assemblyStatus = assembled.status;
    result.profileValidation = assembled.profileValidation;
    if (!assembled.ok()) {
        result.status = Status::AssemblyFailed;
        return result;
    }

    FlowSolver1D::Result solved = FlowSolver1D::solve(assembled.solverInput);
    result.solverStatus = solved.status;
    if (!solved.ok()) {
        result.status = Status::SolverFailed;
        return result;
    }
    if (solved.flow.segments().size() + 1 != assembled.stationMap.size()
        || !finite_flow_result(solved.flow)) {
        result.status = Status::InvalidResult;
        return result;
    }

    XQSimulationCase simulationCase;
    simulationCase.setId(request.caseNode);
    SolverParameters solverParameters;
    solverParameters.timeSteps = request.protocol.numTimeSteps;
    solverParameters.timeStepSize = request.protocol.dt;
    simulationCase.setSolverParameters(solverParameters);
    simulationCase.setFluidProperties(request.protocol.fluid);

    RomSettings rom;
    rom.vesselProfileNode = request.sourceProfileNode;
    rom.period = request.protocol.period;
    rom.numTimeSteps = request.protocol.numTimeSteps;
    rom.dt = request.protocol.dt;
    rom.numCycles = request.protocol.numCycles;
    rom.inletFaceIds.push_back(1);
    rom.outletFaceIds.push_back(2);
    simulationCase.setRomSettings(rom);

    BoundaryCondition inlet;
    inlet.faceId = 1;
    inlet.type = BoundaryConditionType::InletFlowWaveform;
    inlet.flowWaveform = request.protocol.inletWaveform;
    inlet.waveformPeriod = request.protocol.period;
    simulationCase.addBoundaryCondition(inlet);

    BoundaryCondition outlet;
    outlet.faceId = 2;
    outlet.type = BoundaryConditionType::RCR;
    outlet.rcr = request.protocol.rcr;
    simulationCase.addBoundaryCondition(outlet);

    FlowSmokeCaseProvenance caseProvenance;
    caseProvenance.protocol = request.protocol.stamp();
    caseProvenance.assemblerId = FlowInputAssembler::AssemblerId;
    caseProvenance.assemblerVersion = FlowInputAssembler::AssemblerVersion;
    caseProvenance.sourceVesselProfileNode = request.sourceProfileNode;
    caseProvenance.sourceVesselProfileRevision = request.sourceProfileRevision;
    caseProvenance.conversion = assembled.conversion;
    caseProvenance.stationMap = assembled.stationMap;
    simulationCase.setFlowSmokeProvenance(caseProvenance);

    XQFlowResult flow = std::move(solved.flow);
    flow.setSourceCaseNode(request.caseNode);
    FlowSmokeResultProvenance resultProvenance;
    resultProvenance.protocol = request.protocol.stamp();
    resultProvenance.solverId = SolverId;
    resultProvenance.solverVersion = SolverVersion;
    resultProvenance.sourceVesselProfileNode = request.sourceProfileNode;
    resultProvenance.sourceVesselProfileRevision = request.sourceProfileRevision;
    flow.setFlowSmokeProvenance(resultProvenance);

    result.simulationCase = std::move(simulationCase);
    result.flowResult = std::move(flow);
    result.status = Status::Ok;
    return result;
}

} // namespace xq
