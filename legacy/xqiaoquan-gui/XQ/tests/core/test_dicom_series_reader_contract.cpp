#include "core/image/IDicomSeriesReader.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

xq::DicomSeriesDescriptor descriptor(const std::string& study,
                                     const std::string& series,
                                     const std::string& frame,
                                     xq::ImageModality modality,
                                     int x,
                                     int y,
                                     int z,
                                     const std::string& fingerprint)
{
    xq::DicomSeriesDescriptor value;
    value.identity.studyInstanceUid = study;
    value.identity.seriesInstanceUid = series;
    value.identity.frameOfReferenceUid = frame;
    value.modality = modality;
    value.dimensions[0] = x;
    value.dimensions[1] = y;
    value.dimensions[2] = z;
    value.sliceCount = static_cast<std::size_t>(z);
    value.contentFingerprint = fingerprint;
    value.safeDisplayName = xq::makeSafeDicomSeriesDisplayName(value);
    return value;
}

class FakeDicomSeriesReader : public xq::IDicomSeriesReader {
public:
    explicit FakeDicomSeriesReader(std::vector<xq::DicomSeriesDescriptor> series)
        : series_(std::move(series))
    {
    }

    xq::DicomSeriesDiscoveryResult discover(const std::string& directory) override
    {
        xq::DicomSeriesDiscoveryResult result;
        if (directory.empty()) {
            result.status = xq::DicomSeriesStatus::InvalidArgument;
            return result;
        }
        result.status = series_.empty()
            ? xq::DicomSeriesStatus::NoSeries
            : xq::DicomSeriesStatus::Ok;
        result.series = series_;
        return result;
    }

    xq::DicomSeriesReadResult read(const std::string& directory,
                                   const std::string& seriesInstanceUid) override
    {
        xq::DicomSeriesReadResult result;
        if (directory.empty()) {
            result.status = xq::DicomSeriesStatus::InvalidArgument;
            return result;
        }
        std::size_t selected = 0;
        result.status = xq::selectDicomSeriesDescriptor(
            series_, seriesInstanceUid, &selected);
        if (result.status != xq::DicomSeriesStatus::Ok) {
            return result;
        }

        result.descriptor = series_[selected];
        xq::ImageGeometry geometry = {};
        for (int i = 0; i < 3; ++i) {
            geometry.dimensions[i] = result.descriptor.dimensions[i];
            geometry.spacing[i] = 1.0;
            geometry.direction[i][i] = 1.0;
        }
        geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;
        result.volume.setGeometry(geometry);
        result.volume.setScalarType(xq::ScalarType::Int16);
        result.volume.setComponentCount(1);
        result.volume.setModality(result.descriptor.modality);
        result.volume.setDicomIdentity(result.descriptor.identity);
        result.volume.setRescaleSlope(1.0);
        result.volume.setRescaleIntercept(0.0);

        const std::size_t voxelCount =
            static_cast<std::size_t>(geometry.dimensions[0])
            * static_cast<std::size_t>(geometry.dimensions[1])
            * static_cast<std::size_t>(geometry.dimensions[2]);
        std::vector<std::uint8_t> bytes(voxelCount * sizeof(std::int16_t));
        for (std::size_t i = 0; i < voxelCount; ++i) {
            const std::int16_t value = static_cast<std::int16_t>(selected * 100 + i);
            std::memcpy(bytes.data() + i * sizeof(value), &value, sizeof(value));
        }
        result.buffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
            xq::ScalarType::Int16, geometry.dimensions, 1, std::move(bytes));
        result.status = result.buffer->is_valid()
            ? xq::DicomSeriesStatus::Ok
            : xq::DicomSeriesStatus::ReadFailed;
        return result;
    }

private:
    std::vector<xq::DicomSeriesDescriptor> series_;
};

} // namespace

