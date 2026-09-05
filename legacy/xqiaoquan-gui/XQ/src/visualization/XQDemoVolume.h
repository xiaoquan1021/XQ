#ifndef XQ_VISUALIZATION_DEMO_VOLUME_H
#define XQ_VISUALIZATION_DEMO_VOLUME_H
#include "core/XQImageVolume.h"
#include "core/XQMemoryImageBufferHandle.h"
#include <memory>
namespace xq {
// Decoded image volume + its scalar buffer, held together so the MPR view's
// borrowed pointers stay valid for the lifetime of the loaded dataset.
struct XQDemoVolume {
    XQImageVolume image;
    std::shared_ptr<XQMemoryImageBufferHandle> buffer;
};
} // namespace xq
#endif
