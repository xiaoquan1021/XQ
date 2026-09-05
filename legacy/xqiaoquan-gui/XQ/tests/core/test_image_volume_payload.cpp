#include <core/XQImageVolume.h>
#include <core/XQImageVolumePayload.h>
#include <core/XQPayload.h>

#include <cmath>
#include <cstdio>
#include <memory>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

bool close(double a, double b)
{
    return std::abs(a - b) < 1e-12;
}

xq::XQImageVolume make_volume()
{
    xq::ImageGeometry geometry{};
    geometry.dimensions[0] = 4;
    geometry.dimensions[1] = 3;
    geometry.dimensions[2] = 2;
    geometry.spacing[0] = 0.7;
    geometry.spacing[1] = 0.8;
    geometry.spacing[2] = 1.5;
    geometry.origin[0] = 10.0;
    geometry.origin[1] = 20.0;
    geometry.origin[2] = 30.0;
    geometry.direction[0][0] = 0.0;
    geometry.direction[0][1] = -1.0;
    geometry.direction[0][2] = 0.0;
    geometry.direction[1][0] = 1.0;
    geometry.direction[1][1] = 0.0;
    geometry.direction[1][2] = 0.0;
    geometry.direction[2][0] = 0.0;
    geometry.direction[2][1] = 0.0;
    geometry.direction[2][2] = 1.0;
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;

    xq::XQImageVolume volume;
    volume.setGeometry(geometry);
    volume.setScalarType(xq::ScalarType::Int16);
    volume.setComponentCount(1);
    volume.setIntensityRange({-1024.0, 3071.0});
    volume.setModality(xq::ImageModality::CT);
    volume.setDicomIdentity({"study-uid", "series-uid", "frame-uid"});
    volume.setWindowCenter(40.0);
    volume.setWindowWidth(400.0);
    volume.setRescaleSlope(1.0);
    volume.setRescaleIntercept(0.0);
    volume.setBuffer(std::make_shared<xq::ImageBufferHandle>());
    return volume;
}

bool same_metadata(const xq::XQImageVolume& a, const xq::XQImageVolume& b)
{
    if (a.hasGeometry() != b.hasGeometry()) {
        return false;
    }
    const xq::ImageGeometry& ga = a.geometry();
    const xq::ImageGeometry& gb = b.geometry();
    for (int axis = 0; axis < 3; ++axis) {
        if (ga.dimensions[axis] != gb.dimensions[axis]
            || !close(ga.spacing[axis], gb.spacing[axis])
            || !close(ga.origin[axis], gb.origin[axis])) {
            return false;
        }
        for (int component = 0; component < 3; ++component) {
            if (!close(ga.direction[axis][component], gb.direction[axis][component])) {
                return false;
            }
        }
    }
    return ga.coordinateSystem == gb.coordinateSystem
        && a.scalarType() == b.scalarType()
        && a.componentCount() == b.componentCount()
        && close(a.intensityRange().minimum, b.intensityRange().minimum)
        && close(a.intensityRange().maximum, b.intensityRange().maximum)
        && a.modality() == b.modality()
        && a.hasDicomIdentity() == b.hasDicomIdentity()
        && a.dicomIdentity().studyInstanceUid == b.dicomIdentity().studyInstanceUid
        && a.dicomIdentity().seriesInstanceUid == b.dicomIdentity().seriesInstanceUid
        && a.dicomIdentity().frameOfReferenceUid == b.dicomIdentity().frameOfReferenceUid
        && close(a.windowCenter(), b.windowCenter())
        && close(a.windowWidth(), b.windowWidth())
        && close(a.rescaleSlope(), b.rescaleSlope())
        && close(a.rescaleIntercept(), b.rescaleIntercept());
}

} // namespace

int main()
{
    {
        xq::XQImageVolumePayload emptyPayload(xq::XQImageVolume{});
        double point[3] = {0.0, 0.0, 0.0};
        double transformed[3] = {0.0, 0.0, 0.0};
        if (emptyPayload.volume().hasGeometry()
            || emptyPayload.volume().bufferHandle() != nullptr) {
            return fail("default image payload has neither geometry nor buffer", __LINE__);
        }
        if (emptyPayload.volume().voxelToWorld(point, transformed)
            != xq::XQImageVolume::TransformStatus::GeometryNotSet) {
            return fail("default image payload reports missing geometry", __LINE__);
        }
    }

    xq::XQImageVolume source = make_volume();
    const std::shared_ptr<xq::ImageBufferHandle> callerBuffer = source.bufferHandle();
    if (callerBuffer == nullptr || !callerBuffer->is_valid()
        || callerBuffer->voxelCount() != 24) {
        return fail("source fixture owns a counts-only buffer handle", __LINE__);
    }

    xq::XQImageVolumePayload payload(source);
    if (payload.domainType() != xq::XQDomainType::Image) {
        return fail("image payload domain", __LINE__);
    }
    if (payload.volume().bufferHandle() != nullptr) {
        return fail("metadata payload strips buffer handle", __LINE__);
    }
    if (source.bufferHandle() == nullptr || !source.bufferHandle()->is_valid()) {
        return fail("payload construction does not mutate source residency", __LINE__);
    }
    if (source.bufferHandle() != callerBuffer || !callerBuffer->is_valid()
        || callerBuffer->voxelCount() != 24) {
        return fail("payload construction does not clear the caller buffer", __LINE__);
    }
    if (!same_metadata(source, payload.volume())) {
        return fail("metadata payload preserves image metadata", __LINE__);
    }

    source.setWindowCenter(123.0);
    source.setDicomIdentity({"changed-study", "changed-series", "changed-frame"});
    if (!close(payload.volume().windowCenter(), 40.0)
        || payload.volume().dicomIdentity().studyInstanceUid != "study-uid") {
        return fail("payload owns an independent metadata value", __LINE__);
    }

    std::shared_ptr<xq::XQPayload> clonedBase = payload.clone();
    if (clonedBase.get() == &payload || clonedBase->domainType() != xq::XQDomainType::Image) {
        return fail("clone yields distinct typed payload", __LINE__);
    }
    const auto* cloned = dynamic_cast<const xq::XQImageVolumePayload*>(clonedBase.get());
    if (cloned == nullptr || cloned->volume().bufferHandle() != nullptr
        || !same_metadata(payload.volume(), cloned->volume())) {
        return fail("clone preserves metadata-only invariant", __LINE__);
    }

    xq::XQImageVolume emptyVolume;
    xq::XQImageVolumePayload emptyPayload(emptyVolume);
    const std::shared_ptr<xq::XQPayload> emptyCloneBase = emptyPayload.clone();
    const auto* emptyClone =
        dynamic_cast<const xq::XQImageVolumePayload*>(emptyCloneBase.get());
    if (emptyPayload.volume().hasGeometry()
        || emptyPayload.volume().hasDicomIdentity()
        || emptyClone == nullptr
        || emptyClone->volume().hasGeometry()
        || emptyClone->volume().hasDicomIdentity()) {
        return fail("unset geometry and DICOM identity survive payload clone", __LINE__);
    }

    double world[3] = {0.0, 0.0, 0.0};
    const double voxel[3] = {1.0, 2.0, 1.0};
    if (payload.volume().voxelToWorld(voxel, world)
            != xq::XQImageVolume::TransformStatus::Ok
        || !close(world[0], 8.4)
        || !close(world[1], 20.7)
        || !close(world[2], 31.5)) {
        return fail("metadata payload preserves patient-space transform", __LINE__);
    }

    return 0;
}
