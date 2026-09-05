#include "xq_Spline.h"

xq_Spline::xq_Spline()
    : m_Periodic(false)
    , m_Resolution(100)
{
}

xq_Spline::~xq_Spline()
{
}

void xq_Spline::SetControlVertices(const std::vector<mitk::Point3D>& points)
{
    m_ControlVertices = points;
}

void xq_Spline::SetPeriodicBoundary(bool closed)
{
    m_Periodic = closed;
}

bool xq_Spline::IsPeriodicBoundary() const
{
    return m_Periodic;
}

int xq_Spline::GetResolution() const
{
    return m_Resolution;
}

void xq_Spline::SetResolution(int num)
{
    if (num > 0)
    {
        m_Resolution = num;
    }
}
