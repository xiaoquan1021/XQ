#ifndef XQ_CORE_XQ_SIMULATION_CASE_H
#define XQ_CORE_XQ_SIMULATION_CASE_H

#include "core/XQFlowSmoke.h"
#include "core/NodeId.h"

#include <optional>
#include <utility>
#include <vector>

namespace xq {

using SimulationCaseId = NodeId;

struct SolverParameters {
    int timeSteps = 0;
    double timeStepSize = 0.0;
};

// Order is load-bearing: tests/core/test_simulation_case.cpp depends on the
// first four values and their semantics (NoSlip=wall / PrescribedVelocity=inlet
// / Resistance=outlet). New values may only be appended at the end.
enum class BoundaryConditionType {
    NoSlip,
    PrescribedVelocity,
    PrescribedPressure,
    Resistance,
    RCR,                // outlet 0D windkessel; uses BoundaryCondition::rcr = [Rp, C, Rd]
    InletFlowWaveform   // inlet flow-rate waveform; uses flowWaveform / waveformPeriod
};

struct BoundaryCondition {
    int faceId = 0;
    BoundaryConditionType type = BoundaryConditionType::NoSlip;
    double value = 0.0;

    // Optional payload for the appended M5 boundary-condition types. Existing
    // call sites and tests construct only {faceId, type, value}; these stay
    // empty/zero for them (aggregate init leaves trailing members defaulted).
    std::vector<double> rcr;                          // [Rp, C, Rd] when type == RCR (CGS)
    std::vector<std::pair<double, double>> flowWaveform; // (t, Q) when type == InletFlowWaveform
    double waveformPeriod = 0.0;                      // waveform period [s]
};

// Blood fluid properties in CGS units. Defaults mirror the SimVascular
// reference job (0090_0001.sjb): density 1.06 g/cm^3, viscosity 0.04 poise.
struct FluidProperties {
    double density = 1.06;
    double viscosity = 0.04;
};

// Reduced-order-model settings carried with the case (centerline source, the
// inlet/outlet face roles, and the time-integration plan for the 1D solver).
struct RomSettings {
    NodeId centerlineNode = NodeId::invalid();
    NodeId vesselProfileNode = NodeId::invalid();
    std::vector<int> inletFaceIds;
    std::vector<int> outletFaceIds;
    double period = 0.0;
    int numTimeSteps = 0;
    double dt = 0.0;
    int numCycles = 1;
};

class XQSimulationCase {
public:
    XQSimulationCase();

    void setId(SimulationCaseId id);
    SimulationCaseId id() const;

    void setSourceMeshNode(const NodeId& node);
    bool hasSourceMeshNode() const;
    NodeId sourceMeshNode() const;

    void setSolverParameters(const SolverParameters& parameters);
    const SolverParameters& solverParameters() const;

    void addBoundaryCondition(const BoundaryCondition& condition);
    const std::vector<BoundaryCondition>& boundaryConditions() const;
    bool boundaryConditionByFaceId(int faceId, BoundaryCondition* out) const;

    void setFluidProperties(const FluidProperties& fluid);
    const FluidProperties& fluidProperties() const;

    void setRomSettings(const RomSettings& settings);
    const RomSettings& romSettings() const;

    void setFlowSmokeProvenance(const FlowSmokeCaseProvenance& provenance);
    bool hasFlowSmokeProvenance() const;
    const FlowSmokeCaseProvenance& flowSmokeProvenance() const;

private:
    SimulationCaseId id_;
    bool hasSourceMesh_;
    NodeId sourceMeshNode_;
    SolverParameters solverParameters_;
    std::vector<BoundaryCondition> boundaryConditions_;
    FluidProperties fluidProperties_;
    RomSettings romSettings_;
    std::optional<FlowSmokeCaseProvenance> flowSmokeProvenance_;
};

} // namespace xq

#endif // XQ_CORE_XQ_SIMULATION_CASE_H
