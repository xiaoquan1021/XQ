#include "xq_Seg3DUtils.h"

#include <vtkDecimatePro.h>
#include <vtkFillHolesFilter.h>
#include <vtkImageData.h>
#include <vtkImageThreshold.h>
#include <vtkMarchingCubes.h>
#include <vtkPointData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmoothPolyDataFilter.h>

#include <queue>
#include <vector>

namespace
{

constexpr double kBinaryInValue  = 1.0;
constexpr double kBinaryOutValue = 0.0;
constexpr double kIsoSurface     = 0.5;

auto thresholdAndExtract(vtkImageData* imageData,
                         double lower,
                         double upper) -> vtkSmartPointer<vtkPolyData>
{
    auto threshold = vtkSmartPointer<vtkImageThreshold>::New();
    threshold->SetInputData(imageData);
    threshold->ThresholdBetween(lower, upper);
    threshold->SetInValue(kBinaryInValue);
    threshold->SetOutValue(kBinaryOutValue);
    threshold->SetOutputScalarTypeToDouble();
    threshold->Update();

    auto mc = vtkSmartPointer<vtkMarchingCubes>::New();
    mc->SetInputData(threshold->GetOutput());
    mc->SetValue(0, kIsoSurface);
    mc->ComputeNormalsOn();
    mc->Update();

    return mc->GetOutput();
}

} // namespace

// ---------------------------------------------------------------------------
// ThresholdSegmentation
// ---------------------------------------------------------------------------

vtkSmartPointer<vtkPolyData> xq_Seg3DUtils::ThresholdSegmentation(
    vtkImageData* imageData,
    double lowerThreshold,
    double upperThreshold)
{
    if (!imageData)
        return nullptr;

    return thresholdAndExtract(imageData, lowerThreshold, upperThreshold);
}

// ---------------------------------------------------------------------------
// RegionGrowingSegmentation
// ---------------------------------------------------------------------------

vtkSmartPointer<vtkPolyData> xq_Seg3DUtils::RegionGrowingSegmentation(
    vtkImageData* imageData,
    const std::vector<mitk::Point3D>& seedPoints,
    double lowerThreshold,
    double upperThreshold)
{
    if (!imageData || seedPoints.empty())
        return nullptr;

    int dims[3];
    imageData->GetDimensions(dims);
    double spacing[3], origin[3];
    imageData->GetSpacing(spacing);
    imageData->GetOrigin(origin);

    const vtkIdType totalVoxels =
        static_cast<vtkIdType>(dims[0]) * dims[1] * dims[2];

    // Create binary mask image (same geometry as input)
    auto mask = vtkSmartPointer<vtkImageData>::New();
    mask->SetDimensions(dims);
    mask->SetSpacing(spacing);
    mask->SetOrigin(origin);
    mask->AllocateScalars(VTK_DOUBLE, 1);

    if (!mask->GetPointData() || !mask->GetPointData()->GetScalars())
        return nullptr;

    // Initialize mask to zero
    for (vtkIdType i = 0; i < totalVoxels; ++i)
        mask->GetPointData()->GetScalars()->SetTuple1(i, kBinaryOutValue);

    // Visited array for BFS
    std::vector<bool> visited(totalVoxels, false);

    // Lambda: flat index from (x,y,z) — VTK X-fastest order
    auto flatIdx = [&](int x, int y, int z) -> vtkIdType {
        return static_cast<vtkIdType>(x)
             + static_cast<vtkIdType>(y) * dims[0]
             + static_cast<vtkIdType>(z) * dims[0] * dims[1];
    };

    // BFS queue holds voxel indices (x,y,z)
    struct Voxel { int x, y, z; };
    std::queue<Voxel> queue;

    // Seed the queue: convert world coordinates to voxel indices
    for (const auto& worldPt : seedPoints)
    {
        int ix = static_cast<int>((worldPt[0] - origin[0]) / spacing[0] + 0.5);
        int iy = static_cast<int>((worldPt[1] - origin[1]) / spacing[1] + 0.5);
        int iz = static_cast<int>((worldPt[2] - origin[2]) / spacing[2] + 0.5);

        if (ix < 0 || ix >= dims[0] ||
            iy < 0 || iy >= dims[1] ||
            iz < 0 || iz >= dims[2])
            continue;

        double val = imageData->GetScalarComponentAsDouble(ix, iy, iz, 0);
        if (val < lowerThreshold || val > upperThreshold)
            continue;

        vtkIdType fi = flatIdx(ix, iy, iz);
        if (!visited[fi])
        {
            visited[fi] = true;
            mask->SetScalarComponentFromDouble(ix, iy, iz, 0, kBinaryInValue);
            queue.push({ix, iy, iz});
        }
    }

    // 6-connected neighborhood offsets
    static const int dx[] = {-1, 1,  0, 0,  0, 0};
    static const int dy[] = { 0, 0, -1, 1,  0, 0};
    static const int dz[] = { 0, 0,  0, 0, -1, 1};

    // BFS flood fill
    while (!queue.empty())
    {
        Voxel v = queue.front();
        queue.pop();

        for (int n = 0; n < 6; ++n)
        {
            int nx = v.x + dx[n];
            int ny = v.y + dy[n];
            int nz = v.z + dz[n];

            if (nx < 0 || nx >= dims[0] ||
                ny < 0 || ny >= dims[1] ||
                nz < 0 || nz >= dims[2])
                continue;

            vtkIdType fi = flatIdx(nx, ny, nz);
            if (visited[fi])
                continue;

            double val = imageData->GetScalarComponentAsDouble(nx, ny, nz, 0);
            if (val < lowerThreshold || val > upperThreshold)
                continue;

            visited[fi] = true;
            mask->SetScalarComponentFromDouble(nx, ny, nz, 0, kBinaryInValue);
            queue.push({nx, ny, nz});
        }
    }

    // Extract surface from mask with marching cubes
    auto mc = vtkSmartPointer<vtkMarchingCubes>::New();
    mc->SetInputData(mask);
    mc->SetValue(0, kIsoSurface);
    mc->ComputeNormalsOn();
    mc->Update();

    return mc->GetOutput();
}

