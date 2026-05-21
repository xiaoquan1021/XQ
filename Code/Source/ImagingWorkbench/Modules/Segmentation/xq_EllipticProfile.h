// XQ approach: parametric ellipse generation via major/minor axis decomposition
#pragma once

#include <xqModuleSegmentationExports.h>
#include "xq_LumenProfile.h"

class XQMODULESEGMENTATION_EXPORT xq_EllipticProfile : public xq_LumenProfile
{
public:
    xq_EllipticProfile();
    ~xq_EllipticProfile() override = default;

    void GenerateProfilePoints() override;
    std::string GetProfileKind() const override;
    // XQ: unique_ptr ownership transfer pattern for profile cloning
    std::unique_ptr<xq_LumenProfile> Duplicate() const override;
};
