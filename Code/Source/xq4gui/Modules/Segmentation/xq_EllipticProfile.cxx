#include "xq_EllipticProfile.h"

#include <cmath>

constexpr double kTwoPi = 2.0 * M_PI;

xq_EllipticProfile::xq_EllipticProfile()
{
    mitk::Point3D center;
    center.Fill(0.0);

    mitk::Point3D majorPt;
    majorPt[0] = 2.0; majorPt[1] = 0.0; majorPt[2] = 0.0;

    mitk::Point3D minorPt;
    minorPt[0] = 0.0; minorPt[1] = 1.0; minorPt[2] = 0.0;

    m_AnchorPoints.push_back(center);
    m_AnchorPoints.push_back(majorPt);
    m_AnchorPoints.push_back(minorPt);
}

void xq_EllipticProfile::GenerateProfilePoints()
{
    m_ProfilePoints.clear();

    if (m_AnchorPoints.size() < 3)
    {
        return;
    }

    const double a = xq_detail::vecLength(m_AnchorPoints[1], m_ProfileCenter);
    const double b = xq_detail::vecLength(m_AnchorPoints[2], m_ProfileCenter);

    // Major direction vector (normalized)
    mitk::Vector3D majorDir;
    majorDir[0] = m_AnchorPoints[1][0] - m_ProfileCenter[0];
    majorDir[1] = m_AnchorPoints[1][1] - m_ProfileCenter[1];
    majorDir[2] = m_AnchorPoints[1][2] - m_ProfileCenter[2];
    majorDir = xq_detail::normalizeVec(majorDir);
    if (std::sqrt(majorDir[0]*majorDir[0] + majorDir[1]*majorDir[1] + majorDir[2]*majorDir[2]) < 1e-10)
    {
        majorDir[0] = 1.0; majorDir[1] = 0.0; majorDir[2] = 0.0;
    }

    // Minor direction vector (normalized)
    mitk::Vector3D minorDir;
    minorDir[0] = m_AnchorPoints[2][0] - m_ProfileCenter[0];
    minorDir[1] = m_AnchorPoints[2][1] - m_ProfileCenter[1];
    minorDir[2] = m_AnchorPoints[2][2] - m_ProfileCenter[2];
    minorDir = xq_detail::normalizeVec(minorDir);
    if (std::sqrt(minorDir[0]*minorDir[0] + minorDir[1]*minorDir[1] + minorDir[2]*minorDir[2]) < 1e-10)
    {
        minorDir[0] = 0.0; minorDir[1] = 1.0; minorDir[2] = 0.0;
    }

    // Generate ellipse points using std::generate
    m_ProfilePoints.resize(m_SubdivisionCount);
    int idx = 0;
    std::generate(m_ProfilePoints.begin(), m_ProfilePoints.end(),
        [&]() {
            const double angle = kTwoPi * static_cast<double>(idx++)
                                 / static_cast<double>(m_SubdivisionCount);
            const double cosA = std::cos(angle);
            const double sinA = std::sin(angle);
            mitk::Point3D pt;
            pt[0] = m_ProfileCenter[0] + a * cosA * majorDir[0] + b * sinA * minorDir[0];
            pt[1] = m_ProfileCenter[1] + a * cosA * majorDir[1] + b * sinA * minorDir[1];
            pt[2] = m_ProfileCenter[2] + a * cosA * majorDir[2] + b * sinA * minorDir[2];
            return pt;
        });
}

std::string xq_EllipticProfile::GetProfileKind() const
{
    return "Ellipse";
}

// XQ: unique_ptr ownership transfer pattern for profile cloning
std::unique_ptr<xq_LumenProfile> xq_EllipticProfile::Duplicate() const
{
    auto clone = std::make_unique<xq_EllipticProfile>();
    transferBaseState(*clone);
    return clone;
}
