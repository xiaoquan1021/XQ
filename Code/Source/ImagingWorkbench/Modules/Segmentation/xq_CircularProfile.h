// XQ approach: parametric circle generation via normalized plane-axis decomposition
#pragma once

#include <xqModuleSegmentationExports.h>
#include "xq_LumenProfile.h"

class XQMODULESEGMENTATION_EXPORT xq_CircularProfile : public xq_LumenProfile
{
public:
    xq_CircularProfile();
    ~xq_CircularProfile() override = default;

    void GenerateProfilePoints() override;
    std::string GetProfileKind() const override;
    // XQ: unique_ptr ownership transfer pattern for profile cloning
    std::unique_ptr<xq_LumenProfile> Duplicate() const override;

    void SetRadius(double r);
    double GetRadius() const;
};
