#ifndef XQ_ADAPTERS_ITK_ITK_VASCULAR_ROI_PRIOR_READER_H
#define XQ_ADAPTERS_ITK_ITK_VASCULAR_ROI_PRIOR_READER_H

#include "core/image/IVascularRoiPriorReader.h"

namespace xq {

// Reads offline binary NIfTI data through ITK and materializes XQ-owned masks
// on the reference CT grid. ITK objects never cross this adapter boundary.
class ItkVascularRoiPriorReader final : public IVascularRoiPriorReader {
public:
    VascularRoiPriorReadResult read(
        const XQImageVolume& reference,
        const std::string& ctInputFingerprint,
        const std::vector<VascularRoiFileInput>& inputs) const override;
};

} // namespace xq

#endif // XQ_ADAPTERS_ITK_ITK_VASCULAR_ROI_PRIOR_READER_H
