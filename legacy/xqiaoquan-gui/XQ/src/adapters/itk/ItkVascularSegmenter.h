#ifndef XQ_ADAPTERS_ITK_ITK_VASCULAR_SEGMENTER_H
#define XQ_ADAPTERS_ITK_ITK_VASCULAR_SEGMENTER_H

#include "core/segmentation/ILevelSetSegmenter.h"

namespace xq {

// ILevelSetSegmenter implementation backed by the vendored SimVascular ITK
// vascular two-phase level set (third_party/sv_levelset). The section grayscale
// is imported to an itk::Image<float,2> in index/pixel space (UseImageSpacingOff,
// see memory sv-vascular-levelset-propagation-zero-required); a
// GradientMagnitudeRecursiveGaussian feature image + a signed-distance seed
// circle drive PhaseOne (kc) then PhaseTwo (kupp/klow), and the zero level set is
// traced to an ordered (u,v) mm loop.
//
// The public header carries ZERO ITK types: all ITK / sv_levelset headers appear
// only in the .cpp, so xq_core / xq_services never link ITK.
class ItkVascularSegmenter : public ILevelSetSegmenter {
public:
    std::vector<SectionPoint2D> segment(const std::vector<double>& gray,
                                        int width, int height,
                                        double pixelSizeMm,
                                        const SectionPoint2D& seed,
                                        const LevelSetParams& params) override;
};

} // namespace xq

#endif // XQ_ADAPTERS_ITK_ITK_VASCULAR_SEGMENTER_H
