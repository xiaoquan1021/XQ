#ifndef XQ_CORE_XQ_SOURCE_PAYLOADS_H
#define XQ_CORE_XQ_SOURCE_PAYLOADS_H

#include "core/XQDomainType.h"
#include "core/XQPayload.h"

#include <memory>
#include <string>

namespace xq {

// M0 skeleton payloads. Each carries the XQ-owned source binding (the relative
// file the node was loaded from) and its domain type. Geometry/scalar data is
// populated incrementally by the M1+ readers; the binding is enough to satisfy
// "data lands in XQ-owned payloads with correct provenance" at M0.
//
// A single concrete type is parameterized by domain so the reader can mint a
// node for any folder without one near-identical class per domain.
class XQSourcePayload : public XQPayload {
public:
    XQSourcePayload(XQDomainType domain, std::string source_path)
        : domain_(domain)
        , source_path_(std::move(source_path))
    {
    }

    XQDomainType domainType() const override
    {
        return domain_;
    }

    std::shared_ptr<XQPayload> clone() const override
    {
        return std::make_shared<XQSourcePayload>(domain_, source_path_);
    }

    const std::string& sourcePath() const
    {
        return source_path_;
    }

private:
    XQDomainType domain_;
    std::string source_path_;
};

} // namespace xq

#endif // XQ_CORE_XQ_SOURCE_PAYLOADS_H
