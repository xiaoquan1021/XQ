#include "xq_ITKLevelSetSegmentation.h"

#include <vtkCellArray.h>
#include <vtkContourFilter.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkSmartPointer.h>
#include <vtkStripper.h>

#include <itkBinaryThresholdImageFilter.h>
#include <itkFastMarchingImageFilter.h>
#include <itkImage.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace
{

using ItkFloat2D = itk::Image<float, 2>;

// ---------------------------------------------------------------------------
// VTK (3D, single Z-slice)  <->  ITK 2D
// ---------------------------------------------------------------------------

ItkFloat2D::Pointer VtkSliceToItk2D(vtkImageData* vtkImg)
{
    if (!vtkImg) return nullptr;

    int dims[3];
    vtkImg->GetDimensions(dims);
    if (dims[0] < 1 || dims[1] < 1) return nullptr;

    double spacing[3], origin[3];
    vtkImg->GetSpacing(spacing);
    vtkImg->GetOrigin(origin);

    ItkFloat2D::RegionType region;
    region.SetSize(0, static_cast<unsigned long>(dims[0]));
    region.SetSize(1, static_cast<unsigned long>(dims[1]));

    auto itkImg = ItkFloat2D::New();
    itkImg->SetRegions(region);
    float itkSpacing[2] = {static_cast<float>(spacing[0]), static_cast<float>(spacing[1])};
    float itkOrigin[2]  = {static_cast<float>(origin[0]), static_cast<float>(origin[1])};
    itkImg->SetSpacing(itkSpacing);
    itkImg->SetOrigin(itkOrigin);
    itkImg->Allocate();

    for (int y = 0; y < dims[1]; ++y)
        for (int x = 0; x < dims[0]; ++x)
        {
            ItkFloat2D::IndexType idx;
            idx[0] = static_cast<long>(x);
            idx[1] = static_cast<long>(y);
            itkImg->SetPixel(idx,
                static_cast<float>(vtkImg->GetScalarComponentAsDouble(x, y, 0, 0)));
        }
    return itkImg;
}

vtkSmartPointer<vtkImageData> Itk2DToVtkDeep(ItkFloat2D::Pointer itkImg)
{
    if (!itkImg) return nullptr;

    const auto& region = itkImg->GetLargestPossibleRegion();
    const auto& size = region.GetSize();
    const auto& sp = itkImg->GetSpacing();
    const auto& org = itkImg->GetOrigin();
    double spacing2D[3] = {sp[0], sp[1], 1.0};
    double origin2D[3] = {org[0], org[1], 0.0};

    vtkNew<vtkImageData> vtkImg;
    vtkImg->SetDimensions(
        static_cast<int>(size[0]),
        static_cast<int>(size[1]),
        1);
    vtkImg->SetSpacing(spacing2D);
    vtkImg->SetOrigin(origin2D);
    vtkImg->AllocateScalars(VTK_FLOAT, 1);

    float* ptr = static_cast<float*>(vtkImg->GetScalarPointer());
    for (unsigned long y = 0; y < size[1]; ++y)
        for (unsigned long x = 0; x < size[0]; ++x)
        {
            ItkFloat2D::IndexType idx;
            idx[0] = static_cast<long>(x);
            idx[1] = static_cast<long>(y);
            *ptr++ = itkImg->GetPixel(idx);
        }
    return vtkImg;
}

// ---------------------------------------------------------------------------
// Polygon extraction
// ---------------------------------------------------------------------------

vtkSmartPointer<vtkPolyData> LongestLoopToPolygon(
    vtkPolyData* contours, int targetN)
{
    if (!contours || contours->GetNumberOfLines() == 0)
        return nullptr;

    auto stripper = vtkSmartPointer<vtkStripper>::New();
    stripper->SetInputData(contours);
    stripper->JoinContiguousSegmentsOn();
    stripper->Update();

    auto stripped = stripper->GetOutput();
    auto* lines = stripped->GetLines();
    lines->InitTraversal();

    vtkIdType npts = 0;
    const vtkIdType* pts = nullptr;
    vtkIdType bestLen = 0;
    std::vector<vtkIdType> bestLoop;

    while (lines->GetNextCell(npts, pts))
    {
        if (npts < 4) continue;
        if (npts > bestLen)
        {
            bestLen = npts;
            bestLoop.assign(pts, pts + npts);
        }
    }

    if (bestLoop.empty()) return nullptr;

    std::vector<std::array<double, 3>> rawPts;
    rawPts.reserve(bestLoop.size());
    for (vtkIdType id : bestLoop)
    {
        double p[3];
        stripped->GetPoint(id, p);
        rawPts.push_back({p[0], p[1], p[2]});
    }
    if (rawPts.size() > 2 &&
        std::fabs(rawPts.front()[0] - rawPts.back()[0]) < 1e-9 &&
        std::fabs(rawPts.front()[1] - rawPts.back()[1]) < 1e-9)
    {
        rawPts.pop_back();
    }

    if (rawPts.size() < 3) return nullptr;

    const int nOut = std::max(8, targetN);
    std::vector<double> arc(rawPts.size() + 1, 0.0);
    for (size_t i = 0; i < rawPts.size(); ++i)
    {
        const auto& a = rawPts[i];
        const auto& b = rawPts[(i + 1) % rawPts.size()];
        arc[i + 1] = arc[i] + std::sqrt(
            (b[0]-a[0])*(b[0]-a[0]) + (b[1]-a[1])*(b[1]-a[1]) + (b[2]-a[2])*(b[2]-a[2]));
    }
    const double totalLen = arc.back();
    if (totalLen < 1e-12) return nullptr;

    auto outPoints = vtkSmartPointer<vtkPoints>::New();
    outPoints->SetNumberOfPoints(nOut);
    for (int i = 0; i < nOut; ++i)
    {
        const double target = totalLen * static_cast<double>(i) / nOut;
        auto it = std::upper_bound(arc.begin(), arc.end(), target);
        size_t idx = static_cast<size_t>(std::max<ptrdiff_t>(0, it - arc.begin() - 1));
        if (idx >= rawPts.size()) idx = rawPts.size() - 1;
        const double segLen = arc[idx + 1] - arc[idx];
        const double t = segLen > 1e-12 ? (target - arc[idx]) / segLen : 0.0;
        const auto& a = rawPts[idx];
        const auto& b = rawPts[(idx + 1) % rawPts.size()];
        outPoints->SetPoint(i,
            a[0] + t*(b[0]-a[0]),
            a[1] + t*(b[1]-a[1]),
            a[2] + t*(b[2]-a[2]));
    }

    auto polys = vtkSmartPointer<vtkCellArray>::New();
    polys->InsertNextCell(nOut);
    for (int i = 0; i < nOut; ++i) polys->InsertCellPoint(i);

    auto out = vtkSmartPointer<vtkPolyData>::New();
    out->SetPoints(outPoints);
    out->SetPolys(polys);
    return out;
}

vtkSmartPointer<vtkPolyData> BinaryImageToPolygon(
    vtkImageData* binaryImg, const double seed[2], int targetN)
{
    auto contour = vtkSmartPointer<vtkContourFilter>::New();
    contour->SetInputData(binaryImg);
    contour->SetValue(0, 0.5);
    contour->Update();

    auto conn = vtkSmartPointer<vtkPolyDataConnectivityFilter>::New();
    conn->SetInputConnection(contour->GetOutputPort());
    conn->SetExtractionModeToClosestPointRegion();
    conn->SetClosestPoint(seed[0], seed[1], 0.0);
    conn->Update();

    return LongestLoopToPolygon(conn->GetOutput(), targetN);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// xq_ITKLevelSetSegmentation::Extract
//
// Uses the Fast Marching Method (a static level-set / Eikonal solver) with
// an intensity-derived speed function so the front propagates quickly within
// the feature intensity band and slowly outside, naturally stopping at
// lumen boundaries.
// ---------------------------------------------------------------------------
xq_SegmentationAlgorithm::Contour
xq_ITKLevelSetSegmentation::Extract(const SliceInput& slice, const Params& params)
{
    Contour out;

    if (!slice.slice)
    {
        out.diagnostic = "LevelSet: slice is null.";
        return out;
    }

    int dims[3];
    slice.slice->GetDimensions(dims);
    if (dims[0] < 2 || dims[1] < 2)
    {
        out.diagnostic = "LevelSet: slice image is too small.";
        return out;
    }

    const double sx = slice.seed[0];
    const double sy = slice.seed[1];
    if (sx < 0.0 || sx >= static_cast<double>(dims[0]) ||
        sy < 0.0 || sy >= static_cast<double>(dims[1]))
    {
        std::ostringstream oss;
        oss << "LevelSet: seed (" << sx << "," << sy
            << ") outside image [" << dims[0] << "," << dims[1] << "].";
        out.diagnostic = oss.str();
        return out;
    }

    try
    {
        // 1. VTK 3D -> ITK 2D
        auto itkSlice = VtkSliceToItk2D(slice.slice);
        if (!itkSlice)
        {
            out.diagnostic = "LevelSet: VTK→ITK 2D conversion failed.";
            return out;
        }

        // 2. Derive intensity band from params or from seed intensity.
        double lower = params.lowerBand;
        double upper = params.upperBand;
        if (lower < 0.0 || upper < 0.0)
        {
            ItkFloat2D::IndexType seedIdx;
            seedIdx[0] = static_cast<long>(sx);
            seedIdx[1] = static_cast<long>(sy);
            const double seedVal = static_cast<double>(itkSlice->GetPixel(seedIdx));
            const double band = 100.0;
            lower = seedVal - band;
            upper = seedVal + band;
        }

        // 3. Build a speed image: fast (1.0) inside the intensity band,
        //    very slow (0.001) outside.  Fast Marching front propagates
        //    almost exclusively within the band.
        using BinaryThreshType = itk::BinaryThresholdImageFilter<ItkFloat2D, ItkFloat2D>;
        auto speedFilter = BinaryThreshType::New();
        speedFilter->SetInput(itkSlice);
        speedFilter->SetLowerThreshold(lower);
        speedFilter->SetUpperThreshold(upper);
        speedFilter->SetInsideValue(1.0);
        speedFilter->SetOutsideValue(0.001);
        speedFilter->Update();

        auto speedImage = speedFilter->GetOutput();

        // 4. Fast Marching Method (static level set).
        using FastMarchingType = itk::FastMarchingImageFilter<ItkFloat2D, ItkFloat2D>;
        auto fastMarching = FastMarchingType::New();
        fastMarching->SetInput(speedImage);

        using NodeType = FastMarchingType::NodeType;
        using NodeContainerType = FastMarchingType::NodeContainer;
        auto seeds = NodeContainerType::New();

        NodeType seedNode;
        ItkFloat2D::IndexType seedIdx;
        seedIdx[0] = static_cast<long>(sx);
        seedIdx[1] = static_cast<long>(sy);
        seedNode.SetIndex(seedIdx);
        seedNode.SetValue(0.0);
        seeds->InsertElement(0, seedNode);

        // Scale stopping distance from maxIterations so the caller can
        // control how far the front may expand.
        const int maxIters = std::max(1, params.maxIterations);
        const double stopDist = static_cast<double>(maxIters) * 2.0;
        const double captureDist = stopDist * 0.25;
        // curvatureWeight is accepted for API compatibility but is not
        // meaningful for the Fast Marching Method (no curvature term in
        // the Eikonal equation).

        fastMarching->SetTrialPoints(seeds);
        fastMarching->SetStoppingValue(stopDist);
        fastMarching->Update();

        auto timeMap = fastMarching->GetOutput();

        // 5. Binary threshold on the time map: voxels reached within the
        //    capture distance are inside the lumen.
        using MaskFilterType = itk::BinaryThresholdImageFilter<ItkFloat2D, ItkFloat2D>;
        auto maskFilter = MaskFilterType::New();
        maskFilter->SetInput(timeMap);
        maskFilter->SetLowerThreshold(0.0);
        maskFilter->SetUpperThreshold(captureDist);
        maskFilter->SetInsideValue(1.0);
        maskFilter->SetOutsideValue(0.0);
        maskFilter->Update();

        // 6. ITK 2D -> VTK 3D, then extract contour polygon.
        auto binaryVtk = Itk2DToVtkDeep(maskFilter->GetOutput());
        if (!binaryVtk)
        {
            out.diagnostic = "LevelSet: ITK→VTK conversion failed.";
            return out;
        }

        double physSeed[2] = {sx, sy};
        auto polygon = BinaryImageToPolygon(binaryVtk, physSeed, params.outputPointCount);
        if (!polygon)
        {
            out.diagnostic = "LevelSet: no closed contour extracted.";
            return out;
        }

        out.ok = true;
        out.polygon = polygon;
        return out;
    }
    catch (const itk::ExceptionObject& e)
    {
        out.diagnostic = std::string("LevelSet: ITK exception: ") + e.GetDescription();
        return out;
    }
    catch (const std::exception& e)
    {
        out.diagnostic = std::string("LevelSet: exception: ") + e.what();
        return out;
    }
}
