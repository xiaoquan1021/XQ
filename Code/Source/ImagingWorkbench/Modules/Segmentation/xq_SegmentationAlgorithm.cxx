#include "xq_SegmentationAlgorithm.h"
#include "xq_ITKLevelSetSegmentation.h"
#include "xq_MLSegmentation.h"

#include <vtkCellArray.h>
#include <vtkContourFilter.h>
#include <vtkImageData.h>
#include <vtkMath.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkStripper.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

// Extract the longest closed polyline from a vtkPolyData of lines and turn
// it into a single closed polygon sampled to `targetN` points.
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
        if (npts < 4) // not a closed ring
            continue;
        if (npts > bestLen)
        {
            bestLen = npts;
            bestLoop.assign(pts, pts + npts);
        }
    }

    if (bestLoop.empty())
        return nullptr;

    // Gather physical coordinates of the best loop, drop duplicate seam.
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

    if (rawPts.size() < 3)
        return nullptr;

    // Resample to targetN points by arc length.
    const int nOut = std::max(8, targetN);
    std::vector<double> arc(rawPts.size() + 1, 0.0);
    for (size_t i = 0; i < rawPts.size(); ++i)
    {
        const auto& a = rawPts[i];
        const auto& b = rawPts[(i + 1) % rawPts.size()];
        arc[i + 1] = arc[i] + std::sqrt(
            (b[0] - a[0]) * (b[0] - a[0]) +
            (b[1] - a[1]) * (b[1] - a[1]) +
            (b[2] - a[2]) * (b[2] - a[2]));
    }
    const double totalLen = arc.back();
    if (totalLen < 1e-12)
        return nullptr;

    auto outPoints = vtkSmartPointer<vtkPoints>::New();
    outPoints->SetNumberOfPoints(nOut);
    for (int i = 0; i < nOut; ++i)
    {
        const double target = totalLen * static_cast<double>(i) / nOut;
        auto it = std::upper_bound(arc.begin(), arc.end(), target);
        size_t idx = static_cast<size_t>(std::max<ptrdiff_t>(0, it - arc.begin() - 1));
        if (idx >= rawPts.size())
            idx = rawPts.size() - 1;
        const double segLen = arc[idx + 1] - arc[idx];
        const double t = segLen > 1e-12 ? (target - arc[idx]) / segLen : 0.0;
        const auto& a = rawPts[idx];
        const auto& b = rawPts[(idx + 1) % rawPts.size()];
        outPoints->SetPoint(i,
            a[0] + t * (b[0] - a[0]),
            a[1] + t * (b[1] - a[1]),
            a[2] + t * (b[2] - a[2]));
    }

    auto polys = vtkSmartPointer<vtkCellArray>::New();
    polys->InsertNextCell(nOut);
    for (int i = 0; i < nOut; ++i)
        polys->InsertCellPoint(i);

    auto out = vtkSmartPointer<vtkPolyData>::New();
    out->SetPoints(outPoints);
    out->SetPolys(polys);
    return out;
}

} // namespace

xq_SegmentationAlgorithm::Contour
xq_ThresholdSegmentation::Extract(const SliceInput& slice, const Params& params)
{
    Contour out;
    if (!slice.slice)
    {
        out.diagnostic = "Threshold: slice is null.";
        return out;
    }

    // vtkContourFilter on a 2D image yields isocontour lines.
    auto contour = vtkSmartPointer<vtkContourFilter>::New();
    contour->SetInputData(slice.slice);
    contour->SetValue(0, params.threshold);
    contour->Update();

    // Pick the connected loop closest to the seed if multiple exist.
    auto connectivity = vtkSmartPointer<vtkPolyDataConnectivityFilter>::New();
    connectivity->SetInputConnection(contour->GetOutputPort());
    connectivity->SetExtractionModeToClosestPointRegion();
    connectivity->SetClosestPoint(slice.seed[0], slice.seed[1], 0.0);
    connectivity->Update();

    auto loops = connectivity->GetOutput();
    auto polygon = LongestLoopToPolygon(loops, params.outputPointCount);
    if (!polygon)
    {
        out.diagnostic = "Threshold: no closed contour found at threshold "
                       + std::to_string(params.threshold) + ".";
        return out;
    }

    out.ok = true;
    out.polygon = polygon;
    return out;
}

xq_SegmentationAlgorithm::Contour
xq_LevelSetSegmentation::Extract(const SliceInput& slice, const Params& params)
{
    (void)slice;
    (void)params;
    Contour out;
    out.diagnostic =
        "Legacy LevelSet segmentation path is disabled because it does not "
        "run a real level-set algorithm. Use the registered ITK level-set "
        "implementation through CreateSegmentationAlgorithm(\"levelset\").";
    return out;
}

std::unique_ptr<xq_SegmentationAlgorithm>
CreateSegmentationAlgorithm(std::string_view name)
{
    if (name == "levelset")
        return std::make_unique<xq_ITKLevelSetSegmentation>();
    if (name == "ml")
        return std::make_unique<xq_MLSegmentation>();
    return std::make_unique<xq_ThresholdSegmentation>();
}