// ---------------------------------------------------------------------------
// MarchingCubes
// ---------------------------------------------------------------------------

vtkSmartPointer<vtkPolyData> xq_Seg3DUtils::MarchingCubes(
    vtkImageData* imageData,
    double isoValue)
{
    if (!imageData)
        return nullptr;

    auto mc = vtkSmartPointer<vtkMarchingCubes>::New();
    mc->SetInputData(imageData);
    mc->SetValue(0, isoValue);
    mc->ComputeNormalsOn();
    mc->Update();

    return mc->GetOutput();
}

// ---------------------------------------------------------------------------
// SmoothSurface
// ---------------------------------------------------------------------------

vtkSmartPointer<vtkPolyData> xq_Seg3DUtils::SmoothSurface(
    vtkSmartPointer<vtkPolyData> input,
    int iterations,
    double relaxationFactor)
{
    if (!input)
        return nullptr;

    auto smoother = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
    smoother->SetInputData(input);
    smoother->SetNumberOfIterations(iterations);
    smoother->SetRelaxationFactor(relaxationFactor);
    smoother->FeatureEdgeSmoothingOff();
    smoother->BoundarySmoothingOn();
    smoother->Update();

    return smoother->GetOutput();
}

// ---------------------------------------------------------------------------
// DecimateSurface
// ---------------------------------------------------------------------------

vtkSmartPointer<vtkPolyData> xq_Seg3DUtils::DecimateSurface(
    vtkSmartPointer<vtkPolyData> input,
    double targetReduction)
{
    if (!input)
        return nullptr;

    auto decimator = vtkSmartPointer<vtkDecimatePro>::New();
    decimator->SetInputData(input);
    decimator->SetTargetReduction(targetReduction);
    decimator->PreserveTopologyOn();
    decimator->Update();

    return decimator->GetOutput();
}

// ---------------------------------------------------------------------------
// FillHoles
// ---------------------------------------------------------------------------

vtkSmartPointer<vtkPolyData> xq_Seg3DUtils::FillHoles(
    vtkSmartPointer<vtkPolyData> input,
    double holeSize)
{
    if (!input)
        return nullptr;

    auto filler = vtkSmartPointer<vtkFillHolesFilter>::New();
    filler->SetInputData(input);
    filler->SetHoleSize(holeSize);
    filler->Update();

    return filler->GetOutput();
}

// ---------------------------------------------------------------------------
// ComputeNormals
// ---------------------------------------------------------------------------

vtkSmartPointer<vtkPolyData> xq_Seg3DUtils::ComputeNormals(
    vtkSmartPointer<vtkPolyData> input)
{
    if (!input)
        return nullptr;

    auto normals = vtkSmartPointer<vtkPolyDataNormals>::New();
    normals->SetInputData(input);
    normals->ComputePointNormalsOn();
    normals->ComputeCellNormalsOn();
    normals->SplittingOff();
    normals->ConsistencyOn();
    normals->AutoOrientNormalsOn();
    normals->Update();

    return normals->GetOutput();
}
