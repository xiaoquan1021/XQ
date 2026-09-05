#ifndef XQ_CORE_XQ_IMAGE_VOLUME_PAYLOAD_H
#define XQ_CORE_XQ_IMAGE_VOLUME_PAYLOAD_H

#include "core/XQDomainType.h"
#include "core/XQImageVolume.h"
#include "core/XQPayload.h"

#include <memory>
#include <utility>

namespace xq {

// Metadata-only image payload. Real voxel bytes and IVoxelSource residency are
// owned by the asset/source layer, never by this payload. Construction strips
// even XQImageVolume's lightweight counts-only buffer handle so the invariant
// cannot be broken through cloning or a mutable accessor.
class XQImageVolumePayload : public XQPayload {
public:
    explicit XQImageVolumePayload(XQImageVolume volume)
        : volume_(std::move(volume))
    {
        volume_.setBuffer(std::shared_ptr<ImageBufferHandle>());
    }

    XQDomainType domainType() const override
    {
        return XQDomainType::Image;
    }

    std::shared_ptr<XQPayload> clone() const override
    {
        return std::make_shared<XQImageVolumePayload>(volume_);
    }

    const XQImageVolume& volume() const
    {
        return volume_;
    }

private:
    XQImageVolume volume_;
};

} // namespace xq

#endif // XQ_CORE_XQ_IMAGE_VOLUME_PAYLOAD_H
