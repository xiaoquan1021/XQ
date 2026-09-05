#ifndef XQ_CORE_XQ_FLOW_RESULT_PAYLOAD_H
#define XQ_CORE_XQ_FLOW_RESULT_PAYLOAD_H

#include "core/XQDomainType.h"
#include "core/XQFlowResult.h"
#include "core/XQPayload.h"

#include <memory>
#include <utility>

namespace xq {

// Carries a node's reduced-order / 1D blood-flow solution (per-segment Q/P/A
// time series + convergence diagnostics + source case binding). FlowSolver1D
// produces this payload type.
//
// XQFlowResult is a pure value object (its series are owned std::vectors), so
// clone() is a plain value copy -- the copy owns its own series.
class XQFlowResultPayload : public XQPayload {
public:
    explicit XQFlowResultPayload(XQFlowResult result)
        : result_(std::move(result))
    {
    }

    XQDomainType domainType() const override
    {
        return XQDomainType::FlowResult;
    }

    std::shared_ptr<XQPayload> clone() const override
    {
        return std::make_shared<XQFlowResultPayload>(result_);
    }

    const XQFlowResult& result() const
    {
        return result_;
    }

    XQFlowResult& result()
    {
        return result_;
    }

private:
    XQFlowResult result_;
};

} // namespace xq

#endif // XQ_CORE_XQ_FLOW_RESULT_PAYLOAD_H
