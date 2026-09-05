#ifndef XQ_ADAPTERS_ITK_ITK_AUTOMATIC_VESSEL_SEGMENTER_H
#define XQ_ADAPTERS_ITK_ITK_AUTOMATIC_VESSEL_SEGMENTER_H

#include "core/image/IAutomaticVesselSegmenter.h"

namespace xq {

// Gold-free ROI-guided production adapter. ITK 5.4 owns the signed-distance,
// edge-potential, geodesic-active-contour, threshold and component kernels.
// XQ owns validation, lineage, orchestration and the materialized result.
class ItkAutomaticVesselSegmenter final : public IAutomaticVesselSegmenter {
public:
    static const char* algorithmId();
    static const char* algorithmVersion();

    AutomaticVesselSegmentationResult run(
        const XQImageVolume& image,
        const IVoxelSource& source,
        const XQVesselnessVolume& vesselness,
        const XQVascularRoiPriorV1& roiPrior,
        const AutomaticVesselSegmentationProfileV2& profile,
        const AutomaticVesselSegmentationCancellation* cancellation = nullptr) const override;
};

} // namespace xq

#endif // XQ_ADAPTERS_ITK_ITK_AUTOMATIC_VESSEL_SEGMENTER_H
