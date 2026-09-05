#ifndef XQ_CORE_IMAGE_VOLUME_H
#define XQ_CORE_IMAGE_VOLUME_H

#include <cstddef>
#include <memory>
#include <string>

namespace xq {

enum class ScalarType {
    Unknown,
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Float32,
    Float64
};

enum class ImageModality {
    Unknown,
    CT,
    MR,
    Other
};

enum class ImageCoordinateSystem {
    LPS,
    RAS
};

struct ImageGeometry {
    int dimensions[3];
    double spacing[3];
    double origin[3];
    double direction[3][3];
    ImageCoordinateSystem coordinateSystem;
};

struct IntensityRange {
    double minimum;
    double maximum;
};

class XQImageVolume;

class ImageBufferHandle {
public:
    ImageBufferHandle();

    bool is_valid() const;
    std::size_t voxelCount() const;

private:
    friend class XQImageVolume;

    void setVoxelCount(std::size_t voxelCount);

    std::size_t voxelCount_;
    bool valid_;
};

struct DicomSeriesIdentity {
    std::string studyInstanceUid;
    std::string seriesInstanceUid;
    std::string frameOfReferenceUid;
};

class XQImageVolume {
public:
    XQImageVolume();

    void setGeometry(const ImageGeometry& g);
    void setScalarType(ScalarType t);
    void setComponentCount(int c);
    void setIntensityRange(const IntensityRange& r);
    void setBuffer(std::shared_ptr<ImageBufferHandle> buf);
    void setModality(ImageModality m);
    void setDicomIdentity(const DicomSeriesIdentity& id);
    void setWindowCenter(double v);
    void setWindowWidth(double v);
    void setRescaleSlope(double v);
    void setRescaleIntercept(double v);

    const ImageGeometry& geometry() const;
    bool hasGeometry() const;
    ScalarType scalarType() const;
    int componentCount() const;
    const IntensityRange& intensityRange() const;
    std::shared_ptr<ImageBufferHandle> bufferHandle() const;
    ImageModality modality() const;
    bool hasDicomIdentity() const;
    const DicomSeriesIdentity& dicomIdentity() const;
    double windowCenter() const;
    double windowWidth() const;
    double rescaleSlope() const;
    double rescaleIntercept() const;

    enum class TransformStatus {
        Ok,
        GeometryNotSet
    };

    TransformStatus voxelToWorld(const double voxel[3], double world_out[3]) const;
    TransformStatus worldToVoxel(const double world[3], double voxel_out[3]) const;

private:
    ImageGeometry geometry_;
    bool geometrySet_;
    ScalarType scalarType_;
    int componentCount_;
    IntensityRange intensityRange_;
    std::shared_ptr<ImageBufferHandle> buffer_;
    ImageModality modality_;
    bool hasDicom_;
    DicomSeriesIdentity dicom_;
    double windowCenter_;
    double windowWidth_;
    double rescaleSlope_;
    double rescaleIntercept_;
};

} // namespace xq

#endif // XQ_CORE_IMAGE_VOLUME_H
