#include "core/XQImageVolume.h"

#include <cmath>

namespace xq {
namespace {

const double kSingularEpsilon = 1e-12;

std::size_t voxel_count(const ImageGeometry& geometry)
{
    if (geometry.dimensions[0] <= 0
        || geometry.dimensions[1] <= 0
        || geometry.dimensions[2] <= 0) {
        return 0;
    }

    return static_cast<std::size_t>(geometry.dimensions[0])
        * static_cast<std::size_t>(geometry.dimensions[1])
        * static_cast<std::size_t>(geometry.dimensions[2]);
}

bool invert_3x3(const double m[3][3], double out[3][3])
{
    const double det =
        m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
        - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
        + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

    if (std::abs(det) < kSingularEpsilon) {
        return false;
    }

    const double inv_det = 1.0 / det;

    out[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * inv_det;
    out[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * inv_det;
    out[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inv_det;

    out[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * inv_det;
    out[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inv_det;
    out[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * inv_det;

    out[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * inv_det;
    out[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * inv_det;
    out[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inv_det;

    return true;
}

} // namespace

ImageBufferHandle::ImageBufferHandle()
    : voxelCount_(0),
      valid_(false)
{
}

bool ImageBufferHandle::is_valid() const
{
    return valid_;
}

std::size_t ImageBufferHandle::voxelCount() const
{
    return voxelCount_;
}

void ImageBufferHandle::setVoxelCount(std::size_t voxelCount)
{
    voxelCount_ = voxelCount;
    valid_ = voxelCount_ > 0;
}

XQImageVolume::XQImageVolume()
    : geometry_(),
      geometrySet_(false),
      scalarType_(ScalarType::Unknown),
      componentCount_(0),
      intensityRange_(),
      buffer_(),
      modality_(ImageModality::Unknown),
      hasDicom_(false),
      dicom_(),
      windowCenter_(0.0),
      windowWidth_(0.0),
      rescaleSlope_(1.0),
      rescaleIntercept_(0.0)
{
}

void XQImageVolume::setGeometry(const ImageGeometry& g)
{
    geometry_ = g;
    geometrySet_ = true;
    if (buffer_) {
        buffer_->setVoxelCount(voxel_count(geometry_));
    }
}

void XQImageVolume::setScalarType(ScalarType t)
{
    scalarType_ = t;
}

void XQImageVolume::setComponentCount(int c)
{
    componentCount_ = c;
}

void XQImageVolume::setIntensityRange(const IntensityRange& r)
{
    intensityRange_ = r;
}

void XQImageVolume::setBuffer(std::shared_ptr<ImageBufferHandle> buf)
{
    buffer_ = buf;
    if (buffer_) {
        buffer_->setVoxelCount(geometrySet_ ? voxel_count(geometry_) : 0);
    }
}

void XQImageVolume::setModality(ImageModality m)
{
    modality_ = m;
}

void XQImageVolume::setDicomIdentity(const DicomSeriesIdentity& id)
{
    dicom_ = id;
    hasDicom_ = true;
}

void XQImageVolume::setWindowCenter(double v)
{
    windowCenter_ = v;
}

void XQImageVolume::setWindowWidth(double v)
{
    windowWidth_ = v;
}

void XQImageVolume::setRescaleSlope(double v)
{
    rescaleSlope_ = v;
}

void XQImageVolume::setRescaleIntercept(double v)
{
    rescaleIntercept_ = v;
}

const ImageGeometry& XQImageVolume::geometry() const
{
    return geometry_;
}

bool XQImageVolume::hasGeometry() const
{
    return geometrySet_;
}

ScalarType XQImageVolume::scalarType() const
{
    return scalarType_;
}

int XQImageVolume::componentCount() const
{
    return componentCount_;
}

const IntensityRange& XQImageVolume::intensityRange() const
{
    return intensityRange_;
}

std::shared_ptr<ImageBufferHandle> XQImageVolume::bufferHandle() const
{
    return buffer_;
}

ImageModality XQImageVolume::modality() const
{
    return modality_;
}

bool XQImageVolume::hasDicomIdentity() const
{
    return hasDicom_;
}

const DicomSeriesIdentity& XQImageVolume::dicomIdentity() const
{
    return dicom_;
}

double XQImageVolume::windowCenter() const
{
    return windowCenter_;
}

double XQImageVolume::windowWidth() const
{
    return windowWidth_;
}

double XQImageVolume::rescaleSlope() const
{
    return rescaleSlope_;
}

double XQImageVolume::rescaleIntercept() const
{
    return rescaleIntercept_;
}

XQImageVolume::TransformStatus XQImageVolume::voxelToWorld(const double voxel[3], double world_out[3]) const
{
    if (!geometrySet_) {
        return TransformStatus::GeometryNotSet;
    }

    const double scaled[3] = {
        geometry_.spacing[0] * voxel[0],
        geometry_.spacing[1] * voxel[1],
        geometry_.spacing[2] * voxel[2],
    };

    for (int i = 0; i < 3; ++i) {
        world_out[i] = geometry_.origin[i];
        for (int j = 0; j < 3; ++j) {
            world_out[i] += geometry_.direction[i][j] * scaled[j];
        }
    }

    return TransformStatus::Ok;
}

XQImageVolume::TransformStatus XQImageVolume::worldToVoxel(const double world[3], double voxel_out[3]) const
{
    if (!geometrySet_) {
        return TransformStatus::GeometryNotSet;
    }

    for (int i = 0; i < 3; ++i) {
        if (std::abs(geometry_.spacing[i]) < kSingularEpsilon) {
            return TransformStatus::GeometryNotSet;
        }
    }

    double inverse_direction[3][3] = {};
    if (!invert_3x3(geometry_.direction, inverse_direction)) {
        return TransformStatus::GeometryNotSet;
    }

    const double delta[3] = {
        world[0] - geometry_.origin[0],
        world[1] - geometry_.origin[1],
        world[2] - geometry_.origin[2],
    };

    double scaled[3] = {0.0, 0.0, 0.0};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            scaled[i] += inverse_direction[i][j] * delta[j];
        }
        voxel_out[i] = scaled[i] / geometry_.spacing[i];
    }

    return TransformStatus::Ok;
}

} // namespace xq
