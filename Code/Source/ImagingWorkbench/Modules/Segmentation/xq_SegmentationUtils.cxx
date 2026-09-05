#include "xq_SegmentationUtils.h"
#include "xq_CircularProfile.h"
#include "xq_EllipticProfile.h"
#include "xq_PolygonalProfile.h"
#include "xq_ProfileGroup.h"
#include "xq_SplineProfile.h"
#include "xq_SpatialMath.h"

#include <mitkImage.h>
#include <mitkLogMacros.h>
#include <mitkPointSet.h>
#include <mitkSlicedGeometry3D.h>

#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkCutter.h>
#include <vtkIdList.h>
#include <vtkIdTypeArray.h>
#include <vtkMatrix4x4.h>
#include <vtkPlane.h>
#include <vtkPolyData.h>
#include <vtkStripper.h>
#include <vtkTriangleStrip.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
#include <set>

namespace
{

constexpr double kEpsilon = 1e-10;
constexpr int kMinContourPoints = 3;
constexpr mitk::ScalarType kDefaultPlaneBounds[6] = {0.0, 64.0, 0.0, 64.0, 0.0, 1.0};

auto vectorLength(const mitk::Vector3D& v) -> double
{
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

auto normalizedOrFallback(const mitk::Vector3D& v, const mitk::Vector3D& fallback) -> mitk::Vector3D
{
    return vectorLength(v) > kEpsilon ? xq_SpatialMath::UnitVector(v) : fallback;
}

auto pointDistance(const mitk::Point3D& a, const mitk::Point3D& b) -> double
{
    const auto dx = b[0] - a[0];
    const auto dy = b[1] - a[1];
    const auto dz = b[2] - a[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

auto uniquePointCount(const std::vector<mitk::Point3D>& points,
                      double tolerance = 1e-6) -> size_t
{
    std::vector<mitk::Point3D> uniquePoints;
    uniquePoints.reserve(points.size());
    for (const auto& point : points)
    {
        const auto duplicateIt = std::find_if(
            uniquePoints.begin(), uniquePoints.end(),
            [&](const mitk::Point3D& candidate) {
                return pointDistance(candidate, point) <= tolerance;
            });
        if (duplicateIt == uniquePoints.end())
            uniquePoints.push_back(point);
    }
    return uniquePoints.size();
}

auto smoothClosedContour(const std::vector<mitk::Point3D>& contourPoints,
                         int iterations,
                        double relaxation) -> std::vector<mitk::Point3D>
{
    if (contourPoints.size() < kMinContourPoints || iterations <= 0 || relaxation <= 0.0)
        return contourPoints;

    auto result = contourPoints;
    for (int iter = 0; iter < iterations; ++iter)
    {
        auto next = result;
        for (size_t i = 0; i < result.size(); ++i)
        {
            const auto prevIdx = (i + result.size() - 1) % result.size();
            const auto nextIdx = (i + 1) % result.size();

            for (int d = 0; d < 3; ++d)
            {
                const auto neighborAverage = 0.5 * (result[prevIdx][d] + result[nextIdx][d]);
                next[i][d] = result[i][d] + relaxation * (neighborAverage - result[i][d]);
            }
        }
        result = std::move(next);
    }

    return result;
}

auto dotProduct(const mitk::Vector3D& lhs, const mitk::Vector3D& rhs) -> double
{
    return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
}

auto crossProduct(const mitk::Vector3D& lhs, const mitk::Vector3D& rhs) -> mitk::Vector3D
{
    mitk::Vector3D out;
    out[0] = lhs[1] * rhs[2] - lhs[2] * rhs[1];
    out[1] = lhs[2] * rhs[0] - lhs[0] * rhs[2];
    out[2] = lhs[0] * rhs[1] - lhs[1] * rhs[0];
    return out;
}

auto pointDifference(const mitk::Point3D& lhs, const mitk::Point3D& rhs) -> mitk::Vector3D
{
    mitk::Vector3D out;
    out[0] = lhs[0] - rhs[0];
    out[1] = lhs[1] - rhs[1];
    out[2] = lhs[2] - rhs[2];
    return out;
}

auto pointTranslated(
    const mitk::Point3D& origin,
    const mitk::Vector3D& axis0,
    const mitk::Vector3D& axis1,
    const mitk::Vector3D& normal,
    double u,
    double v,
    double w) -> mitk::Point3D
{
    mitk::Point3D out = origin;
    for (int d = 0; d < 3; ++d)
    {
        out[d] += axis0[d] * u + axis1[d] * v + normal[d] * w;
    }
    return out;
}

auto makeCanonicalWritebackViewDecision(
    xq_CanonicalWritebackWarningReason warningReason,
    xq_CanonicalWritebackFlow flow) -> xq_CanonicalWritebackViewDecision
{
    xq_CanonicalWritebackViewDecision decision;
    decision.warningReason = warningReason;
    decision.flow = flow;
    return decision;
}

void resolveFrameBasis(
    const mitk::PlaneGeometry* plane,
    mitk::Vector3D& axis0,
    mitk::Vector3D& axis1,
    mitk::Vector3D& normal)
{
    mitk::Vector3D fallbackAxis0;
    fallbackAxis0.Fill(0.0);
    fallbackAxis0[0] = 1.0;

    mitk::Vector3D fallbackAxis1;
    fallbackAxis1.Fill(0.0);
    fallbackAxis1[1] = 1.0;

    mitk::Vector3D fallbackNormal;
    fallbackNormal.Fill(0.0);
    fallbackNormal[2] = 1.0;

    if (!plane)
    {
        axis0 = fallbackAxis0;
        axis1 = fallbackAxis1;
        normal = fallbackNormal;
        return;
    }

    axis0 = normalizedOrFallback(plane->GetAxisVector(0), fallbackAxis0);
    axis1 = normalizedOrFallback(plane->GetAxisVector(1), fallbackAxis1);
    normal = normalizedOrFallback(plane->GetNormal(), crossProduct(axis0, axis1));

    axis1 = normalizedOrFallback(crossProduct(normal, axis0), fallbackAxis1);
    axis0 = normalizedOrFallback(crossProduct(axis1, normal), fallbackAxis0);
}

void resolveProfileBasis(
    xq_LumenProfile* profile,
    mitk::Vector3D& axis0,
    mitk::Vector3D& axis1,
    mitk::Vector3D& normal)
{
    if (!profile)
        return resolveFrameBasis(nullptr, axis0, axis1, normal);

    if (auto plane = profile->GetSlicePlane(); plane.IsNotNull())
        return resolveFrameBasis(plane, axis0, axis1, normal);

    const auto center = profile->GetProfileCenter();

    mitk::Vector3D fallbackAxis0;
    fallbackAxis0.Fill(0.0);
    fallbackAxis0[0] = 1.0;

    mitk::Vector3D fallbackNormal;
    fallbackNormal.Fill(0.0);
    fallbackNormal[2] = 1.0;

    axis0 = fallbackAxis0;
    for (int i = 0; i < profile->GetAnchorPointCount(); ++i)
    {
        const auto delta = pointDifference(profile->GetAnchorPoint(i), center);
        if (vectorLength(delta) > kEpsilon)
        {
            axis0 = xq_SpatialMath::UnitVector(delta);
            break;
        }
    }

    std::vector<mitk::Point3D> anchors;
    anchors.reserve(profile->GetAnchorPointCount());
    for (int i = 0; i < profile->GetAnchorPointCount(); ++i)
        anchors.push_back(profile->GetAnchorPoint(i));

    normal = normalizedOrFallback(
        xq_SegmentationUtils::ComputeContourNormal(anchors),
        fallbackNormal);
    axis1 = normalizedOrFallback(crossProduct(normal, axis0), xq_SpatialMath::ComputeOrthogonalVector(axis0));
    axis0 = normalizedOrFallback(crossProduct(axis1, normal), fallbackAxis0);
}

auto findNearestAvailableFrameIndex(
    const std::vector<xq_ProfilePlacementFrame>& frames,
    const std::set<int>& usedFrameIndices,
    const mitk::Point3D& referencePoint) -> int
{
    int bestIndex = -1;
    double bestDistance = std::numeric_limits<double>::max();
    for (size_t i = 0; i < frames.size(); ++i)
    {
        if (usedFrameIndices.count(static_cast<int>(i)) != 0)
            continue;

        const auto distance = pointDistance(frames[i].position, referencePoint);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = static_cast<int>(i);
        }
    }

    return bestIndex;
}

auto contourSquaredDistance(
    const std::vector<mitk::Point3D>& reference,
    const std::vector<mitk::Point3D>& contour,
    size_t shift) -> double
{
    double distance = 0.0;
    const auto count = reference.size();
    for (size_t i = 0; i < count; ++i)
    {
        const auto& lhs = reference[i];
        const auto& rhs = contour[(i + shift) % count];
        const double dx = lhs[0] - rhs[0];
        const double dy = lhs[1] - rhs[1];
        const double dz = lhs[2] - rhs[2];
        distance += dx * dx + dy * dy + dz * dz;
    }
    return distance;
}

auto rotateContour(
    const std::vector<mitk::Point3D>& contour,
    size_t shift) -> std::vector<mitk::Point3D>
{
    std::vector<mitk::Point3D> aligned;
    aligned.reserve(contour.size());
    for (size_t i = 0; i < contour.size(); ++i)
        aligned.push_back(contour[(i + shift) % contour.size()]);
    return aligned;
}

auto alignContourToReference(
    const std::vector<mitk::Point3D>& reference,
    const std::vector<mitk::Point3D>& contour) -> std::vector<mitk::Point3D>
{
    if (reference.size() != contour.size() || contour.empty())
        return contour;

    size_t bestShift = 0;
    double bestDistance = std::numeric_limits<double>::max();

    for (size_t shift = 0; shift < contour.size(); ++shift)
    {
        const auto distance = contourSquaredDistance(reference, contour, shift);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestShift = shift;
        }
    }

    auto reversed = contour;
    std::reverse(reversed.begin(), reversed.end());
    for (size_t shift = 0; shift < reversed.size(); ++shift)
    {
        const auto distance = contourSquaredDistance(reference, reversed, shift);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestShift = shift;
            return rotateContour(reversed, bestShift);
        }
    }

    return rotateContour(contour, bestShift);
}

} // namespace

