#include <core/XQImageVolume.h>
#include <visualization/XQImageViewer.h>

#include <cstdio>
#include <memory>

namespace {

int fail(const char* message)
{
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

xq::ImageGeometry make_geometry()
{
    xq::ImageGeometry geometry = {};
    geometry.dimensions[0] = 48;
    geometry.dimensions[1] = 36;
    geometry.dimensions[2] = 12;
    geometry.spacing[0] = 0.7;
    geometry.spacing[1] = 0.7;
    geometry.spacing[2] = 1.2;
    geometry.origin[0] = -12.0;
    geometry.origin[1] = -8.0;
    geometry.origin[2] = 3.5;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            geometry.direction[row][column] = row == column ? 1.0 : 0.0;
        }
    }
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    return geometry;
}

xq::XQImageVolume make_image()
{
    xq::XQImageVolume image;
    image.setGeometry(make_geometry());
    image.setScalarType(xq::ScalarType::Float32);
    image.setComponentCount(1);
    image.setIntensityRange({0.0, 4095.0});
    image.setBuffer(std::make_shared<xq::ImageBufferHandle>());
    image.setWindowCenter(1024.0);
    image.setWindowWidth(2048.0);
    return image;
}

} // namespace

int main()
{
    const int width = 256;
    const int height = 192;

    xq::XQImageVolume image = make_image();
    xq::XQImageViewer viewer;
    const xq::ImageRenderResult result = viewer.renderOffscreen(image, width, height);

    if (!result.ok) {
        return fail("renderOffscreen returns ok for a synthetic XQImageVolume");
    }
    if (result.width != width) {
        return fail("renderOffscreen reports the requested width");
    }
    if (result.height != height) {
        return fail("renderOffscreen reports the requested height");
    }

    return 0;
}
