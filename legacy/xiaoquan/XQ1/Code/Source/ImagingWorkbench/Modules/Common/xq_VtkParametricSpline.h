#ifndef XQ_VTK_PARAMETRIC_SPLINE_H
#define XQ_VTK_PARAMETRIC_SPLINE_H

#include <xqModuleCommonExports.h>
#include "xq_Spline.h"

#include <mitkPoint.h>
#include <mitkVector.h>
#include <vector>

// VTK cardinal-spline based implementation of xq_Spline
class XQMODULECOMMON_EXPORT xq_VtkParametricSpline : public xq_Spline
{
public:
    xq_VtkParametricSpline();
    ~xq_VtkParametricSpline() override;

    std::vector<mitk::Point3D> EvaluateCurve() override;

    // Describes position and local coordinate frame at a spline sample
    struct SplinePoint {
        mitk::Point3D  position;
        mitk::Vector3D tangent;
        mitk::Vector3D normal;
        mitk::Vector3D rotation;  // binormal / rotation vector for frame
    };

    // Evaluate positions together with Frenet-Serret frame vectors
    std::vector<SplinePoint> GetSplineFramePoints();
};

#endif // XQ_VTK_PARAMETRIC_SPLINE_H
