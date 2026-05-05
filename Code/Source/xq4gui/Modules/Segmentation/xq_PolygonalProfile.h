// XQ approach: polygon contour from user-placed control points with centroid tracking
#pragma once

#include <xqModuleSegmentationExports.h>
#include "xq_LumenProfile.h"

class XQMODULESEGMENTATION_EXPORT xq_PolygonalProfile : public xq_LumenProfile
{
public:
    xq_PolygonalProfile() = default;
    ~xq_PolygonalProfile() override = default;

    void GenerateProfilePoints() override;
    std::string GetProfileKind() const override;
    // XQ: unique_ptr ownership transfer pattern for profile cloning
    std::unique_ptr<xq_LumenProfile> Duplicate() const override;
};
