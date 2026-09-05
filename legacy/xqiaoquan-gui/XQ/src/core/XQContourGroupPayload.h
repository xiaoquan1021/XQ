#ifndef XQ_CORE_XQ_CONTOUR_GROUP_PAYLOAD_H
#define XQ_CORE_XQ_CONTOUR_GROUP_PAYLOAD_H

#include "core/XQContourGroup.h"
#include "core/XQDomainType.h"
#include "core/XQPayload.h"

#include <memory>
#include <utility>

namespace xq {

// Carries a node's parsed contour group (the .ctgr segmentation contours). The
// SV project reader leaves ContourGroup nodes with an unresolved XQSourcePayload
// (the .ctgr path); the app layer parses it in the background with
// CTGRContourReader and swaps in this payload, mirroring how .mdl model nodes
// resolve to an XQSurfaceModelPayload. Modelled on XQPathPayload.
//
// clone() deep-copies the held XQContourGroup, supporting copy-style commands
// that duplicate the old payload while keeping the node id.
class XQContourGroupPayload : public XQPayload {
public:
    explicit XQContourGroupPayload(XQContourGroup group)
        : group_(std::move(group))
    {
    }

    XQDomainType domainType() const override
    {
        return XQDomainType::ContourGroup;
    }

    std::shared_ptr<XQPayload> clone() const override
    {
        // XQContourGroup holds only value-type members (a vector of POD contours,
        // ids, an optional NodeId), so copy construction is a full deep copy.
        return std::make_shared<XQContourGroupPayload>(group_);
    }

    const XQContourGroup& group() const
    {
        return group_;
    }

    XQContourGroup& group()
    {
        return group_;
    }

private:
    XQContourGroup group_;
};

} // namespace xq

#endif // XQ_CORE_XQ_CONTOUR_GROUP_PAYLOAD_H
