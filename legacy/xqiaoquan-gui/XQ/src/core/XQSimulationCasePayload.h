#ifndef XQ_CORE_XQ_SIMULATION_CASE_PAYLOAD_H
#define XQ_CORE_XQ_SIMULATION_CASE_PAYLOAD_H

#include "core/XQDomainType.h"
#include "core/XQPayload.h"
#include "core/XQSimulationCase.h"

#include <memory>
#include <utility>

namespace xq {

// Carries a node's simulation case (solver params + boundary conditions + fluid
// properties + ROM settings + source mesh binding). Boundary-condition binding
// and 1D-solver setup happen against this one payload type, mirroring the path /
// mesh / surface-model payload design.
//
// XQSimulationCase is a pure value object (no shared geometry handles), so
// clone() is a plain value copy. This keeps edit-style commands (copy old
// payload, keep node id / provenance) from sharing mutable state.
class XQSimulationCasePayload : public XQPayload {
public:
    explicit XQSimulationCasePayload(XQSimulationCase simulationCase)
        : case_(std::move(simulationCase))
    {
    }

    XQDomainType domainType() const override
    {
        return XQDomainType::SimulationCase;
    }

    std::shared_ptr<XQPayload> clone() const override
    {
        return std::make_shared<XQSimulationCasePayload>(case_);
    }

    const XQSimulationCase& simulationCase() const
    {
        return case_;
    }

    XQSimulationCase& simulationCase()
    {
        return case_;
    }

private:
    XQSimulationCase case_;
};

} // namespace xq

#endif // XQ_CORE_XQ_SIMULATION_CASE_PAYLOAD_H
