// XQ approach: closed Kochanek spline interpolation through control points
#pragma once

#include <xqModuleSegmentationExports.h>
#include "xq_LumenProfile.h"

class XQMODULESEGMENTATION_EXPORT xq_SplineProfile : public xq_LumenProfile
{
public:
    xq_SplineProfile() = default;
    ~xq_SplineProfile() override = default;

    void GenerateProfilePoints() override;
    std::string GetProfileKind() const override;
    // XQ: unique_ptr ownership transfer pattern for profile cloning
    std::unique_ptr<xq_LumenProfile> Duplicate() const override;
};
