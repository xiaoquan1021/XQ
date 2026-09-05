#ifndef XQ_CORE_XQ_PATH_PAYLOAD_H
#define XQ_CORE_XQ_PATH_PAYLOAD_H

#include "core/XQDomainType.h"
#include "core/XQPath.h"
#include "core/XQPayload.h"

#include <memory>

namespace xq {

// Carries a node's real path geometry (control points + resampled samples).
// Both PathService-created paths and PTHPathReader-loaded paths share this one
// payload type, so "new path" and "loaded path" are indistinguishable to the
// rest of the scene (M1 acceptance requirement).
//
// clone() deep-copies the held XQPath, supporting edit-style commands that copy
// the old payload while preserving the node id and provenance.
class XQPathPayload : public XQPayload {
public:
    explicit XQPathPayload(XQPath path)
        : path_(std::move(path))
    {
    }

    XQDomainType domainType() const override
    {
        return XQDomainType::Path;
    }

    std::shared_ptr<XQPayload> clone() const override
    {
        // XQPath holds only value-type members (vectors of POD points, ids),
        // so copy construction is a full deep copy.
        return std::make_shared<XQPathPayload>(path_);
    }

    const XQPath& path() const
    {
        return path_;
    }

    XQPath& path()
    {
        return path_;
    }

private:
    XQPath path_;
};

} // namespace xq

#endif // XQ_CORE_XQ_PATH_PAYLOAD_H
