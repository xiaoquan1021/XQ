#include "xq_GeometryUtils.h"

#include <vtkBooleanOperationPolyDataFilter.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkDecimatePro.h>
#include <vtkFillHolesFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkTriangleFilter.h>
#include <vtkCleanPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkMath.h>

#include <cmath>

namespace
{

constexpr double kEpsilon = 1e-10;

struct TriangulatedPair
{
    vtkSmartPointer<vtkTriangleFilter> triA;
    vtkSmartPointer<vtkTriangleFilter> triB;
};

[[nodiscard]] TriangulatedPair triangulateInputs(vtkPolyData* a, vtkPolyData* b)
{
    auto triA = vtkSmartPointer<vtkTriangleFilter>::New();
    triA->SetInputData(a);
    triA->Update();

    auto triB = vtkSmartPointer<vtkTriangleFilter>::New();
    triB->SetInputData(b);
    triB->Update();

    return {triA, triB};
}

[[nodiscard]] vtkSmartPointer<vtkPolyData> deepCopyOutput(vtkAlgorithm* algo)
{
    auto result = vtkSmartPointer<vtkPolyData>::New();
    result->DeepCopy(vtkPolyData::SafeDownCast(algo->GetOutputDataObject(0)));
    return result;
}

} // anonymous namespace

