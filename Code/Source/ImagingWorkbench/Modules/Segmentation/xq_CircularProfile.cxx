#include "xq_CircularProfile.h"

#include <cmath>

constexpr double kTwoPi = 2.0 * M_PI;

xq_CircularProfile::xq_CircularProfile()
{
    mitk::Point3D center;
    center.Fill(0.0);

    mitk::Point3D radiusPt;
    radiusPt[0] = 1.0; radiusPt[1] = 0.0; radiusPt[2] = 0.0;

    m_AnchorPoints.push_back(center);
    m_AnchorPoints.push_back(radiusPt);
}

void xq_CircularProfile::GenerateProfilePoints()
{
    m_ProfilePoints.clear();

    mitk::Point3D radiusPt;
    if (m_AnchorPoints.size() >= 2)
    {
        radiusPt = m_AnchorPoints[1];
    }
    else if (!m_AnchorPoints.empty())
    {
        radiusPt = m_AnchorPoints[0];
    }
    else
    {
        return;
    }

    const double radius = xq_detail::vecLength(radiusPt, m_ProfileCenter);
    if (radius <= 0.0)
    {
        return;
    }

    // Get two perpendicular axes in the plane
    mitk::Vector3D axis0;
    mitk::Vector3D axis1;

    if (m_SlicePlane.IsNotNull())
    {
        axis0 = xq_detail::normalizeVec(m_SlicePlane->GetAxisVector(0));
        axis1 = xq_detail::normalizeVec(m_SlicePlane->GetAxisVector(1));
    }
    else
    {
        axis0[0] = 1.0; axis0[1] = 0.0; axis0[2] = 0.0;
        axis1[0] = 0.0; axis1[1] = 1.0; axis1[2] = 0.0;
    }

    // Generate circle points using std::generate_n
    m_ProfilePoints.resize(m_SubdivisionCount);
    int idx = 0;
    std::generate(m_ProfilePoints.begin(), m_ProfilePoints.end(),
        [&]() {
            const double angle = kTwoPi * static_cast<double>(idx++)
                                 / static_cast<double>(m_SubdivisionCount);
            const double cosA = std::cos(angle);
            const double sinA = std::sin(angle);
            mitk::Point3D pt;
            pt[0] = m_ProfileCenter[0] + radius * cosA * axis0[0] + radius * sinA * axis1[0];
            pt[1] = m_ProfileCenter[1] + radius * cosA * axis0[1] + radius * sinA * axis1[1];
            pt[2] = m_ProfileCenter[2] + radius * cosA * axis0[2] + radius * sinA * axis1[2];
            return pt;
        });
}

std::string xq_CircularProfile::GetProfileKind() const
{
    return "Circle";
}

// XQ: unique_ptr ownership transfer pattern for profile cloning
std::unique_ptr<xq_LumenProfile> xq_CircularProfile::Duplicate() const
{
    auto clone = std::make_unique<xq_CircularProfile>();
    transferBaseState(*clone);
    return clone;
}

void xq_CircularProfile::SetRadius(double r)
{
    if (m_AnchorPoints.size() < 2 || r <= 0.0)
    {
        return;
    }

    mitk::Vector3D dir;
    dir[0] = m_AnchorPoints[1][0] - m_ProfileCenter[0];
    dir[1] = m_AnchorPoints[1][1] - m_ProfileCenter[1];
    dir[2] = m_AnchorPoints[1][2] - m_ProfileCenter[2];

    dir = xq_detail::normalizeVec(dir);
    if (std::sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]) < 1e-10)
    {
        dir[0] = 1.0; dir[1] = 0.0; dir[2] = 0.0;
    }

    m_AnchorPoints[1][0] = m_ProfileCenter[0] + r * dir[0];
    m_AnchorPoints[1][1] = m_ProfileCenter[1] + r * dir[1];
    m_AnchorPoints[1][2] = m_ProfileCenter[2] + r * dir[2];
}

double xq_CircularProfile::GetRadius() const
{
    if (m_AnchorPoints.size() < 2)
    {
        return 0.0;
    }

    return xq_detail::vecLength(m_AnchorPoints[1], m_ProfileCenter);
}
