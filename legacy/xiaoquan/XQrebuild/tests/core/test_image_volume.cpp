#include <core/XQImageVolume.h>

#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

namespace {

int fail(const char* check)
{
    std::cout << "check failed: " << check << std::endl;
    return 1;
}

bool close(double a, double b)
{
    return std::abs(a - b) < 1e-9;
}

bool same_geometry(const xq::ImageGeometry& a, const xq::ImageGeometry& b)
{
    for (int i = 0; i < 3; ++i) {
        if (a.dimensions[i] != b.dimensions[i]) {
            return false;
        }
        if (!close(a.spacing[i], b.spacing[i])) {
            return false;
        }
        if (!close(a.origin[i], b.origin[i])) {
            return false;
        }
        for (int j = 0; j < 3; ++j) {
            if (!close(a.direction[i][j], b.direction[i][j])) {
                return false;
            }
        }
    }
    return a.coordinateSystem == b.coordinateSystem;
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

xq::ImageGeometry identity_geometry()
{
    xq::ImageGeometry geometry = {};
    geometry.dimensions[0] = 64;
    geometry.dimensions[1] = 64;
    geometry.dimensions[2] = 40;
    geometry.spacing[0] = 0.5;
    geometry.spacing[1] = 0.5;
    geometry.spacing[2] = 1.0;
    geometry.origin[0] = 10.0;
    geometry.origin[1] = 20.0;
    geometry.origin[2] = 30.0;
    geometry.direction[0][0] = 1.0;
    geometry.direction[1][1] = 1.0;
    geometry.direction[2][2] = 1.0;
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    return geometry;
}

} // namespace

int main()
{
    const xq::ImageGeometry geometry = identity_geometry();
    xq::XQImageVolume volume;

    {
        const double voxel[3] = {1.0, 2.0, 3.0};
        double world[3] = {0.0, 0.0, 0.0};
        if (volume.voxelToWorld(voxel, world) != xq::XQImageVolume::TransformStatus::GeometryNotSet) {
            return fail("voxelToWorld fails before geometry is set");
        }
        if (volume.worldToVoxel(world, world) != xq::XQImageVolume::TransformStatus::GeometryNotSet) {
            return fail("worldToVoxel fails before geometry is set");
        }
    }

    if (volume.hasDicomIdentity()) {
        return fail("default volume has no DICOM identity");
    }

    volume.setGeometry(geometry);
    if (!same_geometry(volume.geometry(), geometry)) {
        return fail("geometry round-trips through accessors");
    }

    {
        const double voxel_points[][3] = {
            {0.0, 0.0, 0.0},
            {1.0, 2.0, 3.0},
            {10.25, 20.5, 4.75},
            {63.0, 63.0, 39.0},
        };

        for (int i = 0; i < 4; ++i) {
            if (!round_trip_voxel(volume, voxel_points[i])) {
                return fail("identity direction voxel-world round trip");
            }
        }
    }

    {
        xq::ImageGeometry rotated = geometry;
        rotated.direction[0][0] = 0.0;
        rotated.direction[0][1] = -1.0;
        rotated.direction[0][2] = 0.0;
        rotated.direction[1][0] = 1.0;
        rotated.direction[1][1] = 0.0;
        rotated.direction[1][2] = 0.0;
        rotated.direction[2][0] = 0.0;
        rotated.direction[2][1] = 0.0;
        rotated.direction[2][2] = 1.0;

        xq::XQImageVolume rotated_volume;
        rotated_volume.setGeometry(rotated);

        const double voxel_points[][3] = {
            {0.0, 0.0, 0.0},
            {4.0, 5.0, 6.0},
            {12.5, 8.25, 2.0},
        };

        for (int i = 0; i < 3; ++i) {
            if (!round_trip_voxel(rotated_volume, voxel_points[i])) {
                return fail("rotated direction voxel-world round trip");
            }
        }
    }

    {
        xq::ImageGeometry singular = geometry;
        singular.direction[0][0] = 1.0;
        singular.direction[0][1] = 0.0;
        singular.direction[0][2] = 0.0;
        singular.direction[1][0] = 1.0;
        singular.direction[1][1] = 0.0;
        singular.direction[1][2] = 0.0;
        singular.direction[2][0] = 0.0;
        singular.direction[2][1] = 0.0;
        singular.direction[2][2] = 1.0;

        xq::XQImageVolume singular_volume;
        singular_volume.setGeometry(singular);

        const double world[3] = {0.0, 0.0, 0.0};
        double voxel[3] = {0.0, 0.0, 0.0};
        if (singular_volume.worldToVoxel(world, voxel)
            != xq::XQImageVolume::TransformStatus::GeometryNotSet) {
            return fail("singular direction worldToVoxel fails");
        }
    }

    volume.setScalarType(xq::ScalarType::Float32);
    if (volume.scalarType() != xq::ScalarType::Float32) {
        return fail("scalar type round-trips through accessors");
    }

    volume.setComponentCount(3);
    if (volume.componentCount() != 3) {
        return fail("component count round-trips through accessors");
    }

    const xq::IntensityRange range = {-1024.0, 3071.0};
    volume.setIntensityRange(range);
    if (!close(volume.intensityRange().minimum, range.minimum)
        || !close(volume.intensityRange().maximum, range.maximum)) {
        return fail("intensity range round-trips through accessors");
    }

    volume.setModality(xq::ImageModality::CT);
    if (volume.modality() != xq::ImageModality::CT) {
        return fail("modality round-trips through accessors");
    }

    const xq::DicomSeriesIdentity identity = {
        "study-uid",
        "series-uid",
        "frame-uid",
    };
    volume.setDicomIdentity(identity);
    if (!volume.hasDicomIdentity()) {
        return fail("setting DICOM identity toggles presence flag");
    }
    if (volume.dicomIdentity().studyInstanceUid != identity.studyInstanceUid
        || volume.dicomIdentity().seriesInstanceUid != identity.seriesInstanceUid
        || volume.dicomIdentity().frameOfReferenceUid != identity.frameOfReferenceUid) {
        return fail("DICOM identity round-trips through accessors");
    }

    volume.setWindowCenter(40.0);
    volume.setWindowWidth(400.0);
    volume.setRescaleSlope(1.25);
    volume.setRescaleIntercept(-1024.0);
    if (!close(volume.windowCenter(), 40.0)
        || !close(volume.windowWidth(), 400.0)
        || !close(volume.rescaleSlope(), 1.25)
        || !close(volume.rescaleIntercept(), -1024.0)) {
        return fail("window and rescale metadata round-trip through accessors");
    }

    std::shared_ptr<xq::ImageBufferHandle> buffer(new xq::ImageBufferHandle());
    volume.setBuffer(buffer);
    if (!volume.bufferHandle() || !volume.bufferHandle()->is_valid()) {
        return fail("buffer handle is valid after being attached to geometry");
    }
    if (volume.bufferHandle()->voxelCount() != static_cast<std::size_t>(64 * 64 * 40)) {
        return fail("buffer voxel count matches geometry dimensions product");
    }

    return 0;
}