// ---------------------------------------------------------------------------
// CalculateContourArea – 3D polygon area via cross-product summation
// ---------------------------------------------------------------------------

double xq_SegmentationUtils::CalculateContourArea(
    const std::vector<mitk::Point3D>& contourPoints)
{
    if (contourPoints.size() < kMinContourPoints)
        return 0.0;

    const auto& p0 = contourPoints[0];

    std::array<double, 3> crossSum = {0.0, 0.0, 0.0};
    for (size_t i = 1; i + 1 < contourPoints.size(); ++i)
    {
        const auto& cur = contourPoints[i];
        const auto& nxt = contourPoints[i + 1];

        const auto v1x = cur[0] - p0[0], v1y = cur[1] - p0[1], v1z = cur[2] - p0[2];
        const auto v2x = nxt[0] - p0[0], v2y = nxt[1] - p0[1], v2z = nxt[2] - p0[2];

        crossSum[0] += v1y * v2z - v1z * v2y;
        crossSum[1] += v1z * v2x - v1x * v2z;
        crossSum[2] += v1x * v2y - v1y * v2x;
    }

    return 0.5 * std::sqrt(crossSum[0] * crossSum[0] +
                            crossSum[1] * crossSum[1] +
                            crossSum[2] * crossSum[2]);
}

// ---------------------------------------------------------------------------
// CalculateContourPerimeter – sum of edge lengths (closed loop)
// ---------------------------------------------------------------------------

double xq_SegmentationUtils::CalculateContourPerimeter(
    const std::vector<mitk::Point3D>& contourPoints)
{
    if (contourPoints.size() < 2)
        return 0.0;

    double edgeSum = 0.0;
    for (size_t i = 0; i < contourPoints.size(); ++i)
    {
        const auto nextIdx = (i + 1) % contourPoints.size();
        edgeSum += pointDistance(contourPoints[i], contourPoints[nextIdx]);
    }

    return edgeSum;
}

// ---------------------------------------------------------------------------
// ComputeContourNormal – Newell method
// ---------------------------------------------------------------------------

mitk::Vector3D xq_SegmentationUtils::ComputeContourNormal(
    const std::vector<mitk::Point3D>& contourPoints)
{
    mitk::Vector3D normal;
    normal.Fill(0.0);

    if (contourPoints.size() < kMinContourPoints)
        return normal;

    for (size_t i = 0; i < contourPoints.size(); ++i)
    {
        const auto next = (i + 1) % contourPoints.size();
        const auto& cur = contourPoints[i];
        const auto& nxt = contourPoints[next];

        normal[0] += (cur[1] - nxt[1]) * (cur[2] + nxt[2]);
        normal[1] += (cur[2] - nxt[2]) * (cur[0] + nxt[0]);
        normal[2] += (cur[0] - nxt[0]) * (cur[1] + nxt[1]);
    }

    const auto length = std::sqrt(
        normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (length > kEpsilon)
    {
        normal[0] /= length;
        normal[1] /= length;
        normal[2] /= length;
    }

    return normal;
}

// ---------------------------------------------------------------------------
// ComputeContourCentroid – std::accumulate with lambda
// ---------------------------------------------------------------------------

mitk::Point3D xq_SegmentationUtils::ComputeContourCentroid(
    const std::vector<mitk::Point3D>& contourPoints)
{
    mitk::Point3D centroid;
    centroid.Fill(0.0);

    if (contourPoints.empty())
        return centroid;

    centroid = std::accumulate(
        contourPoints.cbegin(), contourPoints.cend(),
        centroid,
        [](mitk::Point3D acc, const mitk::Point3D& pt) {
            acc[0] += pt[0];
            acc[1] += pt[1];
            acc[2] += pt[2];
            return acc;
        });

    const auto n = static_cast<double>(contourPoints.size());
    centroid[0] /= n;
    centroid[1] /= n;
    centroid[2] /= n;

    return centroid;
}

// ---------------------------------------------------------------------------
// IsPointInsideContour – project to 2D plane, ray casting algorithm
// ---------------------------------------------------------------------------

bool xq_SegmentationUtils::IsPointInsideContour(
    const mitk::Point3D& point,
    const std::vector<mitk::Point3D>& contourPoints,
    const mitk::Vector3D& normal)
{
    const auto n = contourPoints.size();
    if (n == 0)
        return false;

    if (n < kMinContourPoints)
        return false;

    // Drop the dominant normal axis for 2D projection
    const std::array<double, 3> absN = {
        std::fabs(normal[0]), std::fabs(normal[1]), std::fabs(normal[2])};

    int dropAxis = 0;
    if (absN[1] > absN[0] && absN[1] > absN[2])
        dropAxis = 1;
    else if (absN[2] > absN[0] && absN[2] > absN[1])
        dropAxis = 2;

    const auto ax1 = (dropAxis + 1) % 3;
    const auto ax2 = (dropAxis + 2) % 3;

    const auto px = point[ax1];
    const auto py = point[ax2];

    // Ray casting: count crossings of a horizontal ray from (px, py) to +inf
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++)
    {
        const auto yi = contourPoints[i][ax2];
        const auto yj = contourPoints[j][ax2];
        const auto xi = contourPoints[i][ax1];
        const auto xj = contourPoints[j][ax1];

        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
        {
            inside = !inside;
        }
    }

    return inside;
}

