#ifndef XQ_CORE_XQ_AI_ANALYSIS_PAYLOAD_H
#define XQ_CORE_XQ_AI_ANALYSIS_PAYLOAD_H

#include "core/XQAiAnalysis.h"
#include "core/XQDomainType.h"
#include "core/XQPayload.h"

#include <memory>
#include <utility>

namespace xq {

// Carries a node's AI / post-processing analysis (flow metrics, identification,
// surrogate prediction). XQAiAnalysis is a pure value object (owned vectors), so
// clone() is a plain value copy -- the copy owns its own contents.
class XQAiAnalysisPayload : public XQPayload {
public:
    explicit XQAiAnalysisPayload(XQAiAnalysis analysis)
        : analysis_(std::move(analysis))
    {
    }

    XQDomainType domainType() const override
    {
        return XQDomainType::AiAnalysis;
    }

    std::shared_ptr<XQPayload> clone() const override
    {
        return std::make_shared<XQAiAnalysisPayload>(analysis_);
    }

    const XQAiAnalysis& analysis() const
    {
        return analysis_;
    }

    XQAiAnalysis& analysis()
    {
        return analysis_;
    }

private:
    XQAiAnalysis analysis_;
};

} // namespace xq

#endif // XQ_CORE_XQ_AI_ANALYSIS_PAYLOAD_H
