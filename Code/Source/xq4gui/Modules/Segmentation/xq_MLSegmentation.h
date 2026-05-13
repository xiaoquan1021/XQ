#pragma once

// Machine-learning-assisted segmentation placeholder.
//
// Phase 1 (current): no ML backend is linked.  The class can be created
// and queried, but Extract() always returns !ok with a diagnostic stating
// that ML is disabled.  This contract lets callers probe for ML capability
// without crashing.
//
// When a real ML backend (e.g. ONNX Runtime, Torch, or TensorFlow) becomes
// available in a future phase, this class can delegate to it without
// changing the xq_SegmentationAlgorithm interface.

#include <xqModuleSegmentationExports.h>
#include "xq_SegmentationAlgorithm.h"

class XQMODULESEGMENTATION_EXPORT xq_MLSegmentation : public xq_SegmentationAlgorithm
{
public:
    [[nodiscard]] std::string_view Name() const override { return "ml"; }
    [[nodiscard]] Contour Extract(const SliceInput& slice, const Params& params) override;

    // Always false in Phase 1.  Returns true only when a real ML backend is
    // loaded and ready.
    [[nodiscard]] static bool IsBackendAvailable();

    // Human-readable reason why the ML backend is not available.
    [[nodiscard]] static std::string GetBackendDiagnostic();
};