// ---------------------------------------------------------------------------
// LoftContours – connect adjacent contour loops with triangle strips
// ---------------------------------------------------------------------------

vtkSmartPointer<vtkPolyData> xq_SegmentationUtils::LoftContours(
    const std::vector<std::vector<mitk::Point3D>>& contourSets)
{
    if (contourSets.size() < 2)
        return nullptr;

    auto points = vtkSmartPointer<vtkPoints>::New();
    auto strips = vtkSmartPointer<vtkCellArray>::New();
    auto polys  = vtkSmartPointer<vtkCellArray>::New();

    // Resample all contours to the same number of points
    auto targetCount = std::max(kMinContourPoints,
        static_cast<int>(std::max_element(contourSets.cbegin(), contourSets.cend(),
            [](const auto& a, const auto& b) {
                return a.size() < b.size();
            })->size()));

    std::vector<std::vector<mitk::Point3D>> resampled;
    resampled.reserve(contourSets.size());
    for (const auto& contour : contourSets)
        resampled.push_back(ResampleContour(contour, targetCount));

    // Ensure consistent winding and stable point correspondence across slices.
    // Align each contour loop before lofting; without this, two identical
    // circular slices with different start points can inflate into a twisted
    // bulb or sphere-like surface.
    if (!resampled.empty())
    {
        auto refNormal = ComputeContourNormal(resampled[0]);
        for (size_t i = 1; i < resampled.size(); ++i)
        {
            auto ni = ComputeContourNormal(resampled[i]);
            double dot = refNormal[0] * ni[0] + refNormal[1] * ni[1] + refNormal[2] * ni[2];
            if (dot < 0.0)
                std::reverse(resampled[i].begin(), resampled[i].end());

            resampled[i] = alignContourToReference(resampled[i - 1], resampled[i]);
        }
    }

    // Insert all points
    for (const auto& contour : resampled)
        for (const auto& pt : contour)
            points->InsertNextPoint(pt[0], pt[1], pt[2]);

    // Create triangle strips between consecutive contours
    for (size_t c = 0; c + 1 < resampled.size(); ++c)
    {
        const auto baseA = static_cast<vtkIdType>(c * targetCount);
        const auto baseB = static_cast<vtkIdType>((c + 1) * targetCount);

        auto strip = vtkSmartPointer<vtkTriangleStrip>::New();
        strip->GetPointIds()->SetNumberOfIds(targetCount * 2 + 2);

        for (int i = 0; i < targetCount; ++i)
        {
            strip->GetPointIds()->SetId(i * 2, baseA + i);
            strip->GetPointIds()->SetId(i * 2 + 1, baseB + i);
        }

        strip->GetPointIds()->SetId(targetCount * 2, baseA);
        strip->GetPointIds()->SetId(targetCount * 2 + 1, baseB);

        strips->InsertNextCell(strip);
    }

    // Cap the first contour (reverse winding for outward-facing normal toward -Z)
    {
        auto cap = vtkSmartPointer<vtkIdList>::New();
        for (int i = targetCount - 1; i >= 0; --i)
            cap->InsertNextId(i);
        polys->InsertNextCell(cap);
    }

    // Cap the last contour (forward winding for outward-facing normal toward +Z)
    {
        const auto lastBase =
            static_cast<vtkIdType>((resampled.size() - 1) * targetCount);
        auto cap = vtkSmartPointer<vtkIdList>::New();
        for (int i = 0; i < targetCount; ++i)
            cap->InsertNextId(lastBase + i);
        polys->InsertNextCell(cap);
    }

    auto output = vtkSmartPointer<vtkPolyData>::New();
    output->SetPoints(points);
    output->SetStrips(strips);
    output->SetPolys(polys);

    return output;
}

// ---------------------------------------------------------------------------
// ResampleContour – interpolate along contour for evenly spaced points
// ---------------------------------------------------------------------------

std::vector<mitk::Point3D> xq_SegmentationUtils::ResampleContour(
    const std::vector<mitk::Point3D>& contourPoints,
    int targetPointCount)
{
    std::vector<mitk::Point3D> result;
    if (contourPoints.empty() || targetPointCount < 2)
        return result;

    // Compute cumulative arc-length distances (closed loop)
    const auto n = contourPoints.size();
    std::vector<double> dist(n + 1, 0.0);
    for (size_t i = 0; i < n; ++i)
    {
        const auto next = (i + 1) % n;
        dist[i + 1] = dist[i] + pointDistance(contourPoints[i], contourPoints[next]);
    }

    const auto totalLength = dist[n];
    if (totalLength < kEpsilon)
    {
        result.resize(targetPointCount, contourPoints[0]);
        return result;
    }

    const auto step = totalLength / targetPointCount;
    size_t seg = 0;

    for (int i = 0; i < targetPointCount; ++i)
    {
        const auto targetDist = i * step;

        while (seg + 1 < n && dist[seg + 1] < targetDist)
            ++seg;

        seg = std::min(seg, n - 1);

        const auto segLen = dist[seg + 1] - dist[seg];
        const auto t = (segLen > kEpsilon) ? (targetDist - dist[seg]) / segLen : 0.0;

        const auto idx0 = seg % n;
        const auto idx1 = (seg + 1) % n;

        mitk::Point3D pt;
        for (int d = 0; d < 3; ++d)
            pt[d] = contourPoints[idx0][d] + t * (contourPoints[idx1][d] - contourPoints[idx0][d]);

        result.push_back(pt);
    }

    return result;
}

// ---------------------------------------------------------------------------
// ContourToPolyData – closed polyline
// ---------------------------------------------------------------------------

