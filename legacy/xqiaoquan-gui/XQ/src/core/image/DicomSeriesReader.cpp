#include "core/image/IDicomSeriesReader.h"

#include <sstream>

namespace xq {
namespace {

const char* modalityToken(ImageModality modality)
{
    switch (modality) {
    case ImageModality::CT: return "CT";
    case ImageModality::MR: return "MR";
    case ImageModality::Other: return "OTHER";
    case ImageModality::Unknown: break;
    }
    return "UNKNOWN";
}

std::string abbreviatedUid(const std::string& uid)
{
    const std::size_t keep = 10;
    if (uid.size() <= keep) {
        return uid;
    }
    return std::string("...") + uid.substr(uid.size() - keep);
}

} // namespace

const char* dicomSeriesStatusToken(DicomSeriesStatus status)
{
    switch (status) {
    case DicomSeriesStatus::Ok: return "ok";
    case DicomSeriesStatus::InvalidArgument: return "invalid_argument";
    case DicomSeriesStatus::SourceNotFound: return "source_not_found";
    case DicomSeriesStatus::SourceUnreadable: return "source_unreadable";
    case DicomSeriesStatus::NoSeries: return "no_series";
    case DicomSeriesStatus::SeriesSelectionRequired: return "series_selection_required";
    case DicomSeriesStatus::AmbiguousSeries: return "ambiguous_series";
    case DicomSeriesStatus::SeriesNotFound: return "series_not_found";
    case DicomSeriesStatus::ReadFailed: return "read_failed";
    case DicomSeriesStatus::InconsistentSeries: return "inconsistent_series";
    case DicomSeriesStatus::UnsupportedGeometry: return "unsupported_geometry";
    case DicomSeriesStatus::UnsupportedPixelFormat: return "unsupported_pixel_format";
    case DicomSeriesStatus::UnsupportedMultiFrame: return "unsupported_multi_frame";
    case DicomSeriesStatus::UnsupportedRescale: return "unsupported_rescale";
    case DicomSeriesStatus::SourceChanged: return "source_changed";
    }
    return "invalid_argument";
}

std::string makeSafeDicomSeriesDisplayName(const DicomSeriesDescriptor& descriptor)
{
    std::ostringstream out;
    out << modalityToken(descriptor.modality) << " "
        << descriptor.dimensions[0] << "x"
        << descriptor.dimensions[1] << "x"
        << descriptor.dimensions[2] << " ("
        << descriptor.sliceCount << " slices, UID "
        << abbreviatedUid(descriptor.identity.seriesInstanceUid) << ")";
    return out.str();
}

DicomSeriesStatus selectDicomSeriesDescriptor(
    const std::vector<DicomSeriesDescriptor>& descriptors,
    const std::string& seriesInstanceUid,
    std::size_t* selectedIndex)
{
    if (selectedIndex == nullptr) {
        return DicomSeriesStatus::InvalidArgument;
    }
    *selectedIndex = 0;
    if (descriptors.empty()) {
        return DicomSeriesStatus::NoSeries;
    }
    if (seriesInstanceUid.empty()) {
        return descriptors.size() > 1
            ? DicomSeriesStatus::AmbiguousSeries
            : DicomSeriesStatus::SeriesSelectionRequired;
    }

    bool found = false;
    std::size_t foundIndex = 0;
    for (std::size_t i = 0; i < descriptors.size(); ++i) {
        if (descriptors[i].identity.seriesInstanceUid == seriesInstanceUid) {
            if (found) {
                return DicomSeriesStatus::InconsistentSeries;
            }
            found = true;
            foundIndex = i;
        }
    }
    if (!found) {
        return DicomSeriesStatus::SeriesNotFound;
    }
    *selectedIndex = foundIndex;
    return DicomSeriesStatus::Ok;
}

} // namespace xq
