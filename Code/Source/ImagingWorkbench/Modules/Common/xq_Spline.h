#ifndef XQ_SPLINE_H
#define XQ_SPLINE_H

#include <xqModuleCommonExports.h>
#include <mitkPoint.h>
#include <vector>

// Abstract base class for spline curves
class XQMODULECOMMON_EXPORT xq_Spline
{
public:
    enum SplineType { CARDINAL = 0, KOCHANEK };

    xq_Spline();
    virtual ~xq_Spline();

    void SetControlVertices(const std::vector<mitk::Point3D>& points);
    void SetPeriodicBoundary(bool closed);
    bool IsPeriodicBoundary() const;
    int GetResolution() const;
    void SetResolution(int num);

    virtual std::vector<mitk::Point3D> EvaluateCurve() = 0;

protected:
    std::vector<mitk::Point3D> m_ControlVertices;
    bool m_Periodic;
    int m_Resolution;
};

#endif // XQ_SPLINE_H
