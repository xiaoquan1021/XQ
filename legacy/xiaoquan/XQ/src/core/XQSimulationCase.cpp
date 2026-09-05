#include "core/XQSimulationCase.h"

#include <cstddef>

namespace xq {

XQSimulationCase::XQSimulationCase()
    : id_(SimulationCaseId::invalid())
    , hasSourceMesh_(false)
    , sourceMeshNode_(NodeId::invalid())
    , solverParameters_()
    , boundaryConditions_()
    , fluidProperties_()
    , romSettings_()
{
}

void XQSimulationCase::setId(SimulationCaseId id)
{
    id_ = id;
}

SimulationCaseId XQSimulationCase::id() const
{
    return id_;
}

void XQSimulationCase::setSourceMeshNode(const NodeId& node)
{
    sourceMeshNode_ = node;
    hasSourceMesh_ = true;
}

bool XQSimulationCase::hasSourceMeshNode() const
{
    return hasSourceMesh_;
}

NodeId XQSimulationCase::sourceMeshNode() const
{
    if (!hasSourceMesh_) {
        return NodeId::invalid();
    }
    return sourceMeshNode_;
}

void XQSimulationCase::setSolverParameters(const SolverParameters& parameters)
{
    solverParameters_ = parameters;
}

const SolverParameters& XQSimulationCase::solverParameters() const
{
    return solverParameters_;
}

void XQSimulationCase::addBoundaryCondition(const BoundaryCondition& condition)
{
    boundaryConditions_.push_back(condition);
}

const std::vector<BoundaryCondition>& XQSimulationCase::boundaryConditions() const
{
    return boundaryConditions_;
}

bool XQSimulationCase::boundaryConditionByFaceId(int faceId, BoundaryCondition* out) const
{
    for (std::size_t i = 0; i < boundaryConditions_.size(); ++i) {
        if (boundaryConditions_[i].faceId == faceId) {
            if (out != 0) {
                *out = boundaryConditions_[i];
            }
            return true;
        }
    }
    return false;
}

void XQSimulationCase::setFluidProperties(const FluidProperties& fluid)
{
    fluidProperties_ = fluid;
}

const FluidProperties& XQSimulationCase::fluidProperties() const
{
    return fluidProperties_;
}

void XQSimulationCase::setRomSettings(const RomSettings& settings)
{
    romSettings_ = settings;
}

const RomSettings& XQSimulationCase::romSettings() const
{
    return romSettings_;
}

} // namespace xq
