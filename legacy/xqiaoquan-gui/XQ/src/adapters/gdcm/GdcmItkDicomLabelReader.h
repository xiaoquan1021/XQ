#ifndef XQ_ADAPTERS_GDCM_GDCM_ITK_DICOM_LABEL_READER_H
#define XQ_ADAPTERS_GDCM_GDCM_ITK_DICOM_LABEL_READER_H

#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "core/XQSegmentationMask.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace xq {

enum class DicomLabelStatus {
    Ok,
    InvalidProfile,
    SourceReadFailed,
    InvalidSourceBuffer,
    UnsupportedLabelValues,
    EmptyForeground
};

const char* dicomLabelStatusToken(DicomLabelStatus status);

struct DicomBinaryLabelProfile {
    double backgroundValue = 0.0;
    double foregroundValue = 1.0;
    std::uint8_t outputLabel = 1;
    std::string labelName;
};

struct DicomLabelReadResult {
    DicomLabelStatus status = DicomLabelStatus::InvalidProfile;
    DicomSeriesStatus sourceStatus = DicomSeriesStatus::InvalidArgument;
    DicomSeriesDescriptor descriptor;
    std::shared_ptr<XQSegmentationMask> mask;
    std::size_t backgroundVoxelCount = 0;
    std::size_t foregroundVoxelCount = 0;
    std::vector<Diagnostic> diagnostics;

    bool ok() const
    {
        return status == DicomLabelStatus::Ok && mask != nullptr
            && mask->is_valid();
    }
};

// Reads a DICOM label volume through the production GDCM/ITK image reader,
// then performs only the dataset-specific binary-label interpretation. No
// second DICOM enumerator or pixel decoder is maintained here.
class GdcmItkDicomLabelReader final {
public:
    DicomLabelReadResult read(const std::string& directory,
                              const std::string& seriesInstanceUid,
                              const DicomBinaryLabelProfile& profile);

private:
    GdcmItkDicomSeriesReader imageReader_;
};

} // namespace xq

#endif // XQ_ADAPTERS_GDCM_GDCM_ITK_DICOM_LABEL_READER_H