vtkSmartPointer<vtkPolyData> xq_SegmentationUtils::ContourToPolyData(
    const std::vector<mitk::Point3D>& contourPoints)
{
    if (contourPoints.empty())
        return nullptr;

    auto points = vtkSmartPointer<vtkPoints>::New();
    auto lines  = vtkSmartPointer<vtkCellArray>::New();

    const auto numPts = static_cast<vtkIdType>(contourPoints.size());
    for (const auto& pt : contourPoints)
        points->InsertNextPoint(pt[0], pt[1], pt[2]);

    // Closed polyline: numPts + 1 ids (last connects back to first)
    std::vector<vtkIdType> ids(numPts + 1);
    std::iota(ids.begin(), std::prev(ids.end()), vtkIdType{0});
    ids[numPts] = 0;

    lines->InsertNextCell(numPts + 1, ids.data());

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(lines);

    return polyData;
}

std::vector<mitk::Point3D> xq_SegmentationUtils::ExtractSurfaceContourOnPlane(
    vtkPolyData* surface,
    const mitk::PlaneGeometry* planeGeometry)
{
    if (!surface || !planeGeometry)
        return {};

    auto cutPlane = vtkSmartPointer<vtkPlane>::New();
    const auto origin = planeGeometry->GetOrigin();
    auto normal = planeGeometry->GetNormal();
    mitk::Vector3D fallbackNormal;
    fallbackNormal.Fill(0.0);
    fallbackNormal[2] = 1.0;
    normal = normalizedOrFallback(normal, fallbackNormal);
    cutPlane->SetOrigin(origin[0], origin[1], origin[2]);
    cutPlane->SetNormal(normal[0], normal[1], normal[2]);

    auto cutter = vtkSmartPointer<vtkCutter>::New();
    cutter->SetCutFunction(cutPlane);
    cutter->SetInputData(surface);
    cutter->Update();

    auto stripper = vtkSmartPointer<vtkStripper>::New();
    stripper->SetInputData(cutter->GetOutput());
    stripper->JoinContiguousSegmentsOn();
    stripper->Update();

    vtkPolyData* cutOutput = stripper->GetOutput();
    if (!cutOutput || !cutOutput->GetPoints() || !cutOutput->GetLines())
        return {};

    vtkCellArray* lines = cutOutput->GetLines();
    lines->InitTraversal();

    vtkIdType npts = 0;
    const vtkIdType* ptIds = nullptr;
    std::vector<std::vector<mitk::Point3D>> candidateContours;

    while (lines->GetNextCell(npts, ptIds))
    {
        if (npts < kMinContourPoints)
            continue;

        std::vector<mitk::Point3D> contourPoints;
        contourPoints.reserve(static_cast<size_t>(npts));
        for (vtkIdType i = 0; i < npts; ++i)
        {
            double point[3];
            cutOutput->GetPoint(ptIds[i], point);
            mitk::Point3D pt;
            pt[0] = point[0];
            pt[1] = point[1];
            pt[2] = point[2];
            contourPoints.push_back(pt);
        }

        if (contourPoints.size() >= 2 &&
            pointDistance(contourPoints.front(), contourPoints.back()) <= 1e-6)
        {
            contourPoints.pop_back();
        }

        if (uniquePointCount(contourPoints) < kMinContourPoints)
            continue;

        candidateContours.push_back(std::move(contourPoints));
    }

    if (candidateContours.size() != 1)
        return {};

    return candidateContours.front();
}

mitk::DataNode::Pointer xq_SegmentationUtils::CreateLegacyThresholdContourNode(
    const std::vector<mitk::Point3D>& contourPoints,
    std::string_view parentGroupName,
    int contourIndex)
{
    if (contourPoints.size() < kMinContourPoints || contourIndex < 0)
        return nullptr;

    auto pointSet = mitk::PointSet::New();
    for (size_t i = 0; i < contourPoints.size(); ++i)
    {
        pointSet->InsertPoint(static_cast<int>(i), contourPoints[i]);
    }

    auto contourNode = mitk::DataNode::New();
    contourNode->SetData(pointSet);
    contourNode->SetName(
        std::string(parentGroupName) + "_threshold_" + std::to_string(contourIndex));
    contourNode->SetBoolProperty("xq.segmentation.contour", true);
    contourNode->SetStringProperty("xq.segmentation.contourtype", "Threshold");
    contourNode->SetIntProperty("xq.segmentation.contourindex", contourIndex);
    contourNode->SetColor(0.0f, 1.0f, 1.0f);
    contourNode->SetFloatProperty("pointsize", 3.0f);

    return contourNode;
}

std::unique_ptr<xq_LumenProfile> xq_SegmentationUtils::CreatePresetProfile(
    std::string_view contourType,
    const mitk::Point3D& center,
    double primarySize,
    double secondarySize,
    int subdivisionCount)
{
    if (primarySize <= 0.0 || subdivisionCount < kMinContourPoints)
        return nullptr;

    if (contourType == "Circle")
    {
        auto profile = std::make_unique<xq_CircularProfile>();
        profile->SetProfileCenter(center);
        profile->SetSubdivisionCount(subdivisionCount);
        profile->SetAnchorPoint(0, center);

        auto radiusPt = center;
        radiusPt[0] += primarySize;
        profile->SetAnchorPoint(1, radiusPt);

        profile->SetMethod("circle");
        profile->GenerateProfilePoints();
        return profile;
    }

    if (contourType == "Ellipse")
    {
        if (secondarySize <= 0.0)
            return nullptr;

        auto profile = std::make_unique<xq_EllipticProfile>();
        profile->SetProfileCenter(center);
        profile->SetSubdivisionCount(subdivisionCount);

        profile->SetAnchorPoint(0, center);
        auto majorPt = profile->GetAnchorPoint(1);
        majorPt[0] = center[0] + primarySize;
        majorPt[1] = center[1];
        majorPt[2] = center[2];
        profile->SetAnchorPoint(1, majorPt);

        auto minorPt = profile->GetAnchorPoint(2);
        minorPt[0] = center[0];
        minorPt[1] = center[1] + secondarySize;
        minorPt[2] = center[2];
        profile->SetAnchorPoint(2, minorPt);

        profile->SetMethod("ellipse");
        profile->GenerateProfilePoints();
        return profile;
    }

    return nullptr;
}

vtkSmartPointer<vtkPolyData> xq_SegmentationUtils::LoftProfileGroup(
    const xq_ProfileGroup* group,
    unsigned int timeStep)
{
    if (!group)
        return nullptr;

    std::vector<std::vector<mitk::Point3D>> contourSets;
    for (const auto pathPosIndex : group->GetProfilePathIndices(timeStep))
    {
        auto* profile = group->GetProfileAtPathPos(pathPosIndex, timeStep);
        if (!profile)
            continue;

        auto contourPoints = profile->GetProfilePoints();
        if (contourPoints.size() < kMinContourPoints)
            continue;

        contourSets.push_back(std::move(contourPoints));
    }

    return LoftContours(contourSets);
}

