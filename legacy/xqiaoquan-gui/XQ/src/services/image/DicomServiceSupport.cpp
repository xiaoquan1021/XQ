#include "services/image/DicomServiceSupport.h"

#include "core/XQMemoryImageBufferHandle.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace xq {
namespace {

constexpr char kDicomFingerprintPrefix[] = "dicom-series-v1:sha256:";

bool isHexadecimal(char value)
{
    return (value >= '0' && value <= '9')
        || (value >= 'a' && value <= 'f')
        || (value >= 'A' && value <= 'F');
}

bool closeEnough(double left, double right)
{
    const double scale = (std::max)(
        1.0, (std::max)(std::abs(left), std::abs(right)));
    return std::isfinite(left) && std::isfinite(right)
        && std::abs(left - right) <= 1e-9 * scale;
}

bool checkedMultiply(std::size_t left,
                     std::size_t right,
                     std::size_t* product)
{
    if (product == nullptr
        || (right != 0
            && left > (std::numeric_limits<std::size_t>::max)() / right)) {
        return false;
    }
    *product = left * right;
    return true;
}

} // namespace

bool isValidDicomSeriesFingerprint(const std::string& fingerprint)
{
    const std::size_t prefixLength = sizeof(kDicomFingerprintPrefix) - 1;
    if (fingerprint.size() != prefixLength + 64
        || fingerprint.compare(0, prefixLength, kDicomFingerprintPrefix) != 0) {
        return false;
    }
    for (std::size_t index = prefixLength; index < fingerprint.size(); ++index) {
        if (!isHexadecimal(fingerprint[index])) {
            return false;
        }
    }
    return true;
}

bool sameDicomSeriesIdentity(const DicomSeriesIdentity& left,
                             const DicomSeriesIdentity& right)
{
    return left.studyInstanceUid == right.studyInstanceUid
        && left.seriesInstanceUid == right.seriesInstanceUid
        && left.frameOfReferenceUid == right.frameOfReferenceUid;
}

bool sameImageGeometry(const ImageGeometry& left,
                       const ImageGeometry& right)
{
    if (left.coordinateSystem != right.coordinateSystem) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (left.dimensions[axis] != right.dimensions[axis]
            || !closeEnough(left.spacing[axis], right.spacing[axis])
            || !closeEnough(left.origin[axis], right.origin[axis])) {
            return false;
        }
        for (int component = 0; component < 3; ++component) {
            if (!closeEnough(left.direction[axis][component],
                             right.direction[axis][component])) {
                return false;
            }
        }
    }
    return true;
}

bool isValidImageGeometry(const ImageGeometry& geometry)
{
    if (geometry.coordinateSystem != ImageCoordinateSystem::LPS) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (geometry.dimensions[axis] <= 0
            || !std::isfinite(geometry.spacing[axis])
            || !(geometry.spacing[axis] > 0.0)
            || !std::isfinite(geometry.origin[axis])) {
            return false;
        }
        for (int component = 0; component < 3; ++component) {
            if (!std::isfinite(geometry.direction[axis][component])) {
                return false;
            }
        }
    }
    const double determinant =
        geometry.direction[0][0]
            * (geometry.direction[1][1] * geometry.direction[2][2]
               - geometry.direction[1][2] * geometry.direction[2][1])
        - geometry.direction[0][1]
            * (geometry.direction[1][0] * geometry.direction[2][2]
               - geometry.direction[1][2] * geometry.direction[2][0])
        + geometry.direction[0][2]
            * (geometry.direction[1][0] * geometry.direction[2][1]
               - geometry.direction[1][1] * geometry.direction[2][0]);
    return std::isfinite(determinant) && std::abs(determinant) > 1e-8;
}

bool isValidCanonicalDicomImageMetadata(const XQImageVolume& image)
{
    if (!image.hasGeometry() || !image.hasDicomIdentity()
        || !isValidImageGeometry(image.geometry())
        || image.scalarType() == ScalarType::Unknown
        || image.componentCount() != 1
        || (image.modality() != ImageModality::CT
            && image.modality() != ImageModality::MR)) {
        return false;
    }

    const DicomSeriesIdentity& identity = image.dicomIdentity();
    const IntensityRange& range = image.intensityRange();
    return !identity.studyInstanceUid.empty()
        && !identity.seriesInstanceUid.empty()
        && !identity.frameOfReferenceUid.empty()
        && std::isfinite(range.minimum)
        && std::isfinite(range.maximum)
        && range.minimum <= range.maximum
        && std::isfinite(image.windowCenter())
        && std::isfinite(image.windowWidth())
        && std::isfinite(image.rescaleSlope())
        && std::isfinite(image.rescaleIntercept())
        && image.rescaleSlope() == 1.0
        && image.rescaleIntercept() == 0.0;
}

bool isValidVoxelBufferForImage(
    const XQImageVolume& image,
    const XQMemoryImageBufferHandle& buffer)
{
    if (!isValidCanonicalDicomImageMetadata(image)
        || !buffer.is_valid()
        || buffer.scalarType() != image.scalarType()
        || buffer.componentCount() != image.componentCount()) {
        return false;
    }

    std::size_t voxelCount = 1;
    const ImageGeometry& geometry = image.geometry();
    const int bufferDimensions[3] = {
        buffer.dimensionX(), buffer.dimensionY(), buffer.dimensionZ()
    };
    for (int axis = 0; axis < 3; ++axis) {
        if (bufferDimensions[axis] != geometry.dimensions[axis]
            || !checkedMultiply(
                voxelCount,
                static_cast<std::size_t>(geometry.dimensions[axis]),
                &voxelCount)) {
            return false;
        }
    }

    std::size_t scalarCount = 0;
    std::size_t expectedBytes = 0;
    const std::size_t scalarBytes =
        XQMemoryImageBufferHandle::scalarSize(image.scalarType());
    return scalarBytes != 0
        && checkedMultiply(
            voxelCount,
            static_cast<std::size_t>(image.componentCount()),
            &scalarCount)
        && checkedMultiply(scalarCount, scalarBytes, &expectedBytes)
        && buffer.voxelCount() == voxelCount
        && buffer.bytes().size() == expectedBytes;
}

void appendSafeDicomReaderDiagnostics(
    std::vector<Diagnostic>* destination,
    const std::vector<Diagnostic>& readerDiagnostics,
    const std::string& codePrefix)
{
    if (destination == nullptr) {
        return;
    }
    bool hasInfo = false;
    bool hasWarning = false;
    bool hasError = false;
    for (std::vector<Diagnostic>::const_iterator it = readerDiagnostics.begin();
         it != readerDiagnostics.end(); ++it) {
        switch (it->severity()) {
        case DiagnosticSeverity::Info:
            if (!hasInfo) {
                destination->emplace_back(
                    DiagnosticSeverity::Info,
                    codePrefix + ".reader_info",
                    "The DICOM reader reported technical information.");
                hasInfo = true;
            }
            break;
        case DiagnosticSeverity::Warning:
            if (!hasWarning) {
                destination->emplace_back(
                    DiagnosticSeverity::Warning,
                    codePrefix + ".reader_warning",
                    "The DICOM reader reported a technical warning.");
                hasWarning = true;
            }
            break;
        case DiagnosticSeverity::Error:
            if (!hasError) {
                destination->emplace_back(
                    DiagnosticSeverity::Error,
                    codePrefix + ".reader_error",
                    "The DICOM reader reported a technical error.");
                hasError = true;
            }
            break;
        }
    }
}

} // namespace xq
