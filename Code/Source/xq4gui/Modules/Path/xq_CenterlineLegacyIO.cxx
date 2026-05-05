#include "xq_CenterlineLegacyIO.h"
#include "xq_CenterlineSegment.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

// Returns a parsed point, or std::nullopt on failure
auto parsePointFromLine(const std::string& line) -> std::optional<mitk::Point3D>
{
    std::istringstream iss(line);
    if (double x, y, z; iss >> x >> y >> z)
    {
        mitk::Point3D pt;
        pt[0] = x; pt[1] = y; pt[2] = z;
        return pt;
    }
    return std::nullopt;
}

} // namespace

xq_VesselCenterline::Pointer xq_CenterlineLegacyIO::ReadFile(std::string_view filePath)
{
    std::ifstream ifs(std::string{filePath});
    if (!ifs.is_open())
        return nullptr;

    std::string firstLine;
    if (!std::getline(ifs, firstLine))
        return nullptr;

    if (int numPoints = 0; !(std::istringstream{firstLine} >> numPoints) || numPoints <= 0)
        return nullptr;
    else
    {
        std::vector<mitk::Point3D> controlPoints;
        controlPoints.reserve(static_cast<size_t>(numPoints));

        // Read N points using generate_n + back_inserter, with early-exit on parse failure
        bool parseOk = true;
        std::generate_n(std::back_inserter(controlPoints), numPoints, [&]() -> mitk::Point3D {
            std::string line;
            if (!parseOk || !std::getline(ifs, line))
            {
                parseOk = false;
                return mitk::Point3D();
            }
            if (auto pt = parsePointFromLine(line))
                return *pt;
            parseOk = false;
            return mitk::Point3D();
        });

        if (!parseOk || controlPoints.empty())
            return nullptr;

        auto path = xq_VesselCenterline::New();
        auto* elem = new xq_CenterlineSegment();
        elem->SetInterpolationMode(xq_CenterlineSegment::CUBIC_SPLINE);
        elem->ReplaceAnchors(controlPoints, true);
        path->SetSegment(elem, 0);

        return path;
    }
}

bool xq_CenterlineLegacyIO::ParsePoint(const std::string& line, mitk::Point3D& point)
{
    if (auto result = parsePointFromLine(line))
    {
        point = *result;
        return true;
    }
    return false;
}