std::unique_ptr<xq_LumenProfile> xq_SegmentationUtils::CloneProfileWithOffset(
    const xq_LumenProfile* profile,
    double zOffset)
{
    if (!profile)
        return nullptr;

    auto clone = profile->Duplicate();
    if (!clone)
        return nullptr;

    auto center = clone->GetProfileCenter();
    center[2] += zOffset;
    clone->SetProfileCenter(center);

    for (int i = 0; i < clone->GetAnchorPointCount(); ++i)
    {
        auto pt = clone->GetAnchorPoint(i);
        pt[2] += zOffset;
        clone->SetAnchorPoint(i, pt);
    }

    if (auto plane = clone->GetSlicePlane(); plane.IsNotNull())
    {
        auto shiftedPlane = plane->Clone();
        auto origin = shiftedPlane->GetOrigin();
        origin[2] += zOffset;
        shiftedPlane->SetOrigin(origin);
        clone->SetSlicePlane(shiftedPlane);
    }

    clone->GenerateProfilePoints();
    return clone;
}

std::unique_ptr<xq_LumenProfile> xq_SegmentationUtils::ScaleProfile(
    const xq_LumenProfile* profile,
    double factor)
{
    if (!profile || factor <= 0.0)
        return nullptr;

    auto clone = profile->Duplicate();
    if (!clone)
        return nullptr;

    const auto center = clone->GetProfileCenter();
    for (int i = 0; i < clone->GetAnchorPointCount(); ++i)
    {
        auto pt = clone->GetAnchorPoint(i);
        pt[0] = center[0] + (pt[0] - center[0]) * factor;
        pt[1] = center[1] + (pt[1] - center[1]) * factor;
        pt[2] = center[2] + (pt[2] - center[2]) * factor;
        clone->SetAnchorPoint(i, pt);
    }

    clone->GenerateProfilePoints();
    return clone;
}

std::unique_ptr<xq_LumenProfile> xq_SegmentationUtils::CreateEditableProfile(
    std::string_view contourType,
    const mitk::Point3D& center)
{
    if (contourType == "Manual")
    {
        auto profile = std::make_unique<xq_PolygonalProfile>();
        profile->SetProfileCenter(center);
        profile->SetMethod("manual");
        return profile;
    }

    if (contourType == "SplinePolygon")
    {
        auto profile = std::make_unique<xq_SplineProfile>();
        profile->SetProfileCenter(center);
        profile->SetMethod("splinepolygon");
        return profile;
    }

    return nullptr;
}

std::unique_ptr<xq_LumenProfile> xq_SegmentationUtils::CreateProfileFromContourPoints(
    const std::vector<mitk::Point3D>& contourPoints,
    std::string_view method,
    std::string_view contourType)
{
    if (contourPoints.size() < kMinContourPoints)
        return nullptr;

    auto center = ComputeContourCentroid(contourPoints);
    auto profile = CreateEditableProfile(
        contourType == "SplinePolygon" ? "SplinePolygon" : "Manual",
        center);
    if (!profile)
        return nullptr;

    profile->SetMethod(method);
    profile->SetAnchorPoints(contourPoints);
    return profile;
}

bool xq_SegmentationUtils::FindNearestPlacementFrame(
    const std::vector<xq_ProfilePlacementFrame>& frames,
    const mitk::Point3D& referencePoint,
    xq_ProfilePlacementFrame& outFrame)
{
    int frameIndex = -1;
    if (!FindNearestPlacementFrameIndex(frames, referencePoint, frameIndex))
        return false;

    outFrame = frames[frameIndex];
    return true;
}

bool xq_SegmentationUtils::FindNearestPlacementFrameIndex(
    const std::vector<xq_ProfilePlacementFrame>& frames,
    const mitk::Point3D& referencePoint,
    int& outFrameIndex,
    const std::set<int>* excludedFrameIndices)
{
    static const std::set<int> kNoExcludedFrames;
    outFrameIndex = findNearestAvailableFrameIndex(
        frames,
        excludedFrameIndices ? *excludedFrameIndices : kNoExcludedFrames,
        referencePoint);
    return outFrameIndex >= 0;
}

mitk::PlaneGeometry::Pointer xq_SegmentationUtils::CreatePlacementPlane(
    const xq_ProfilePlacementFrame& frame,
    const mitk::PlaneGeometry* referencePlane)
{
    mitk::Vector3D fallbackNormal;
    fallbackNormal.Fill(0.0);
    fallbackNormal[2] = 1.0;

    const auto normal = normalizedOrFallback(frame.tangent, fallbackNormal);
    auto right = normalizedOrFallback(frame.rotation, xq_SpatialMath::ComputeOrthogonalVector(normal));
    auto bottom = xq_SpatialMath::CrossProduct3D(normal, right);
    if (vectorLength(bottom) <= kEpsilon)
    {
        right = xq_SpatialMath::ComputeOrthogonalVector(normal);
        bottom = xq_SpatialMath::CrossProduct3D(normal, right);
    }
    bottom = xq_SpatialMath::UnitVector(bottom);
    right = xq_SpatialMath::UnitVector(xq_SpatialMath::CrossProduct3D(bottom, normal));

    auto matrix = vtkSmartPointer<vtkMatrix4x4>::New();
    matrix->Identity();
    for (int row = 0; row < 3; ++row)
    {
        matrix->SetElement(row, 0, right[row]);
        matrix->SetElement(row, 1, bottom[row]);
        matrix->SetElement(row, 2, normal[row]);
        matrix->SetElement(row, 3, frame.position[row]);
    }

    auto plane = referencePlane ? referencePlane->Clone() : mitk::PlaneGeometry::New();
    plane->SetIndexToWorldTransformByVtkMatrix(matrix);

    if (!referencePlane)
    {
        mitk::Vector3D spacing;
        spacing.Fill(1.0);
        plane->SetSpacing(spacing);
        plane->SetBounds(kDefaultPlaneBounds);
    }

    return plane;
}

