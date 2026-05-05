#include "xq_PolygonalProfile.h"

#include <numeric>

void xq_PolygonalProfile::GenerateProfilePoints()
{
    m_ProfilePoints = m_AnchorPoints;

    if (!m_AnchorPoints.empty())
    {
        // Compute centroid using std::accumulate with lambda
        mitk::Point3D zero;
        zero.Fill(0.0);

        auto sum = std::accumulate(m_AnchorPoints.cbegin(), m_AnchorPoints.cend(), zero,
            [](mitk::Point3D acc, const mitk::Point3D& p) {
                acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2];
                return acc;
            });

        const auto n = static_cast<double>(m_AnchorPoints.size());
        m_ProfileCenter[0] = sum[0] / n;
        m_ProfileCenter[1] = sum[1] / n;
        m_ProfileCenter[2] = sum[2] / n;
    }
}

std::string xq_PolygonalProfile::GetProfileKind() const
{
    return "Polygon";
}

// XQ: unique_ptr ownership transfer pattern for profile cloning
std::unique_ptr<xq_LumenProfile> xq_PolygonalProfile::Duplicate() const
{
    auto clone = std::make_unique<xq_PolygonalProfile>();
    transferBaseState(*clone);
    return clone;
}
