#include "services/flow/BoundaryConditionService.h"

#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQMesh.h"
#include "core/XQScene.h"
#include "core/XQSimulationCase.h"
#include "core/XQSimulationCasePayload.h"
#include "core/command/XQSceneCommands.h"

#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace xq {
namespace {

BoundaryConditionService::Result bcFailure(BoundaryConditionService::Status status)
{
    BoundaryConditionService::Result result;
    result.status = status;
    return result;
}

bool isInletRole(BoundaryConditionType type)
{
    return type == BoundaryConditionType::InletFlowWaveform
        || type == BoundaryConditionType::PrescribedVelocity;
}

bool isOutletRole(BoundaryConditionType type)
{
    return type == BoundaryConditionType::RCR
        || type == BoundaryConditionType::Resistance
        || type == BoundaryConditionType::PrescribedPressure;
}

} // namespace

std::vector<std::pair<double, double>> BoundaryConditionService::parseFlowFile(
    const std::string& text)
{
    std::vector<std::pair<double, double>> samples;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        std::istringstream lineStream(line);
        double t = 0.0;
        double q = 0.0;
        // Require both columns to parse as numbers; otherwise skip the line
        // (covers blank lines and comment / header rows).
        if (lineStream >> t >> q) {
            samples.push_back(std::make_pair(t, q));
        }
    }
    return samples;
}

BoundaryConditionService::Result BoundaryConditionService::validateAndBind(
    const XQSimulationCase& base,
    const XQMesh& mesh)
{
    bool hasInlet = false;
    bool hasOutlet = false;

    const std::vector<BoundaryCondition>& conditions = base.boundaryConditions();
    for (std::size_t i = 0; i < conditions.size(); ++i) {
        const BoundaryCondition& bc = conditions[i];

        MeshBoundaryFace face = {};
        const bool faceFound = mesh.boundaryFaceById(bc.faceId, &face);
        if (!faceFound) {
            return bcFailure(Status::MissingFace);
        }

        if (bc.type == BoundaryConditionType::RCR) {
            if (bc.rcr.size() != 3) {
                return bcFailure(Status::InvalidRcr);
            }
            if (bc.rcr[0] <= 0.0 || bc.rcr[1] <= 0.0 || bc.rcr[2] <= 0.0) {
                return bcFailure(Status::InvalidRcr);
            }
        }

        if (bc.type == BoundaryConditionType::InletFlowWaveform) {
            if (bc.flowWaveform.empty() || bc.waveformPeriod <= 0.0) {
                return bcFailure(Status::EmptyWaveform);
            }
        }

        if (isInletRole(bc.type)) {
            hasInlet = true;
        }
        if (isOutletRole(bc.type)) {
            hasOutlet = true;
        }
    }

    if (!hasInlet) {
        return bcFailure(Status::MissingInlet);
    }
    if (!hasOutlet) {
        return bcFailure(Status::MissingOutlet);
    }

    Result result;
    result.status = Status::Ok;
    result.validatedCase = base;
    return result;
}

BoundaryConditionService::CommandResult BoundaryConditionService::bindCommand(
    XQScene* scene,
    const NodeId& caseNodeId,
    const XQSimulationCase& base,
    const XQMesh& mesh,
    const std::string& label)
{
    CommandResult result;
    if (scene == nullptr) {
        result.status = Status::NullScene;
        result.command = nullptr;
        return result;
    }

    const Result validated = validateAndBind(base, mesh);
    if (!validated.ok()) {
        result.status = validated.status;
        result.command = nullptr;
        return result;
    }

    auto payload = std::make_shared<XQSimulationCasePayload>(validated.validatedCase);
    result.status = Status::Ok;
    result.command.reset(new SemanticReplacePayloadCommand(
        scene, caseNodeId, XQDomainType::SimulationCase, payload, label));
    return result;
}

} // namespace xq