mitk::PlaneGeometry::Pointer xq_SegmentationUtils::CreateReslicePlaneGeometry(
    const xq_ProfilePlacementFrame& frame,
    mitk::BaseData* baseData,
    double size,
    bool useOnlyMinimumSpacing)
{
    auto planeGeometry = CreatePlacementPlane(frame);
    if (planeGeometry.IsNull())
        return planeGeometry;

    mitk::Vector3D spacing;
    spacing.Fill(1.0);

    if (auto* image = dynamic_cast<mitk::Image*>(baseData))
    {
        auto imageGeometry = image->GetTimeGeometry()->GetGeometryForTimeStep(0);
        auto imageSpacing = imageGeometry->GetSpacing();

        if (useOnlyMinimumSpacing)
        {
            const auto minSpacing = std::min(imageSpacing[0], std::min(imageSpacing[1], imageSpacing[2]));
            spacing.Fill(minSpacing);
        }
        else
        {
            auto right = planeGeometry->GetAxisVector(0);
            auto bottom = planeGeometry->GetAxisVector(1);
            right.Normalize();
            bottom.Normalize();
            right *= size;
            bottom *= size;

            mitk::Vector3D rightInIndex;
            mitk::Vector3D bottomInIndex;
            imageGeometry->WorldToIndex(right, rightInIndex);
            imageGeometry->WorldToIndex(bottom, bottomInIndex);

            if (rightInIndex.GetNorm() > kEpsilon)
                spacing[0] = size / rightInIndex.GetNorm();
            if (bottomInIndex.GetNorm() > kEpsilon)
                spacing[1] = size / bottomInIndex.GetNorm();
            spacing[2] = 1.0;
        }
    }

    auto right = planeGeometry->GetAxisVector(0);
    auto bottom = planeGeometry->GetAxisVector(1);
    right.Normalize();
    bottom.Normalize();

    mitk::Point3D origin;
    origin[0] = frame.position[0] - right[0] * size * 0.5 - bottom[0] * size * 0.5;
    origin[1] = frame.position[1] - right[1] * size * 0.5 - bottom[1] * size * 0.5;
    origin[2] = frame.position[2] - right[2] * size * 0.5 - bottom[2] * size * 0.5;

    planeGeometry->SetOrigin(origin);
    planeGeometry->SetSpacing(spacing);

    const double width = size / spacing[0];
    const double height = size / spacing[1];
    const mitk::ScalarType bounds[6] = {0, width, 0, height, 0, 1};
    planeGeometry->SetBounds(bounds);

    if (dynamic_cast<mitk::Image*>(baseData))
        planeGeometry->SetImageGeometry(true);

    return planeGeometry;
}

mitk::ProportionalTimeGeometry::Pointer xq_SegmentationUtils::CreateSlicedGeometry(
    const std::vector<xq_ProfilePlacementFrame>& frames,
    mitk::BaseData* baseData,
    double size,
    bool useOnlyMinimumSpacing)
{
    auto propTimeGeom = mitk::ProportionalTimeGeometry::New();
    if (frames.empty())
        return propTimeGeom;

    auto slicedGeometry = mitk::SlicedGeometry3D::New();
    slicedGeometry->SetEvenlySpaced(false);
    slicedGeometry->InitializeSlicedGeometry(frames.size());

    for (size_t i = 0; i < frames.size(); ++i)
    {
        auto planeGeometry = CreateReslicePlaneGeometry(frames[i], baseData, size, useOnlyMinimumSpacing);
        slicedGeometry->SetPlaneGeometry(planeGeometry, static_cast<unsigned int>(i));
    }

    if (baseData)
    {
        auto geometry = baseData->GetTimeGeometry()->GetGeometryForTimeStep(0);
        slicedGeometry->SetReferenceGeometry(geometry);
        slicedGeometry->SetBounds(geometry->GetBounds());
        slicedGeometry->SetOrigin(geometry->GetOrigin());
        slicedGeometry->SetIndexToWorldTransform(geometry->GetIndexToWorldTransform());
    }

    propTimeGeom->Initialize(slicedGeometry, 1);
    return propTimeGeom;
}

void xq_SegmentationUtils::ApplyPlacementFrame(
    xq_LumenProfile* profile,
    const xq_ProfilePlacementFrame& frame,
    const mitk::PlaneGeometry* referencePlane)
{
    if (!profile)
        return;

    mitk::Vector3D sourceAxis0;
    mitk::Vector3D sourceAxis1;
    mitk::Vector3D sourceNormal;
    resolveProfileBasis(profile, sourceAxis0, sourceAxis1, sourceNormal);

    auto targetPlane = CreatePlacementPlane(frame, referencePlane);
    mitk::Vector3D targetAxis0;
    mitk::Vector3D targetAxis1;
    mitk::Vector3D targetNormal;
    resolveFrameBasis(targetPlane, targetAxis0, targetAxis1, targetNormal);

    const auto sourceCenter = profile->GetProfileCenter();
    for (int i = 0; i < profile->GetAnchorPointCount(); ++i)
    {
        const auto sourcePoint = profile->GetAnchorPoint(i);
        const auto delta = pointDifference(sourcePoint, sourceCenter);
        const double u = dotProduct(delta, sourceAxis0);
        const double v = dotProduct(delta, sourceAxis1);
        const double w = dotProduct(delta, sourceNormal);
        profile->SetAnchorPoint(i, pointTranslated(
            frame.position, targetAxis0, targetAxis1, targetNormal, u, v, w));
    }

    profile->SetPathPosIndex(frame.pathPosIndex);
    profile->SetProfileCenter(frame.position);
    profile->SetSlicePlane(targetPlane);
    profile->GenerateProfilePoints();
}

bool xq_SegmentationUtils::RebindProfileGroupToPlacementFrames(
    xq_ProfileGroup* group,
    const std::vector<xq_ProfilePlacementFrame>& frames)
{
    if (!group || frames.empty())
        return false;

    struct PendingProfile
    {
        int sourceIndex = -1;
        int targetIndex = -1;
        std::unique_ptr<xq_LumenProfile> profile;
    };

    std::vector<PendingProfile> reboundProfiles;
    std::set<int> usedFrameIndices;
    std::set<int> reservedTargetIndices;
    const auto sourceIndices = group->GetProfilePathIndices();
    reboundProfiles.reserve(sourceIndices.size());

    for (const int sourceIndex : sourceIndices)
    {
        auto* sourceProfile = group->GetProfileAtPathPos(sourceIndex);
        if (!sourceProfile)
            continue;

        auto reboundProfile = sourceProfile->Duplicate();
        if (!reboundProfile)
            continue;

        int frameIndex = -1;
        if (!FindNearestPlacementFrameIndex(
                frames, sourceProfile->GetProfileCenter(), frameIndex, &usedFrameIndices))
        {
            MITK_WARN << "xq_SegmentationUtils::RebindProfileGroupToPlacementFrames: "
                      << "cannot resolve a unique placement frame for source profile at index "
                      << sourceIndex << "; leaving the group unchanged.";
            return false;
        }

        usedFrameIndices.insert(frameIndex);
        const int targetIndex = frames[frameIndex].pathPosIndex;
        if (!reservedTargetIndices.insert(targetIndex).second)
        {
            MITK_WARN << "xq_SegmentationUtils::RebindProfileGroupToPlacementFrames: "
                      << "placement frames map multiple profiles onto canonical slot "
                      << targetIndex << "; leaving the group unchanged.";
            return false;
        }

        reboundProfile->SetPathPosIndex(targetIndex);
        reboundProfile->SetSlicePlane(CreatePlacementPlane(frames[frameIndex]));

        reboundProfiles.push_back({
            sourceIndex,
            targetIndex,
            std::move(reboundProfile)
        });
    }

    for (const int sourceIndex : sourceIndices)
        group->RemoveProfile(sourceIndex);

    for (auto& rebound : reboundProfiles)
        group->AppendProfile(rebound.profile.release(), rebound.targetIndex);

    return true;
}

bool xq_SegmentationUtils::RequiresPathPlacement(const xq_ProfileGroup* group)
{
    return group != nullptr && !group->GetAttribute("path_name").empty();
}

