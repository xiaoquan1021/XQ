#ifndef XQ_VISUALIZATION_IMAGE_VIEWER_H
#define XQ_VISUALIZATION_IMAGE_VIEWER_H

#include <memory>
#include <vector>

namespace xq {

class XQImageVolume;

struct ImageRenderResult {
    bool ok;
    int width;
    int height;
};

class XQImageViewer {
public:
    XQImageViewer();
    ~XQImageViewer();

    XQImageViewer(const XQImageViewer&) = delete;
    XQImageViewer& operator=(const XQImageViewer&) = delete;

    ImageRenderResult renderOffscreen(const XQImageVolume& image, int width, int height);
    ImageRenderResult renderToRgba(const XQImageVolume& image,
                                   int width,
                                   int height,
                                   std::vector<unsigned char>* outRgba);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xq

#endif // XQ_VISUALIZATION_IMAGE_VIEWER_H
