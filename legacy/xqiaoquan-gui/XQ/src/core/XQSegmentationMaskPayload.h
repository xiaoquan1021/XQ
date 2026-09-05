#ifndef XQ_CORE_XQ_SEGMENTATION_MASK_PAYLOAD_H
#define XQ_CORE_XQ_SEGMENTATION_MASK_PAYLOAD_H

#include "core/XQDomainType.h"
#include "core/XQPayload.h"
#include "core/XQSegmentationMask.h"

#include <memory>
#include <utility>

namespace xq {

// Carries a node's real segmentation mask (label volume + geometry + source
// image binding). SegmentationService-produced masks and any future
// reader-loaded masks share this one payload type, mirroring the path payload
// design.
//
// clone() deep-copies the held XQSegmentationMask (value-type members only:
// vectors of POD labels/voxels and geometry), supporting edit-style commands
// that copy the old payload while preserving the node id and provenance.
class XQSegmentationMaskPayload : public XQPayload {
public:
    explicit XQSegmentationMaskPayload(XQSegmentationMask mask)
        : mask_(std::move(mask))
    {
    }

    XQDomainType domainType() const override
    {
        return XQDomainType::SegmentationMask;
    }

    std::shared_ptr<XQPayload> clone() const override
    {
        return std::make_shared<XQSegmentationMaskPayload>(mask_);
    }

    const XQSegmentationMask& mask() const
    {
        return mask_;
    }

    XQSegmentationMask& mask()
    {
        return mask_;
    }

private:
    XQSegmentationMask mask_;
};

} // namespace xq

#endif // XQ_CORE_XQ_SEGMENTATION_MASK_PAYLOAD_H