bool xq_SegmentationUtils::ResolveCanonicalPathPosIndex(
    const xq_ProfileGroup* group,
    const xq_ProfilePlacementFrame* placementFrame,
    int& outPathPosIndex)
{
    if (!group)
        return false;

    if (placementFrame && placementFrame->pathPosIndex >= 0)
    {
        outPathPosIndex = placementFrame->pathPosIndex;
        return true;
    }

    if (RequiresPathPlacement(group))
        return false;

    const auto indices = group->GetProfilePathIndices();
    outPathPosIndex = indices.empty() ? 0 : (indices.back() + 1);
    return true;
}

std::string xq_SegmentationUtils::GetLoftReadinessBlockingReason(
    const xq_ProfileGroup* group,
    unsigned int timeStep)
{
    if (!group)
        return "No canonical profile group is available for lofting.";

    if (group->GetProfileCount(timeStep) < 2)
        return "At least two canonical profiles are required before lofting.";

    const auto unresolvedIndices = group->GetUnresolvedProfilePathIndices(timeStep);
    if (!unresolvedIndices.empty())
    {
        std::ostringstream message;
        message << "Profiles ";
        for (size_t i = 0; i < unresolvedIndices.size(); ++i)
        {
            if (i > 0)
                message << ", ";
            message << unresolvedIndices[i];
        }
        message << " are missing resolved placement metadata and cannot be lofted.";
        return message.str();
    }

    const auto missingIndices = group->GetMissingProfilePathIndices(timeStep);
    if (missingIndices.empty())
        return {};

    std::ostringstream message;
    message << "Canonical profile chain has path gaps at slots ";
    for (size_t i = 0; i < missingIndices.size(); ++i)
    {
        if (i > 0)
            message << ", ";
        message << missingIndices[i];
    }
    message << " and cannot be lofted.";
    return message.str();
}

std::string xq_SegmentationUtils::GetModelingPhaseBoundaryBlockingReason(
    const xq_ProfileGroup* group,
    unsigned int timeStep)
{
    if (!group)
        return "No canonical profile group is available.";

    if (group->IsLoftCacheDirty(timeStep))
        return "Profile group has been modified since the last loft. "
               "Regenerate the loft in 2D Segmentation before modeling.";

    if (!group->GetLoftedMesh(timeStep))
        return "No finalized loft surface exists for this profile group. "
               "Run Loft in 2D Segmentation to finalize segmentation before modeling.";

    return {};
}

xq_ProfileGroupReadinessReport xq_SegmentationUtils::BuildReadinessReport(
    const xq_ProfileGroup* group,
    unsigned int timeStep)
{
    xq_ProfileGroupReadinessReport report;

    if (!group)
    {
        report.errors.push_back("No profile group provided.");
        return report;
    }

    report.profileCount = group->GetProfileCount(timeStep);
    auto missingIndices = group->GetMissingProfilePathIndices(timeStep);
    report.missingCount = static_cast<int>(missingIndices.size());

    if (report.missingCount > 0)
    {
        std::ostringstream msg;
        msg << report.missingCount << " missing profile(s) at path slots: ";
        for (size_t i = 0; i < missingIndices.size(); ++i)
        {
            if (i > 0) msg << ", ";
            msg << missingIndices[i];
        }
        report.warnings.push_back(msg.str());
    }

    // Gather loft readiness
    std::string loftReason = GetLoftReadinessBlockingReason(group, timeStep);
    report.loftReady = loftReason.empty();
    if (!loftReason.empty())
        report.errors.push_back(loftReason);

    // Gather modeling readiness
    std::string modelingReason = GetModelingPhaseBoundaryBlockingReason(group, timeStep);
    report.modelingReady = modelingReason.empty();
    if (!modelingReason.empty())
    {
        if (report.loftReady)
            report.errors.push_back(modelingReason);
        else
            report.warnings.push_back(modelingReason);
    }

    return report;
}

xq_CanonicalWritebackWarningReason xq_SegmentationUtils::GetCanonicalWritebackWarningReason(
    const xq_ProfileGroup* group,
    bool hasReferencePlane,
    bool hasPlacementFrame)
{
    if (!RequiresPathPlacement(group))
        return xq_CanonicalWritebackWarningReason::None;

    if (!hasReferencePlane)
        return xq_CanonicalWritebackWarningReason::SlicePlacementUnavailable;

    if (!hasPlacementFrame)
        return xq_CanonicalWritebackWarningReason::PlacementUnresolved;

    return xq_CanonicalWritebackWarningReason::None;
}

std::string_view xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
    xq_CanonicalWritebackWarningContext context,
    xq_CanonicalWritebackWarningReason reason)
{
    if (reason == xq_CanonicalWritebackWarningReason::None)
        return {};

    if (reason == xq_CanonicalWritebackWarningReason::ExtractionYieldedNoValidContour)
    {
        if (context == xq_CanonicalWritebackWarningContext::ThresholdContour)
            return "The threshold-based extraction did not produce a single valid contour on the current slice. The contour may be disconnected (multiple loops) or degenerate. Try adjusting the threshold value.";
        return "The surface extraction did not yield a single valid contour on the current slice. The intersection may be disconnected (multiple loops) or degenerate. The surface preview will still be created.";
    }

    if (context == xq_CanonicalWritebackWarningContext::ThresholdContour)
    {
        return "Cannot convert the threshold contour into a canonical profile because the current centerline placement could not be resolved.";
    }

    if (reason == xq_CanonicalWritebackWarningReason::SlicePlacementUnavailable)
    {
        return "Cannot write the 3D segmentation result back into the canonical profile group because the current centerline slice placement is unavailable. The surface preview will still be created.";
    }

    return "Cannot write the 3D segmentation result back into the canonical profile group because the current centerline placement could not be resolved. The surface preview will still be created.";
}

xq_CanonicalWritebackViewDecision xq_SegmentationUtils::GetThresholdContourWritebackDecision(
    const xq_ProfileGroup* group,
    bool hasReferencePlane,
    bool hasPlacementFrame)
{
    const auto warningReason = GetCanonicalWritebackWarningReason(
        group, hasReferencePlane, hasPlacementFrame);
    if (warningReason == xq_CanonicalWritebackWarningReason::None)
        return makeCanonicalWritebackViewDecision(warningReason, xq_CanonicalWritebackFlow::Proceed);

    return makeCanonicalWritebackViewDecision(warningReason, xq_CanonicalWritebackFlow::WarnAndReturn);
}

xq_CanonicalWritebackViewDecision xq_SegmentationUtils::GetAutoSegmentationWritebackDecision(
    const xq_ProfileGroup* group,
    bool hasReferencePlane,
    bool hasPlacementFrame)
{
    const auto warningReason = GetCanonicalWritebackWarningReason(
        group, hasReferencePlane, hasPlacementFrame);
    if (warningReason == xq_CanonicalWritebackWarningReason::None)
        return makeCanonicalWritebackViewDecision(warningReason, xq_CanonicalWritebackFlow::Proceed);

    return makeCanonicalWritebackViewDecision(warningReason, xq_CanonicalWritebackFlow::WarnAndContinue);
}