namespace xq::geometry
{

vtkSmartPointer<vtkPolyData> booleanUnion(vtkPolyData* a, vtkPolyData* b)
{
    if (!a || !b)
        return nullptr;

    auto [triA, triB] = triangulateInputs(a, b);

    auto boolFilter = vtkSmartPointer<vtkBooleanOperationPolyDataFilter>::New();
    boolFilter->SetOperationToUnion();
    boolFilter->SetInputConnection(0, triA->GetOutputPort());
    boolFilter->SetInputConnection(1, triB->GetOutputPort());
    boolFilter->Update();

    return deepCopyOutput(boolFilter);
}

vtkSmartPointer<vtkPolyData> booleanIntersection(vtkPolyData* a, vtkPolyData* b)
{
    if (!a || !b)
        return nullptr;

    auto [triA, triB] = triangulateInputs(a, b);

    auto boolFilter = vtkSmartPointer<vtkBooleanOperationPolyDataFilter>::New();
    boolFilter->SetOperationToIntersection();
    boolFilter->SetInputConnection(0, triA->GetOutputPort());
    boolFilter->SetInputConnection(1, triB->GetOutputPort());
    boolFilter->Update();

    return deepCopyOutput(boolFilter);
}

vtkSmartPointer<vtkPolyData> booleanSubtract(vtkPolyData* a, vtkPolyData* b)
{
    if (!a || !b)
        return nullptr;

    auto [triA, triB] = triangulateInputs(a, b);

    auto boolFilter = vtkSmartPointer<vtkBooleanOperationPolyDataFilter>::New();
    boolFilter->SetOperationToDifference();
    boolFilter->SetInputConnection(0, triA->GetOutputPort());
    boolFilter->SetInputConnection(1, triB->GetOutputPort());
    boolFilter->Update();

    return deepCopyOutput(boolFilter);
}

vtkSmartPointer<vtkPolyData> smoothSurface(vtkPolyData* polyData,
                                            int iterations,
                                            double relaxation)
{
    if (!polyData)
        return nullptr;

    auto smoother = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
    smoother->SetInputData(polyData);
    smoother->SetNumberOfIterations(iterations);
    smoother->SetRelaxationFactor(relaxation);
    smoother->FeatureEdgeSmoothingOff();
    smoother->BoundarySmoothingOn();
    smoother->Update();

    return deepCopyOutput(smoother);
}

vtkSmartPointer<vtkPolyData> decimateSurface(vtkPolyData* polyData,
                                              double targetReduction)
{
    if (!polyData)
        return nullptr;

    auto triFilter = vtkSmartPointer<vtkTriangleFilter>::New();
    triFilter->SetInputData(polyData);
    triFilter->Update();

    auto decimate = vtkSmartPointer<vtkDecimatePro>::New();
    decimate->SetInputConnection(triFilter->GetOutputPort());
    decimate->SetTargetReduction(targetReduction);
    decimate->PreserveTopologyOn();
    decimate->Update();

    return deepCopyOutput(decimate);
}

vtkSmartPointer<vtkPolyData> fillHoles(vtkPolyData* polyData,
                                        double maxHoleSize)
{
    if (!polyData)
        return nullptr;

    auto filter = vtkSmartPointer<vtkFillHolesFilter>::New();
    filter->SetInputData(polyData);
    filter->SetHoleSize(maxHoleSize);
    filter->Update();

    return deepCopyOutput(filter);
}

vtkSmartPointer<vtkPolyData> loftContours(const std::vector<vtkPolyData*>& contours,
                                           int numPoints,
                                           int /*splineType*/)
{
    if (contours.size() < 2)
        return nullptr;

    // Resample each contour to uniform point count
    std::vector<std::vector<double>> sampledContours;
    sampledContours.reserve(contours.size());

    for (auto* contour : contours)
    {
        if (!contour || contour->GetNumberOfPoints() < 2)
            return nullptr;

        auto* pts = contour->GetPoints();
        const vtkIdType nPts = pts->GetNumberOfPoints();

        // Cumulative arc lengths
        std::vector<double> arcLength(nPts, 0.0);
        for (vtkIdType i = 1; i < nPts; ++i)
        {
            double p0[3], p1[3];
            pts->GetPoint(i - 1, p0);
            pts->GetPoint(i, p1);
            arcLength[i] = arcLength[i - 1] + std::sqrt(vtkMath::Distance2BetweenPoints(p0, p1));
        }

        const double totalLength = arcLength[nPts - 1];
        if (totalLength < kEpsilon)
            return nullptr;

        std::vector<double> resampled(numPoints * 3);
        for (int j = 0; j < numPoints; ++j)
        {
            const double targetLen = totalLength * j / (numPoints - 1);

            vtkIdType seg = 0;
            for (vtkIdType i = 1; i < nPts; ++i)
            {
                if (arcLength[i] >= targetLen)
                {
                    seg = i - 1;
                    break;
                }
            }
            // Clamp to valid segment range to avoid out-of-bounds access
            if (seg >= nPts - 1) seg = nPts - 2;

            const double segLen = arcLength[seg + 1] - arcLength[seg];
            const double t = (segLen > kEpsilon) ? (targetLen - arcLength[seg]) / segLen : 0.0;

            double p0[3], p1[3];
            pts->GetPoint(seg, p0);
            pts->GetPoint(seg + 1, p1);

            resampled[j * 3 + 0] = p0[0] + t * (p1[0] - p0[0]);
            resampled[j * 3 + 1] = p0[1] + t * (p1[1] - p0[1]);
            resampled[j * 3 + 2] = p0[2] + t * (p1[2] - p0[2]);
        }
        sampledContours.push_back(std::move(resampled));
    }

    // Build lofted surface with triangle strips between consecutive contours
    auto allPoints = vtkSmartPointer<vtkPoints>::New();
    auto polys = vtkSmartPointer<vtkCellArray>::New();

    for (const auto& sc : sampledContours)
    {
        for (int j = 0; j < numPoints; ++j)
        {
            allPoints->InsertNextPoint(sc[j * 3], sc[j * 3 + 1], sc[j * 3 + 2]);
        }
    }

    for (size_t c = 0; c < sampledContours.size() - 1; ++c)
    {
        const auto offset0 = static_cast<vtkIdType>(c * numPoints);
        const auto offset1 = static_cast<vtkIdType>((c + 1) * numPoints);

        for (int j = 0; j < numPoints - 1; ++j)
        {
            std::array<vtkIdType, 3> tri1 = {offset0 + j, offset1 + j, offset1 + j + 1};
            polys->InsertNextCell(3, tri1.data());

            std::array<vtkIdType, 3> tri2 = {offset0 + j, offset1 + j + 1, offset0 + j + 1};
            polys->InsertNextCell(3, tri2.data());
        }
    }

    auto surface = vtkSmartPointer<vtkPolyData>::New();
    surface->SetPoints(allPoints);
    surface->SetPolys(polys);

    auto cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
    cleaner->SetInputData(surface);
    cleaner->Update();

    return deepCopyOutput(cleaner);
}

vtkSmartPointer<vtkPolyData> computeNormals(vtkPolyData* polyData)
{
    if (!polyData)
        return nullptr;

    auto normalsFilter = vtkSmartPointer<vtkPolyDataNormals>::New();
    normalsFilter->SetInputData(polyData);
    normalsFilter->ComputePointNormalsOn();
    normalsFilter->ComputeCellNormalsOn();
    normalsFilter->SplittingOff();
    normalsFilter->ConsistencyOn();
    normalsFilter->AutoOrientNormalsOn();
    normalsFilter->Update();

    return deepCopyOutput(normalsFilter);
}

} // namespace xq::geometry
