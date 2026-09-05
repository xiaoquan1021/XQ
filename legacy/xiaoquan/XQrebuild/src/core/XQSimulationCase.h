#ifndef XQ_CORE_XQ_SIMULATION_CASE_H
#define XQ_CORE_XQ_SIMULATION_CASE_H

#include "core/NodeId.h"

#include <vector>

namespace xq {

using SimulationCaseId = NodeId;

struct SolverParameters {
    int timeSteps = 0;
    double timeStepSize = 0.0;
};

enum class BoundaryConditionType {
    NoSlip,
    PrescribedVelocity,
    PrescribedPressure,
    Resistance
};

struct BoundaryCondition {
    int faceId = 0;
    BoundaryConditionType type = BoundaryConditionType::NoSlip;
    double value = 0.0;
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

private:
    SimulationCaseId id_;
    bool hasSourceMesh_;
    NodeId sourceMeshNode_;
    SolverParameters solverParameters_;
    std::vector<BoundaryCondition> boundaryConditions_;
};

} // namespace xq

#endif // XQ_CORE_XQ_SIMULATION_CASE_H