xq_CanonicalWritebackViewDecision xq_SegmentationUtils::GetAutoSegmentationWritebackDecisionForPlacement(
    const xq_ProfileGroup* group,
    const mitk::PlaneGeometry* referencePlane,
    const xq_ProfilePlacementFrame* placementFrame)
{
    return GetAutoSegmentationWritebackDecision(
        group,
        referencePlane != nullptr,
        placementFrame != nullptr && placementFrame->pathPosIndex >= 0);
}

xq_CanonicalWritebackViewDecision xq_SegmentationUtils::ResolveAutoSegmentationWritebackDecisionForPlacementFrames(
    const xq_ProfileGroup* group,
    const mitk::PlaneGeometry* referencePlane,
    const std::vector<xq_ProfilePlacementFrame>& placementFrames,
    xq_ProfilePlacementFrame* resolvedPlacementFrame)
{
    xq_ProfilePlacementFrame placementFrame;
    const bool hasPlacementFrame =
        referencePlane != nullptr &&
        FindNearestPlacementFrame(placementFrames, referencePlane->GetOrigin(), placementFrame);

    if (resolvedPlacementFrame)
    {
        if (hasPlacementFrame)
            *resolvedPlacementFrame = placementFrame;
        else
            *resolvedPlacementFrame = xq_ProfilePlacementFrame();
    }

    return GetAutoSegmentationWritebackDecisionForPlacement(
        group,
        referencePlane,
        hasPlacementFrame ? &placementFrame : nullptr);
}

xq_CanonicalWritebackViewDecision xq_SegmentationUtils::GetExtractionFailedWritebackDecision()
{
    return makeCanonicalWritebackViewDecision(
        xq_CanonicalWritebackWarningReason::ExtractionYieldedNoValidContour,
        xq_CanonicalWritebackFlow::WarnAndReturn);
}

xq_PreprocTargetResolution xq_SegmentationUtils::ResolvePreprocessingTarget(
    const xq_ProfileGroup* group,
    unsigned int timeStep)
{
    xq_PreprocTargetResolution result;

    if (!group)
    {
        result.state = xq_PreprocTargetState::NoTargetAvailable;
        result.blockingReason =
            "No segmentation group is selected as a preprocessing target.";
        return result;
    }

    if (RequiresPathPlacement(group))
    {
        const auto loftReason = GetLoftReadinessBlockingReason(group, timeStep);
        if (!loftReason.empty())
        {
            result.state = xq_PreprocTargetState::TargetIneligible;
            result.blockingReason = loftReason;
            return result;
        }
    }

    result.state = xq_PreprocTargetState::TargetReady;
    return result;
}

void xq_SegmentationUtils::OrientPresetProfileToPlacement(
    xq_LumenProfile* profile,
    double primarySize,
    double secondarySize)
{
    if (!profile || profile->GetSlicePlane().IsNull())
        return;

    auto center = profile->GetProfileCenter();
    auto axis0 = profile->GetSlicePlane()->GetAxisVector(0);
    auto axis1 = profile->GetSlicePlane()->GetAxisVector(1);

    mitk::Vector3D fallbackAxis0;
    fallbackAxis0.Fill(0.0);
    fallbackAxis0[0] = 1.0;

    axis0 = normalizedOrFallback(axis0, fallbackAxis0);
    axis1 = normalizedOrFallback(axis1, xq_SpatialMath::ComputeOrthogonalVector(axis0));

    if (profile->GetProfileKind() == "Circle" && profile->GetAnchorPointCount() >= 2)
    {
        mitk::Point3D radiusPoint;
        radiusPoint[0] = center[0] + primarySize * axis0[0];
        radiusPoint[1] = center[1] + primarySize * axis0[1];
        radiusPoint[2] = center[2] + primarySize * axis0[2];
        profile->SetAnchorPoint(1, radiusPoint);
        profile->GenerateProfilePoints();
    }
    else if (profile->GetProfileKind() == "Ellipse" && profile->GetAnchorPointCount() >= 3)
    {
        mitk::Point3D majorPoint;
        majorPoint[0] = center[0] + primarySize * axis0[0];
        majorPoint[1] = center[1] + primarySize * axis0[1];
        majorPoint[2] = center[2] + primarySize * axis0[2];

        mitk::Point3D minorPoint;
        minorPoint[0] = center[0] + secondarySize * axis1[0];
        minorPoint[1] = center[1] + secondarySize * axis1[1];
        minorPoint[2] = center[2] + secondarySize * axis1[2];

        profile->SetAnchorPoint(1, majorPoint);
        profile->SetAnchorPoint(2, minorPoint);
        profile->GenerateProfilePoints();
    }
}

xq_ProfileStatistics xq_SegmentationUtils::ComputeProfileStatistics(
    xq_LumenProfile* profile)
{
    xq_ProfileStatistics stats;
    if (!profile)
        return stats;

    const auto contourPoints = profile->GetProfilePoints();
    stats.pointCount = static_cast<int>(contourPoints.size());
    if (contourPoints.empty())
        return stats;

    stats.perimeter = CalculateContourPerimeter(contourPoints);
    stats.area = CalculateContourArea(contourPoints);

    stats.boundingBox = {{
        contourPoints[0][0], contourPoints[0][0],
        contourPoints[0][1], contourPoints[0][1],
        contourPoints[0][2], contourPoints[0][2]
    }};

    for (size_t i = 1; i < contourPoints.size(); ++i)
    {
        for (int d = 0; d < 3; ++d)
        {
            stats.boundingBox[d * 2] = std::min(stats.boundingBox[d * 2], contourPoints[i][d]);
            stats.boundingBox[d * 2 + 1] = std::max(stats.boundingBox[d * 2 + 1], contourPoints[i][d]);
        }
    }

    return stats;
}

std::unique_ptr<xq_LumenProfile> xq_SegmentationUtils::SmoothProfile(
    xq_LumenProfile* profile,
    int iterations,
    double relaxation)
{
    if (!profile || iterations < 1 || relaxation <= 0.0)
        return nullptr;

    const auto contourPoints = profile->GetProfilePoints();
    if (contourPoints.size() < kMinContourPoints)
        return nullptr;

    auto clone = profile->Duplicate();
    if (!clone)
        return nullptr;

    clone->SetAnchorPoints(smoothClosedContour(contourPoints, iterations, relaxation));
    clone->SetMethod(profile->GetMethod());
    return clone;
}

std::unique_ptr<xq_LumenProfile> xq_SegmentationUtils::ResampleProfile(
    xq_LumenProfile* profile,
    int targetPointCount)
{
    if (!profile || targetPointCount < kMinContourPoints)
        return nullptr;

    auto clone = profile->Duplicate();
    if (!clone)
        return nullptr;

    const auto kind = profile->GetProfileKind();
    if (kind == "Circle" || kind == "Ellipse")
    {
        clone->SetSubdivisionCount(targetPointCount);
        clone->GenerateProfilePoints();
        return clone;
    }

    const auto contourPoints = profile->GetProfilePoints();
    if (contourPoints.size() < kMinContourPoints)
        return nullptr;

    clone->SetAnchorPoints(ResampleContour(contourPoints, targetPointCount));
    clone->SetMethod(profile->GetMethod());
    return clone;
}
