#pragma once

// ITK-based level-set segmentation for 2D slice images.
//
// Uses the Fast Marching Method (itk::FastMarchingImageFilter) — a static
// level-set / Eikonal solver — with an intensity-derived speed function:
//   speed=1.0 inside the feature intensity band, speed≈0 outside.
// The front propagates quickly within the lumen and naturally stops at
// intensity boundaries.  Stopping distance is scaled from Params::maxIterations
// so that the caller controls the maximum region size.
//
// The contour is extracted as a closed vtkPolyData polygon in slice
// coordinates, matching the xq_SegmentationAlgorithm contract.

#include <xqModuleSegmentationExports.h>
#include "xq_SegmentationAlgorithm.h"

class XQMODULESEGMENTATION_EXPORT xq_ITKLevelSetSegmentation : public xq_SegmentationAlgorithm
{
public:
    [[nodiscard]] std::string_view Name() const override { return "levelset"; }
    [[nodiscard]] Contour Extract(const SliceInput& slice, const Params& params) override;
};
