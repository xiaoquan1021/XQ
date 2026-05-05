// XQ approach: closed cardinal spline with adjustable tension for smooth contours
#pragma once

#include <xqModuleSegmentationExports.h>
#include "xq_LumenProfile.h"

class XQMODULESEGMENTATION_EXPORT xq_TensionProfile : public xq_LumenProfile
{
public:
    xq_TensionProfile() = default;
    ~xq_TensionProfile() override = default;

    void GenerateProfilePoints() override;
    std::string GetProfileKind() const override;
    // XQ: unique_ptr ownership transfer pattern for profile cloning
    std::unique_ptr<xq_LumenProfile> Duplicate() const override;

    void SetTension(double t);
    double GetTension() const;

protected:
    double m_Tension = 0.5;
};
