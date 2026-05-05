#pragma once

#include <xqModulePathExports.h>

#include "xq_VesselCenterline.h"

#include <mitkPoint.h>
#include <string>
#include <string_view>
#include <vector>

// Reads legacy .pth path files produced by older versions of the application
class XQMODULEPATH_EXPORT xq_CenterlineLegacyIO
{
public:
    xq_CenterlineLegacyIO() = default;
    ~xq_CenterlineLegacyIO() = default;

    [[nodiscard]] static xq_VesselCenterline::Pointer ReadFile(std::string_view filePath);

private:
    static bool ParsePoint(const std::string& line, mitk::Point3D& point);
};
