#include "xq_LumenProfile.h"

#include <cmath>
#include <limits>

xq_LumenProfile::xq_LumenProfile()
{
    m_ProfileCenter.Fill(0.0);
}

void xq_LumenProfile::SetAnchorPoint(int index, const mitk::Point3D& pt)
{
    if (index >= 0 && index < static_cast<int>(m_AnchorPoints.size()))
    {
        m_AnchorPoints[index] = pt;
    }
}

mitk::Point3D xq_LumenProfile::GetAnchorPoint(int index) const
{
    if (index >= 0 && index < static_cast<int>(m_AnchorPoints.size()))
    {
        return m_AnchorPoints[index];
    }

    mitk::Point3D origin;
    origin.Fill(0.0);
    return origin;
}

void xq_LumenProfile::InsertAnchorPoint(int index, const mitk::Point3D& pt)
{
    if (index >= 0 && index <= static_cast<int>(m_AnchorPoints.size()))
    {
        m_AnchorPoints.insert(m_AnchorPoints.begin() + index, pt);
    }
}

void xq_LumenProfile::RemoveAnchorPoint(int index)
{
    if (index >= 0 && index < static_cast<int>(m_AnchorPoints.size()))
    {
        m_AnchorPoints.erase(m_AnchorPoints.begin() + index);
    }
}

int xq_LumenProfile::GetAnchorPointCount() const
{
    return static_cast<int>(m_AnchorPoints.size());
}

std::vector<mitk::Point3D>& xq_LumenProfile::GetAnchorPoints()
{
    return m_AnchorPoints;
}

std::vector<mitk::Point3D> xq_LumenProfile::GetProfilePoints()
{
    GenerateProfilePoints();
    return m_ProfilePoints;
}

mitk::Point3D xq_LumenProfile::GetProfileCenter() const
{
    return m_ProfileCenter;
}

void xq_LumenProfile::SetProfileCenter(const mitk::Point3D& pt)
{
    m_ProfileCenter = pt;
}

mitk::Point3D xq_LumenProfile::GetScalingPoint() const
{
    if (!m_AnchorPoints.empty())
    {
        return m_AnchorPoints[0];
    }
    return m_ProfileCenter;
}

std::array<double, 6> xq_LumenProfile::GetBoundingBox() const
{
    const auto& points = m_ProfilePoints;

    if (points.empty())
    {
        std::array<double, 6> zeros;
        zeros.fill(0.0);
        return zeros;
    }

    using lim = std::numeric_limits<double>;
    std::array<double, 6> init = {{ lim::max(), lim::lowest(),
                                    lim::max(), lim::lowest(),
                                    lim::max(), lim::lowest() }};

    return std::accumulate(points.cbegin(), points.cend(), init,
        [](std::array<double, 6> b, const mitk::Point3D& pt) {
            b[0] = std::min(b[0], pt[0]);
            b[1] = std::max(b[1], pt[0]);
            b[2] = std::min(b[2], pt[1]);
            b[3] = std::max(b[3], pt[1]);
            b[4] = std::min(b[4], pt[2]);
            b[5] = std::max(b[5], pt[2]);
            return b;
        });
}

void xq_LumenProfile::PlaceProfile(const mitk::Point3D& pt)
{
    m_ProfileCenter = pt;
    m_ProfilePlaced = true;
}

bool xq_LumenProfile::IsProfilePlaced() const
{
    return m_ProfilePlaced;
}

void xq_LumenProfile::SetSlicePlane(mitk::PlaneGeometry::Pointer plane)
{
    m_SlicePlane = plane;
}

mitk::PlaneGeometry::Pointer xq_LumenProfile::GetSlicePlane() const
{
    return m_SlicePlane;
}

void xq_LumenProfile::SetSubdivisionCount(int count)
{
    m_SubdivisionCount = count;
}

int xq_LumenProfile::GetSubdivisionCount() const
{
    return m_SubdivisionCount;
}

void xq_LumenProfile::SetAnchorPoints(const std::vector<mitk::Point3D>& points)
{
    m_AnchorPoints = points;
    m_ProfilePoints.clear();
    GenerateProfilePoints();
}

void xq_LumenProfile::transferBaseState(xq_LumenProfile& target) const
{
    target.m_AnchorPoints = m_AnchorPoints;
    target.m_ProfilePoints = m_ProfilePoints;
    target.m_ProfileCenter = m_ProfileCenter;
    target.m_SlicePlane = m_SlicePlane;
    target.m_ProfilePlaced = m_ProfilePlaced;
    target.m_SubdivisionCount = m_SubdivisionCount;
    target.m_PathPosIndex = m_PathPosIndex;
    target.m_Method = m_Method;
}