int main()
{
    const xq::DicomSeriesDescriptor ct = descriptor(
        "1.2.826.0.1.3680043.10.100.1",
        "1.2.826.0.1.3680043.10.100.1000000001",
        "1.2.826.0.1.3680043.10.100.9",
        xq::ImageModality::CT, 2, 3, 4, "sha256:ct");
    const xq::DicomSeriesDescriptor mr = descriptor(
        "1.2.826.0.1.3680043.10.100.2",
        "1.2.826.0.1.3680043.10.100.2000000002",
        "1.2.826.0.1.3680043.10.100.8",
        xq::ImageModality::MR, 3, 2, 5, "sha256:mr");

    if (ct.safeDisplayName.find("CT 2x3x4") == std::string::npos
        || ct.safeDisplayName.find("4 slices") == std::string::npos
        || ct.safeDisplayName.find(ct.identity.seriesInstanceUid) != std::string::npos) {
        return fail("safe display name uses only technical fields and abbreviated UID", __LINE__);
    }

    std::size_t selected = 99;
    if (xq::selectDicomSeriesDescriptor({ct, mr}, "", &selected)
            != xq::DicomSeriesStatus::AmbiguousSeries
        || selected != 0) {
        return fail("multi-series selection never chooses the first series implicitly", __LINE__);
    }
    if (xq::selectDicomSeriesDescriptor({ct}, "", &selected)
        != xq::DicomSeriesStatus::SeriesSelectionRequired) {
        return fail("single-series read still requires an explicit UID", __LINE__);
    }
    if (xq::selectDicomSeriesDescriptor({ct, mr}, mr.identity.seriesInstanceUid, &selected)
            != xq::DicomSeriesStatus::Ok
        || selected != 1) {
        return fail("explicit UID selects the requested series", __LINE__);
    }
    if (xq::selectDicomSeriesDescriptor({ct, mr}, "1.2.3.missing", &selected)
        != xq::DicomSeriesStatus::SeriesNotFound) {
        return fail("unknown UID never falls back to another series", __LINE__);
    }
    if (xq::selectDicomSeriesDescriptor({}, ct.identity.seriesInstanceUid, &selected)
        != xq::DicomSeriesStatus::NoSeries) {
        return fail("empty discovery has a stable no-series result", __LINE__);
    }
    if (xq::selectDicomSeriesDescriptor({ct}, ct.identity.seriesInstanceUid, nullptr)
        != xq::DicomSeriesStatus::InvalidArgument) {
        return fail("selection rejects a null output index", __LINE__);
    }

    FakeDicomSeriesReader reader({ct, mr});
    const xq::DicomSeriesDiscoveryResult discovery = reader.discover("fixture");
    if (!discovery.ok() || discovery.series.size() != 2
        || discovery.series[0].identity.seriesInstanceUid
            != ct.identity.seriesInstanceUid) {
        return fail("reader port exposes deterministic discovery descriptors", __LINE__);
    }
    const xq::DicomSeriesReadResult ambiguous = reader.read("fixture", "");
    if (ambiguous.status != xq::DicomSeriesStatus::AmbiguousSeries
        || ambiguous.buffer != nullptr) {
        return fail("reader port preserves explicit multi-series failure semantics", __LINE__);
    }
    const xq::DicomSeriesReadResult read = reader.read(
        "fixture", mr.identity.seriesInstanceUid);
    if (!read.ok() || read.descriptor.identity.seriesInstanceUid
            != mr.identity.seriesInstanceUid
        || !read.volume.hasDicomIdentity()
        || read.volume.dicomIdentity().seriesInstanceUid
            != mr.identity.seriesInstanceUid
        || read.volume.geometry().coordinateSystem != xq::ImageCoordinateSystem::LPS
        || read.volume.rescaleSlope() != 1.0
        || read.volume.rescaleIntercept() != 0.0
        || read.buffer->scalarAt(0) != 100.0) {
        return fail("reader port returns XQ-owned LPS metadata and resident scalar buffer", __LINE__);
    }

    if (std::string(xq::dicomSeriesStatusToken(xq::DicomSeriesStatus::SourceChanged))
            != "source_changed"
        || std::string(xq::dicomSeriesStatusToken(
               xq::DicomSeriesStatus::UnsupportedMultiFrame))
            != "unsupported_multi_frame") {
        return fail("DICOM status tokens are stable for diagnostics and persistence", __LINE__);
    }

    std::printf("OK: DICOM reader contract\n");
    return 0;
}
