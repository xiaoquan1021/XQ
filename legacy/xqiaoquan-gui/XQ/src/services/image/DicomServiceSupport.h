#ifndef XQ_SERVICES_IMAGE_DICOM_SERVICE_SUPPORT_H
#define XQ_SERVICES_IMAGE_DICOM_SERVICE_SUPPORT_H

#include "core/Diagnostics.h"
#include "core/XQImageVolume.h"

#include <string>
#include <vector>

namespace xq {

class XQMemoryImageBufferHandle;

bool isValidDicomSeriesFingerprint(const std::string& fingerprint);

bool sameDicomSeriesIdentity(const DicomSeriesIdentity& left,
                             const DicomSeriesIdentity& right);

bool sameImageGeometry(const ImageGeometry& left,
                       const ImageGeometry& right);

bool isValidImageGeometry(const ImageGeometry& geometry);

bool isValidCanonicalDicomImageMetadata(const XQImageVolume& image);

bool isValidVoxelBufferForImage(
    const XQImageVolume& image,
    const XQMemoryImageBufferHandle& buffer);

// Maps reader-owned diagnostic codes and text to a small fixed vocabulary.
// Neither the original code nor message is copied across the service boundary.
void appendSafeDicomReaderDiagnostics(
    std::vector<Diagnostic>* destination,
    const std::vector<Diagnostic>& readerDiagnostics,
    const std::string& codePrefix);

} // namespace xq

#endif // XQ_SERVICES_IMAGE_DICOM_SERVICE_SUPPORT_H
