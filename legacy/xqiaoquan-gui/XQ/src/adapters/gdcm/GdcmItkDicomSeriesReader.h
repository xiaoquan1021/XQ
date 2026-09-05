#ifndef XQ_ADAPTERS_GDCM_GDCM_ITK_DICOM_SERIES_READER_H
#define XQ_ADAPTERS_GDCM_GDCM_ITK_DICOM_SERIES_READER_H

#include "core/image/IDicomSeriesReader.h"

namespace xq {

// Production DICOM adapter backed by the official GDCM/ITK libraries. The
// public header intentionally contains no external-library type; all GDCM and
// ITK objects are confined to the implementation translation unit.
class GdcmItkDicomSeriesReader final : public IDicomSeriesReader {
public:
    DicomSeriesDiscoveryResult discover(const std::string& directory) override;
    DicomSeriesReadResult read(const std::string& directory,
                               const std::string& seriesInstanceUid) override;
};

} // namespace xq

#endif // XQ_ADAPTERS_GDCM_GDCM_ITK_DICOM_SERIES_READER_H
