#include "xq_SpatialMath.h"

#include <vtkCardinalSpline.h>
#include <vtkMath.h>
#include <vtkSmartPointer.h>

#include <cmath>
#include <algorithm>
#include <limits>

std::vector<mitk::Point3D> xq_SpatialMath::InterpolateCurveViaSpline(
    const std::vector<mitk::Point3D>& controlPoints,
    bool closed,
    int numInterpolationPoints)
{
    std::vector<mitk::Point3D> result;

    const int numCtrl = static_cast<int>(controlPoints.size());
    if (numCtrl < 2)
    {
        return controlPoints;
    }

    auto splineX = vtkSmartPointer<vtkCardinalSpline>::New();
    auto splineY = vtkSmartPointer<vtkCardinalSpline>::New();
    auto splineZ = vtkSmartPointer<vtkCardinalSpline>::New();

    splineX->SetClosed(closed);
    splineY->SetClosed(closed);
    splineZ->SetClosed(closed);

    for (int i = 0; i < numCtrl; ++i)
    {
        double t = static_cast<double>(i);
        splineX->AddPoint(t, controlPoints[i][0]);
        splineY->AddPoint(t, controlPoints[i][1]);
        splineZ->AddPoint(t, controlPoints[i][2]);
    }

    double tMax = closed ? static_cast<double>(numCtrl)
                         : static_cast<double>(numCtrl - 1);
    int totalOut = numCtrl * numInterpolationPoints;
    if (!closed)
    {
        totalOut = (numCtrl - 1) * numInterpolationPoints + 1;
    }

    result.reserve(totalOut);

    for (int i = 0; i < totalOut; ++i)
    {
        double t = tMax * static_cast<double>(i) / static_cast<double>(totalOut);
        mitk::Point3D pt;
        pt[0] = splineX->Evaluate(t);
        pt[1] = splineY->Evaluate(t);
        pt[2] = splineZ->Evaluate(t);
        result.push_back(pt);
    }

    return result;
}

int xq_SpatialMath::FindNearestSegmentInsertionPoint(
    const std::vector<mitk::Point3D>& points,
    const mitk::Point3D& newPoint)
{
    if (points.empty())
    {
        return 0;
    }
    if (points.size() == 1)
    {
        return 1;
    }

    double minDist = std::numeric_limits<double>::max();
    int insertIdx = static_cast<int>(points.size());

    for (int i = 0; i < static_cast<int>(points.size()) - 1; ++i)
    {
        // Distance from newPoint to the segment (points[i], points[i+1])
        // Project newPoint onto the line defined by the segment
        mitk::Vector3D seg;
        seg[0] = points[i + 1][0] - points[i][0];
        seg[1] = points[i + 1][1] - points[i][1];
        seg[2] = points[i + 1][2] - points[i][2];

        mitk::Vector3D toNew;
        toNew[0] = newPoint[0] - points[i][0];
        toNew[1] = newPoint[1] - points[i][1];
        toNew[2] = newPoint[2] - points[i][2];

        double segLenSq = DotProduct3D(seg, seg);
        double t = 0.0;
        if (segLenSq > 1e-12)
        {
            t = DotProduct3D(toNew, seg) / segLenSq;
            t = std::max(0.0, std::min(1.0, t));
        }

        mitk::Point3D proj;
        proj[0] = points[i][0] + t * seg[0];
        proj[1] = points[i][1] + t * seg[1];
        proj[2] = points[i][2] + t * seg[2];

        double d = EuclideanDistance3D(newPoint, proj);
        if (d < minDist)
        {
            minDist = d;
            insertIdx = i + 1;
        }
    }

    return insertIdx;
}

bool xq_SpatialMath::IsWithinBoundingBox(const mitk::Point3D& point, double bounds[6])
{
    return point[0] >= bounds[0] && point[0] <= bounds[1]
        && point[1] >= bounds[2] && point[1] <= bounds[3]
        && point[2] >= bounds[4] && point[2] <= bounds[5];
}

mitk::Vector3D xq_SpatialMath::ComputeOrthogonalVector(const mitk::Vector3D& vec)
{
    mitk::Vector3D seed;
    // Choose a seed that is not parallel to vec
    if (std::fabs(vec[0]) < 0.9)
    {
        seed[0] = 1.0; seed[1] = 0.0; seed[2] = 0.0;
    }
    else
    {
        seed[0] = 0.0; seed[1] = 1.0; seed[2] = 0.0;
    }

    mitk::Vector3D perp = CrossProduct3D(vec, seed);
    return UnitVector(perp);
}

double xq_SpatialMath::EuclideanDistance3D(const mitk::Point3D& p1, const mitk::Point3D& p2)
{
    double dx = p1[0] - p2[0];
    double dy = p1[1] - p2[1];
    double dz = p1[2] - p2[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double xq_SpatialMath::DotProduct3D(const mitk::Vector3D& v1, const mitk::Vector3D& v2)
{
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

mitk::Vector3D xq_SpatialMath::CrossProduct3D(const mitk::Vector3D& v1, const mitk::Vector3D& v2)
{
    mitk::Vector3D result;
    result[0] = v1[1] * v2[2] - v1[2] * v2[1];
    result[1] = v1[2] * v2[0] - v1[0] * v2[2];
    result[2] = v1[0] * v2[1] - v1[1] * v2[0];
    return result;
}

mitk::Vector3D xq_SpatialMath::UnitVector(const mitk::Vector3D& vec)
{
    double len = std::sqrt(DotProduct3D(vec, vec));
    mitk::Vector3D result;
    if (len > 1e-12)
    {
        result[0] = vec[0] / len;
        result[1] = vec[1] / len;
        result[2] = vec[2] / len;
    }
    else
    {
        result[0] = 0.0;
        result[1] = 0.0;
        result[2] = 0.0;
    }
    return result;
}

double xq_SpatialMath::ComputeAngleRadians(const mitk::Vector3D& v1, const mitk::Vector3D& v2)
{
    double len1 = std::sqrt(DotProduct3D(v1, v1));
    double len2 = std::sqrt(DotProduct3D(v2, v2));
    if (len1 < 1e-12 || len2 < 1e-12)
    {
        return 0.0;
    }
    double cosAngle = DotProduct3D(v1, v2) / (len1 * len2);
    cosAngle = std::max(-1.0, std::min(1.0, cosAngle));
    return std::acos(cosAngle);
}

mitk::Point3D xq_SpatialMath::LinearInterpolate3D(const mitk::Point3D& p1, const mitk::Point3D& p2, double t)
{
    mitk::Point3D result;
    result[0] = p1[0] + t * (p2[0] - p1[0]);
    result[1] = p1[1] + t * (p2[1] - p1[1]);
    result[2] = p1[2] + t * (p2[2] - p1[2]);
    return result;
}
