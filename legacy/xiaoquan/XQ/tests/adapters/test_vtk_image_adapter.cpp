#include <adapters/vtk/VtkImageAdapter.h>

#include <cmath>
#include <cstddef>
#include <cstdio>

#ifndef XQ_TEST_VTI_PATH
#error "XQ_TEST_VTI_PATH must be defined"
#endif

namespace {

int fail(const char* check)
{
    std::printf("check failed: %s\n", check);
    return 1;
}

bool close(double a, double b)
{
    return std::abs(a - b) < 1e-6;
}

bool round_trip_voxel(const xq::XQImageVolume& volume, const double voxel[3])
{
    double world[3] = {0.0, 0.0, 0.0};
    double actual[3] = {0.0, 0.0, 0.0};

    if (volume.voxelToWorld(voxel, world) != xq::XQImageVolume::TransformStatus::Ok) {
        return false;
    }
    if (volume.worldToVoxel(world, actual) != xq::XQImageVolume::TransformStatus::Ok) {
        return false;
    }

    for (int i = 0; i < 3; ++i) {
        if (!close(actual[i], voxel[i])) {
            return false;
        }
    }

    return true;
}

} // namespace

int main()
{
    xq::XQImageVolume volume;
    if (xq::VtkImageAdapter::loadVti(XQ_TEST_VTI_PATH, &volume)
        != xq::VtkImageAdapter::LoadStatus::Ok) {
        return fail("loadVti loads the real .vti file");
    }

    const xq::ImageGeometry& geometry = volume.geometry();
    const xq::IntensityRange& range = volume.intensityRange();
    std::printf("dims=%d,%d,%d\n",
        geometry.dimensions[0],
        geometry.dimensions[1],
        geometry.dimensions[2]);
    std::printf("spacing=%.17g,%.17g,%.17g\n",
        geometry.spacing[0],
        geometry.spacing[1],
        geometry.spacing[2]);
    std::printf("origin=%.17g,%.17g,%.17g\n",
        geometry.origin[0],
        geometry.origin[1],
        geometry.origin[2]);
    std::printf("scalarRange=%.17g,%.17g\n", range.minimum, range.maximum);

    for (int i = 0; i < 3; ++i) {
        if (geometry.dimensions[i] <= 0) {
            return fail("all dimensions are positive");
        }
        if (geometry.spacing[i] <= 0.0) {
            return fail("all spacing values are positive");
        }
    }

    const std::size_t expected_voxels =
        static_cast<std::size_t>(geometry.dimensions[0])
        * static_cast<std::size_t>(geometry.dimensions[1])
        * static_cast<std::size_t>(geometry.dimensions[2]);
    if (!volume.bufferHandle() || !volume.bufferHandle()->is_valid()) {
        return fail("buffer handle is valid");
    }
    if (volume.bufferHandle()->voxelCount() != expected_voxels) {
        return fail("buffer voxel count matches dimensions product");
    }

    if (volume.scalarType() == xq::ScalarType::Unknown) {
        return fail("scalar type is known");
    }
    if (volume.componentCount() < 1) {
        return fail("component count is at least one");
    }
    if (range.minimum > range.maximum) {
        return fail("intensity range is ordered");
    }

    const double zero_voxel[3] = {0.0, 0.0, 0.0};
    if (!round_trip_voxel(volume, zero_voxel)) {
        return fail("voxel {0,0,0} round trips through world coordinates");
    }

    const double sample_voxel[3] = {10.0, 20.0, 5.0};
    if (!round_trip_voxel(volume, sample_voxel)) {
        return fail("voxel {10,20,5} round trips through world coordinates");
    }

    xq::XQImageVolume missing_volume;
    if (xq::VtkImageAdapter::loadVti("C:/no/such/file.vti", &missing_volume)
        != xq::VtkImageAdapter::LoadStatus::FileNotFound) {
        return fail("missing .vti returns FileNotFound");
    }

    return 0;
}
