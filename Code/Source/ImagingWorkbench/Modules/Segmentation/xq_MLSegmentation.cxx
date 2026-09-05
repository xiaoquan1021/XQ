#include "xq_MLSegmentation.h"

bool xq_MLSegmentation::IsBackendAvailable()
{
    return false;
}

std::string xq_MLSegmentation::GetBackendDiagnostic()
{
    return "ML segmentation backend not available.  "
           "No model runtime (ONNX / Torch / TensorFlow) is linked.";
}

xq_SegmentationAlgorithm::Contour
xq_MLSegmentation::Extract(const SliceInput& slice, const Params& params)
{
    (void)params;
    Contour out;
    if (!slice.slice)
    {
        out.diagnostic = "ML segmentation: slice is null.";
        return out;
    }
    out.diagnostic = "ML segmentation disabled: " + GetBackendDiagnostic();
    return out;
}
