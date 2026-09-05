#ifndef XQ_ADAPTERS_ITK_ITK_VASCULAR_SEGMENTATION_EVALUATOR_H
#define XQ_ADAPTERS_ITK_ITK_VASCULAR_SEGMENTATION_EVALUATOR_H

#include "core/image/IVascularSegmentationEvaluator.h"

namespace xq {

class ItkVascularSegmentationEvaluator final
    : public IVascularSegmentationEvaluator {
public:
    static const char* evaluatorId();
    static const char* evaluatorVersion();
    static const char* thinningBackendId();
    static const char* thinningBackendVersion();

    VascularSegmentationEvaluationResult evaluate(
        const XQSegmentationMask& prediction,
        const XQSegmentationMask& reference) const override;
};

} // namespace xq

#endif // XQ_ADAPTERS_ITK_ITK_VASCULAR_SEGMENTATION_EVALUATOR_H
