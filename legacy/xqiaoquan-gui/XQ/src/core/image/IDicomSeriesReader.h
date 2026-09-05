#ifndef XQ_CORE_IMAGE_I_DICOM_SERIES_READER_H
#define XQ_CORE_IMAGE_I_DICOM_SERIES_READER_H

#include "core/Diagnostics.h"
#include "core/XQImageVolume.h"
#include "core/XQMemoryImageBufferHandle.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace xq {

// Stable caller-visible DICOM failure categories. ITK/GDCM exceptions and tag
// details are mapped to these values inside the adapter and never cross this
// XQ-owned boundary.
enum class DicomSeriesStatus {
    Ok,
    InvalidArgument,
    SourceNotFound,
    SourceUnreadable,
    NoSeries,
    SeriesSelectionRequired,
    AmbiguousSeries,
    SeriesNotFound,
    ReadFailed,
    InconsistentSeries,
    UnsupportedGeometry,
    UnsupportedPixelFormat,
    UnsupportedMultiFrame,
    UnsupportedRescale,
    SourceChanged
};

const char* dicomSeriesStatusToken(DicomSeriesStatus status);

// Technical, non-PHI information sufficient to present and select one series.
// Free-text DICOM description tags are deliberately absent from this contract.
struct DicomSeriesDescriptor {
    DicomSeriesIdentity identity;
    ImageModality modality = ImageModality::Unknown;
    int dimensions[3] = {0, 0, 0};
    std::size_t sliceCount = 0;
    std::string safeDisplayName;
    std::string contentFingerprint;
};

// Builds a deterministic label exclusively from modality, dimensions, slice
// count, and an abbreviated SeriesInstanceUID. It never accepts DICOM free text.
std::string makeSafeDicomSeriesDisplayName(const DicomSeriesDescriptor& descriptor);

// Enforces the explicit-series contract shared by fake and production readers.
// Even a single discovered series requires its UID; multiple series with no UID
// are reported as ambiguous rather than selecting the first entry.
DicomSeriesStatus selectDicomSeriesDescriptor(
    const std::vector<DicomSeriesDescriptor>& descriptors,
    const std::string& seriesInstanceUid,
    std::size_t* selectedIndex);

struct DicomSeriesDiscoveryResult {
    DicomSeriesStatus status = DicomSeriesStatus::InvalidArgument;
    std::vector<DicomSeriesDescriptor> series;
    std::vector<Diagnostic> diagnostics;

    bool ok() const { return status == DicomSeriesStatus::Ok; }
};

struct DicomSeriesReadResult {
    DicomSeriesStatus status = DicomSeriesStatus::InvalidArgument;
    DicomSeriesDescriptor descriptor;
    XQImageVolume volume;
    std::shared_ptr<XQMemoryImageBufferHandle> buffer;
    std::vector<Diagnostic> diagnostics;

    bool ok() const
    {
        return status == DicomSeriesStatus::Ok && buffer != nullptr
            && buffer->is_valid();
    }
};

// Narrow DICOM series port. Implementations may use ITK/GDCM privately, but the
// public API exposes only XQ value types and an XQ-owned resident buffer.
class IDicomSeriesReader {
public:
    virtual ~IDicomSeriesReader() = default;

    virtual DicomSeriesDiscoveryResult discover(const std::string& directory) = 0;
    virtual DicomSeriesReadResult read(const std::string& directory,
                                       const std::string& seriesInstanceUid) = 0;
};

} // namespace xq

#endif // XQ_CORE_IMAGE_I_DICOM_SERIES_READER_H
