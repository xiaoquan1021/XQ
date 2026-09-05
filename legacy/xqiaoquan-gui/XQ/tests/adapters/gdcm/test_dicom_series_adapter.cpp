#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

bool diagnosticsContainText(const std::vector<xq::Diagnostic>& diagnostics,
                            const std::string& text)
{
    for (const xq::Diagnostic& diagnostic : diagnostics) {
        if (diagnostic.message().find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void printDiagnostics(const std::vector<xq::Diagnostic>& diagnostics)
{
    for (const xq::Diagnostic& diagnostic : diagnostics) {
        std::fprintf(stderr, "%s: %s\n",
                     diagnostic.code().c_str(), diagnostic.message().c_str());
    }
}

bool nearlyEqual(double a, double b, double tolerance = 1e-5)
{
    return std::abs(a - b) <= tolerance;
}

bool hasDimensions(const int actual[3], int x, int y, int z)
{
    return actual[0] == x && actual[1] == y && actual[2] == z;
}

} // namespace

int main()
{
    xq::GdcmItkDicomSeriesReader reader;

    const xq::DicomSeriesDiscoveryResult emptyArgument = reader.discover("");
    if (emptyArgument.status != xq::DicomSeriesStatus::InvalidArgument
        || emptyArgument.diagnostics.empty()) {
        return fail("empty source path is rejected explicitly", __LINE__);
    }

    const std::string missingPath = "xq-dicom-source-that-does-not-exist";
    const xq::DicomSeriesDiscoveryResult missing = reader.discover(missingPath);
    if (missing.status != xq::DicomSeriesStatus::SourceNotFound
        || missing.diagnostics.empty()
        || diagnosticsContainText(missing.diagnostics, missingPath)) {
        return fail("missing source diagnostics do not echo the source path", __LINE__);
    }

    const std::filesystem::path tempRoot = std::filesystem::temp_directory_path()
        / (std::string("xq-dicom-adapter-")
           + std::to_string(std::chrono::high_resolution_clock::now()
                                .time_since_epoch().count()));
    std::error_code error;
    if (!std::filesystem::create_directories(tempRoot, error) || error) {
        return fail("temporary DICOM test directory created", __LINE__);
    }

    const xq::DicomSeriesDiscoveryResult noSeries = reader.discover(tempRoot.string());
    if (noSeries.status != xq::DicomSeriesStatus::NoSeries
        || noSeries.series.size() != 0) {
        std::filesystem::remove_all(tempRoot, error);
        return fail("empty directory reports no series", __LINE__);
    }

    {
        std::ofstream textFile(tempRoot / "not-dicom.txt", std::ios::binary);
        textFile << "not a DICOM image";
    }
    const xq::DicomSeriesDiscoveryResult ignored = reader.discover(tempRoot.string());
    if (ignored.status != xq::DicomSeriesStatus::NoSeries
        || ignored.diagnostics.size() < 2) {
        std::filesystem::remove_all(tempRoot, error);
        return fail("mixed invalid input reports ignored files and no series", __LINE__);
    }

    const xq::DicomSeriesReadResult readNoSeries = reader.read(tempRoot.string(), "1.2.3");
    if (readNoSeries.status != xq::DicomSeriesStatus::NoSeries
        || readNoSeries.buffer != nullptr) {
        std::filesystem::remove_all(tempRoot, error);
        return fail("read from a directory without a series has no side effects", __LINE__);
    }

    std::filesystem::remove_all(tempRoot, error);
    if (error) {
        return fail("temporary DICOM test directory removed", __LINE__);
    }

    const std::filesystem::path fixtureRoot(XQ_DICOM_FIXTURE_ROOT);
    const std::filesystem::path regularDir = fixtureRoot / "regular-oblique";
    const std::string regularUid = "1.2.826.0.1.3680043.10.543.101";
    const xq::DicomSeriesDiscoveryResult regularDiscovery =
        reader.discover(regularDir.string());
    if (!regularDiscovery.ok() || regularDiscovery.series.size() != 1) {
        return fail("regular oblique fixture discovers one series", __LINE__);
    }
    const xq::DicomSeriesDescriptor& regularDescriptor =
        regularDiscovery.series.front();
    if (regularDescriptor.identity.seriesInstanceUid != regularUid
        || regularDescriptor.identity.studyInstanceUid
            != "1.2.826.0.1.3680043.10.543.100"
        || regularDescriptor.identity.frameOfReferenceUid
            != "1.2.826.0.1.3680043.10.543.109"
        || regularDescriptor.modality != xq::ImageModality::CT
        || !hasDimensions(regularDescriptor.dimensions, 4, 3, 4)
        || regularDescriptor.sliceCount != 4
        || regularDescriptor.safeDisplayName
            != "CT 4x3x4 (4 slices, UID ...10.543.101)") {
        return fail("regular discovery exposes only deterministic technical metadata", __LINE__);
    }

    const xq::DicomSeriesReadResult selectionRequired =
        reader.read(regularDir.string(), "");
    if (selectionRequired.status != xq::DicomSeriesStatus::SeriesSelectionRequired
        || selectionRequired.buffer != nullptr) {
        return fail("single-series read still requires an explicit UID", __LINE__);
    }

    const xq::DicomSeriesReadResult regular =
        reader.read(regularDir.string(), regularUid);
    if (!regular.ok()
        || regular.descriptor.contentFingerprint.rfind(
               "dicom-series-v1:sha256:", 0) != 0
        || regular.volume.scalarType() != xq::ScalarType::Float32
        || regular.volume.componentCount() != 1
        || regular.volume.modality() != xq::ImageModality::CT
        || !regular.volume.hasDicomIdentity()
        || regular.volume.dicomIdentity().seriesInstanceUid != regularUid
        || !regular.buffer->is_valid()
        || !hasDimensions(regular.volume.geometry().dimensions, 4, 3, 4)) {
        std::fprintf(stderr, "status: %s\n", xq::dicomSeriesStatusToken(regular.status));
        printDiagnostics(regular.diagnostics);
        return fail("regular oblique fixture decodes to XQ-owned image data", __LINE__);
    }

    const xq::ImageGeometry& geometry = regular.volume.geometry();
    const double expectedDirection[3][3] = {
        {0.8660254037844386, -0.4330127018922193, 0.25},
        {0.5, 0.75, -0.4330127018922193},
        {0.0, 0.5, 0.8660254037844386}
    };
    if (geometry.coordinateSystem != xq::ImageCoordinateSystem::LPS
        || !nearlyEqual(geometry.spacing[0], 0.7)
        || !nearlyEqual(geometry.spacing[1], 0.8)
        || !nearlyEqual(geometry.spacing[2], 1.5)
        || !nearlyEqual(geometry.origin[0], 10.0)
        || !nearlyEqual(geometry.origin[1], 20.0)
        || !nearlyEqual(geometry.origin[2], 30.0)) {
        return fail("regular oblique geometry preserves LPS millimetres", __LINE__);
    }
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            if (!nearlyEqual(geometry.direction[row][column],
                             expectedDirection[row][column])) {
                return fail("regular oblique direction is preserved", __LINE__);
            }
        }
    }

    const double voxel[3] = {2.0, 1.0, 3.0};
    double world[3] = {};
    double roundTrip[3] = {};
    if (regular.volume.voxelToWorld(voxel, world)
            != xq::XQImageVolume::TransformStatus::Ok
        || regular.volume.worldToVoxel(world, roundTrip)
            != xq::XQImageVolume::TransformStatus::Ok
        || !nearlyEqual(roundTrip[0], voxel[0])
        || !nearlyEqual(roundTrip[1], voxel[1])
        || !nearlyEqual(roundTrip[2], voxel[2])) {
        return fail("oblique voxel and world transforms round-trip", __LINE__);
    }

    if (!nearlyEqual(regular.buffer->scalarAt(0), -824.0)
        || !nearlyEqual(regular.buffer->scalarAt(47), -682.0)
        || !nearlyEqual(regular.volume.intensityRange().minimum, -824.0)
        || !nearlyEqual(regular.volume.intensityRange().maximum, -682.0)
        || !nearlyEqual(regular.volume.windowCenter(), 40.0)
        || !nearlyEqual(regular.volume.windowWidth(), 400.0)
        || !nearlyEqual(regular.volume.rescaleSlope(), 1.0)
        || !nearlyEqual(regular.volume.rescaleIntercept(), 0.0)) {
        return fail("modality rescale is applied exactly once", __LINE__);
    }

    const xq::DicomSeriesReadResult regularAgain =
        reader.read(regularDir.string(), regularUid);
    if (!regularAgain.ok()
        || regularAgain.descriptor.contentFingerprint
            != regular.descriptor.contentFingerprint) {
        return fail("ordered instance fingerprint is deterministic", __LINE__);
    }

    const std::filesystem::path multiDir = fixtureRoot / "multi-series";
    const xq::DicomSeriesDiscoveryResult multi = reader.discover(multiDir.string());
    if (!multi.ok() || multi.series.size() != 2
        || multi.series[0].identity.seriesInstanceUid
            != "1.2.826.0.1.3680043.10.543.201"
        || multi.series[1].identity.seriesInstanceUid
            != "1.2.826.0.1.3680043.10.543.301") {
        return fail("multi-series discovery is deterministic", __LINE__);
    }
    const xq::DicomSeriesReadResult ambiguous = reader.read(multiDir.string(), "");
    if (ambiguous.status != xq::DicomSeriesStatus::AmbiguousSeries
        || ambiguous.buffer != nullptr) {
        return fail("multi-series read never selects the first series", __LINE__);
    }
    const xq::DicomSeriesReadResult unknown = reader.read(multiDir.string(), "9.9.9");
    if (unknown.status != xq::DicomSeriesStatus::SeriesNotFound
        || unknown.buffer != nullptr) {
        return fail("unknown explicit UID does not fall back", __LINE__);
    }
    const xq::DicomSeriesReadResult selectedMr =
        reader.read(multiDir.string(), "1.2.826.0.1.3680043.10.543.301");
    if (!selectedMr.ok()
        || selectedMr.volume.modality() != xq::ImageModality::MR
        || !hasDimensions(selectedMr.volume.geometry().dimensions, 2, 2, 3)
        || !nearlyEqual(selectedMr.buffer->scalarAt(0), 500.0)) {
        return fail("explicit UID selects the requested series", __LINE__);
    }

    const std::filesystem::path nonUniformDir = fixtureRoot / "non-uniform";
    const xq::DicomSeriesDiscoveryResult nonUniformDiscovery =
        reader.discover(nonUniformDir.string());
    if (!nonUniformDiscovery.ok() || nonUniformDiscovery.series.size() != 1) {
        return fail("non-uniform fixture remains discoverable", __LINE__);
    }
    const xq::DicomSeriesReadResult nonUniform = reader.read(
        nonUniformDir.string(),
        nonUniformDiscovery.series.front().identity.seriesInstanceUid);
    if (nonUniform.status != xq::DicomSeriesStatus::UnsupportedGeometry
        || nonUniform.buffer != nullptr) {
        return fail("non-uniform slice spacing is rejected", __LINE__);
    }

    const std::filesystem::path mixedOrientationDir =
        fixtureRoot / "mixed-orientation";
    const xq::DicomSeriesDiscoveryResult mixedDiscovery =
        reader.discover(mixedOrientationDir.string());
    if (!mixedDiscovery.ok() || mixedDiscovery.series.size() != 1) {
        return fail("mixed-orientation fixture remains discoverable", __LINE__);
    }
    const xq::DicomSeriesReadResult mixedOrientation = reader.read(
        mixedOrientationDir.string(),
        mixedDiscovery.series.front().identity.seriesInstanceUid);
    if (mixedOrientation.status != xq::DicomSeriesStatus::InconsistentSeries
        || mixedOrientation.buffer != nullptr) {
        return fail("mixed slice orientation is rejected", __LINE__);
    }

    std::printf("OK: GDCM/ITK DICOM adapter geometry, intensity, selection and failures\n");
    return 0;
}
