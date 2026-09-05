#ifndef XQ_ADAPTERS_VTK_IMAGE_ADAPTER_H
#define XQ_ADAPTERS_VTK_IMAGE_ADAPTER_H

#include "core/XQImageVolume.h"

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
};

} // namespace xq

#endif // XQ_ADAPTERS_VTK_IMAGE_ADAPTER_H
