#ifndef XQ_CORE_XQ_VESSEL_PROFILE_PAYLOAD_H
#define XQ_CORE_XQ_VESSEL_PROFILE_PAYLOAD_H

#include "core/XQDomainType.h"
#include "core/XQPayload.h"
#include "core/XQVesselProfile.h"

#include <memory>
#include <utility>

namespace xq {

class XQVesselProfilePayload : public XQPayload {
public:
    explicit XQVesselProfilePayload(VesselProfileV1 profile)
        : profile_(std::move(profile))
    {
    }

    XQDomainType domainType() const override
    {
        return XQDomainType::VesselProfile;
    }

    std::shared_ptr<XQPayload> clone() const override
    {
        return std::make_shared<XQVesselProfilePayload>(profile_);
    }

    const VesselProfileV1& profile() const
    {
        return profile_;
    }

private:
    VesselProfileV1 profile_;
};

} // namespace xq

#endif // XQ_CORE_XQ_VESSEL_PROFILE_PAYLOAD_H
