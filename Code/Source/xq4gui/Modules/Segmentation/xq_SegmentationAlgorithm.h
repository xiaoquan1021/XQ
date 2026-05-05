#pragma once

// XQ Pipeline Stage 2: 2D cross-section segmentation algorithm interface.
//
// Design: strategy pattern. A concrete algorithm receives a 2D grayscale
// slice (vtkImageData, 1 or 2 voxels thick) resampled on the local normal
// plane of a path trace vertex, plus a suggested seed point on that plane.
// It returns an ordered closed polygon (vtkPolyData containing a single
// vtkPolygon) in slice-local (2D) or world (3D) coordinates as declared by
// the algorithm. The pipeline driver handles the reslice and 2D→world
// transform, so algorithms only deal with intensity data.
//
// Adding a new method (e.g. LevelSet, MorphologicalActiveContour) means
// implementing this interface and registering it in the algorithm factory.

#include <xqModuleSegmentationExports.h>

#include <mitkPoint.h>

#include <vtkSmartPointer.h>
class vtkImageData;
class vtkPolyData;

#include <memory>
#include <string>
#include <string_view>

class XQMODULESEGMENTATION_EXPORT xq_SegmentationAlgorithm
{
public:
    struct SliceInput
    {
        vtkImageData* slice = nullptr;         // resampled 2D slice (XY plane)
        mitk::Point2D seed;                    // 2D seed in slice pixel space
        double pixelSpacing[2] = {1.0, 1.0};   // physical size of a slice pixel (mm)
    };

    struct Params
    {
        double threshold    = 0.0;             // used by Threshold
        double lowerBand    = -1.0;            // optional; -1 means unused
        double upperBand    = -1.0;
        int    maxIterations = 100;            // used by LevelSet
        double curvatureWeight = 0.2;          // used by LevelSet
        int    outputPointCount = 64;          // resample the contour to this N
    };

    struct Contour
    {
        bool ok = false;
        std::string diagnostic;
        // Ordered closed polygon in SLICE coordinates (pixel index * pixelSpacing).
        // The pipeline driver lifts this to world coordinates using the slice
        // plane geometry.
        vtkSmartPointer<vtkPolyData> polygon;
    };

    virtual ~xq_SegmentationAlgorithm() = default;
    [[nodiscard]] virtual std::string_view Name() const = 0;
    [[nodiscard]] virtual Contour Extract(const SliceInput& slice, const Params& params) = 0;
};

// ---------------------------------------------------------------------------
// Concrete: intensity thresholding + largest-connected-component boundary.
// Produces a ring polygon by isocontouring at `params.threshold`.
// ---------------------------------------------------------------------------
class XQMODULESEGMENTATION_EXPORT xq_ThresholdSegmentation : public xq_SegmentationAlgorithm
{
public:
    [[nodiscard]] std::string_view Name() const override { return "threshold"; }
    [[nodiscard]] Contour Extract(const SliceInput& slice, const Params& params) override;
};

// ---------------------------------------------------------------------------
// Concrete: level-set active contour (stub — falls back to Threshold until
// ITK FastMarching+ActiveContour is wired).
// ---------------------------------------------------------------------------
class XQMODULESEGMENTATION_EXPORT xq_LevelSetSegmentation : public xq_SegmentationAlgorithm
{
public:
    [[nodiscard]] std::string_view Name() const override { return "levelset"; }
    [[nodiscard]] Contour Extract(const SliceInput& slice, const Params& params) override;
};

// Factory.
XQMODULESEGMENTATION_EXPORT std::unique_ptr<xq_SegmentationAlgorithm>
CreateSegmentationAlgorithm(std::string_view name);
