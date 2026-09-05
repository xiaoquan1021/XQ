#ifndef XQ_ADAPTERS_ITK_ITK_VASCULAR_PREPROCESSOR_H
#define XQ_ADAPTERS_ITK_ITK_VASCULAR_PREPROCESSOR_H

#include "core/image/IVascularPreprocessor.h"

namespace xq {

// Production 3D vascular preprocessing adapter. External image/filter types are
// confined to the implementation file; callers receive only XQ-owned values.
class ItkVascularPreprocessor final : public IVascularPreprocessor {
public:
    static const char* algorithmId();
    static const char* algorithmVersion();

    VascularPreprocessResult run(
        const XQImageVolume& image,
        const IVoxelSource& source,
        const VascularPreprocessProfileV1& profile,
        const VascularPreprocessCancellation* cancellation = nullptr) const override;
};

} // namespace xq

#endif // XQ_ADAPTERS_ITK_ITK_VASCULAR_PREPROCESSOR_H
