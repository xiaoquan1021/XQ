#ifndef XQ_ADAPTERS_VTK_IMAGE_ADAPTER_H
#define XQ_ADAPTERS_VTK_IMAGE_ADAPTER_H

#include "core/XQImageVolume.h"
#include "core/XQMemoryImageBufferHandle.h"

#include <memory>
#include <string>

namespace xq {

class VtkImageAdapter {
public:
    enum class LoadStatus {
        Ok,
        FileNotFound,
        ReadFailed,
        InvalidImage
    };

    static LoadStatus loadVti(const std::string& path, XQImageVolume* out);

    // Loads a .vti and decodes its real point scalars into an XQ-owned in-memory
    // buffer (segmentation algorithms need actual voxel values, which loadVti's
    // counts-only handle does not provide). On success *outImage gets the
    // geometry/type and *outBuffer the decoded scalars. VTK stays private to this
    // adapter; the public result is core's XQImageVolume + XQMemoryImageBufferHandle.
    static LoadStatus loadVtiWithBuffer(const std::string& path,
                                        XQImageVolume* outImage,
                                        std::shared_ptr<XQMemoryImageBufferHandle>* outBuffer);
};

} // namespace xq

#endif // XQ_ADAPTERS_VTK_IMAGE_ADAPTER_H
