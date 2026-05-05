#ifndef XQ_MATH3_H
#define XQ_MATH3_H

#include <xqModuleCommonExports.h>
#include <mitkPoint.h>
#include <mitkVector.h>
#include <vector>
#include <array>

// Static-only utility class for 3D math operations
class XQMODULECOMMON_EXPORT xq_Math3
{
public:
    static std::vector<mitk::Point3D> InterpolateCurveViaSpline(
        const std::vector<mitk::Point3D>& controlPoints,
        bool closed = false,
        int numInterpolationPoints = 10);

    static int FindNearestSegmentInsertionPoint(
        const std::vector<mitk::Point3D>& points,
        const mitk::Point3D& newPoint);

    static bool IsWithinBoundingBox(const mitk::Point3D& point, double bounds[6]);

    static mitk::Vector3D ComputeOrthogonalVector(const mitk::Vector3D& vec);

    static double EuclideanDistance3D(const mitk::Point3D& p1, const mitk::Point3D& p2);

    static double DotProduct3D(const mitk::Vector3D& v1, const mitk::Vector3D& v2);

    static mitk::Vector3D CrossProduct3D(const mitk::Vector3D& v1, const mitk::Vector3D& v2);

    static mitk::Vector3D UnitVector(const mitk::Vector3D& vec);

    static double ComputeAngleRadians(const mitk::Vector3D& v1, const mitk::Vector3D& v2);

    static mitk::Point3D LinearInterpolate3D(const mitk::Point3D& p1, const mitk::Point3D& p2, double t);
};

#endif // XQ_MATH3_H
