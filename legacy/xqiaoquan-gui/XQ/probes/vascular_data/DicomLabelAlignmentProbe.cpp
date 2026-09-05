#include "adapters/gdcm/GdcmItkDicomLabelReader.h"
#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "io/blob/Sha256.h"
#include "services/image/DicomServiceSupport.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace {

const char* severityToken(xq::DiagnosticSeverity severity)
{
    switch (severity) {
    case xq::DiagnosticSeverity::Info: return "info";
    case xq::DiagnosticSeverity::Warning: return "warning";
    case xq::DiagnosticSeverity::Error: return "error";
    }
    return "unknown";
}

void printDiagnostics(const char* prefix,
                      const std::vector<xq::Diagnostic>& diagnostics)
{
    std::printf("%s.diagnostic_count=%zu\n", prefix, diagnostics.size());
    for (std::size_t index = 0; index < diagnostics.size(); ++index) {
        std::printf("%s.diagnostic.%zu.severity=%s\n", prefix, index,
                    severityToken(diagnostics[index].severity()));
        std::printf("%s.diagnostic.%zu.code=%s\n", prefix, index,
                    diagnostics[index].code().c_str());
    }
}

bool parseFiniteDouble(const char* text, double* value)
{
    if (text == nullptr || value == nullptr || *text == '\0') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (errno == ERANGE || end == text || *end != '\0'
        || !std::isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

bool discoverSingleSeries(xq::GdcmItkDicomSeriesReader* reader,
                          const std::string& directory,
                          const char* prefix,
                          xq::DicomSeriesDescriptor* descriptor)
{
    if (reader == nullptr || descriptor == nullptr) {
        return false;
    }
    const xq::DicomSeriesDiscoveryResult discovery = reader->discover(directory);
    std::printf("%s.discovery_status=%s\n", prefix,
                xq::dicomSeriesStatusToken(discovery.status));
    std::printf("%s.series_count=%zu\n", prefix, discovery.series.size());
    printDiagnostics(prefix, discovery.diagnostics);
    if (!discovery.ok() || discovery.series.size() != 1) {
        return false;
    }
    *descriptor = discovery.series.front();
    std::printf("%s.series_uid=%s\n", prefix,
                descriptor->identity.seriesInstanceUid.c_str());
    std::printf("%s.frame_uid_present=%s\n", prefix,
                descriptor->identity.frameOfReferenceUid.empty() ? "false" : "true");
    return true;
}

void printGeometry(const char* prefix, const xq::ImageGeometry& geometry)
{
    std::printf("%s.dimensions=%d,%d,%d\n", prefix,
                geometry.dimensions[0], geometry.dimensions[1],
                geometry.dimensions[2]);
    std::printf("%s.spacing_mm=%.17g,%.17g,%.17g\n", prefix,
                geometry.spacing[0], geometry.spacing[1], geometry.spacing[2]);
    std::printf("%s.origin_lps_mm=%.17g,%.17g,%.17g\n", prefix,
                geometry.origin[0], geometry.origin[1], geometry.origin[2]);
    for (int row = 0; row < 3; ++row) {
        std::printf("%s.direction.%d=%.17g,%.17g,%.17g\n", prefix, row,
                    geometry.direction[row][0],
                    geometry.direction[row][1],
                    geometry.direction[row][2]);
    }
}

bool printWorldEvidence(const char* prefix, const xq::ImageGeometry& geometry)
{
    xq::XQImageVolume volume;
    volume.setGeometry(geometry);

    double minimum[3] = {
        (std::numeric_limits<double>::max)(),
        (std::numeric_limits<double>::max)(),
        (std::numeric_limits<double>::max)()
    };
    double maximum[3] = {
        (std::numeric_limits<double>::lowest)(),
        (std::numeric_limits<double>::lowest)(),
        (std::numeric_limits<double>::lowest)()
    };
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                const double voxel[3] = {
                    x == 0 ? 0.0 : static_cast<double>(geometry.dimensions[0] - 1),
                    y == 0 ? 0.0 : static_cast<double>(geometry.dimensions[1] - 1),
                    z == 0 ? 0.0 : static_cast<double>(geometry.dimensions[2] - 1)
                };
                double world[3] = {};
                if (volume.voxelToWorld(voxel, world)
                    != xq::XQImageVolume::TransformStatus::Ok) {
                    return false;
                }
                for (int axis = 0; axis < 3; ++axis) {
                    minimum[axis] = (std::min)(minimum[axis], world[axis]);
                    maximum[axis] = (std::max)(maximum[axis], world[axis]);
                }
            }
        }
    }
    std::printf("%s.bounds_lps_mm=%.17g,%.17g;%.17g,%.17g;%.17g,%.17g\n",
                prefix,
                minimum[0], maximum[0], minimum[1], maximum[1],
                minimum[2], maximum[2]);

    const double samples[3][3] = {
        {0.0, 0.0, 0.0},
        {(geometry.dimensions[0] - 1) * 0.5,
         (geometry.dimensions[1] - 1) * 0.5,
         (geometry.dimensions[2] - 1) * 0.5},
        {static_cast<double>(geometry.dimensions[0] - 1),
         static_cast<double>(geometry.dimensions[1] - 1),
         static_cast<double>(geometry.dimensions[2] - 1)}
    };
    const char* names[3] = {"first", "center", "last"};
    for (int sample = 0; sample < 3; ++sample) {
        double world[3] = {};
        double roundTrip[3] = {};
        if (volume.voxelToWorld(samples[sample], world)
                != xq::XQImageVolume::TransformStatus::Ok
            || volume.worldToVoxel(world, roundTrip)
                != xq::XQImageVolume::TransformStatus::Ok) {
            return false;
        }
        std::printf("%s.sample.%s.index=%.17g,%.17g,%.17g\n", prefix,
                    names[sample], samples[sample][0], samples[sample][1],
                    samples[sample][2]);
        std::printf("%s.sample.%s.lps_mm=%.17g,%.17g,%.17g\n", prefix,
                    names[sample], world[0], world[1], world[2]);
        std::printf("%s.sample.%s.roundtrip_index=%.17g,%.17g,%.17g\n",
                    prefix, names[sample], roundTrip[0], roundTrip[1],
                    roundTrip[2]);
    }
    return true;
}

void printUsage(const char* executable)
{
    std::fprintf(stderr,
                 "Usage: %s <image-dicom-directory> <label-dicom-directory> <label-name> [foreground-value]\n",
                 executable == nullptr
                     ? "xq_vascular_label_alignment_probe"
                     : executable);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 4 || argc > 5 || argv == nullptr) {
        printUsage(argc > 0 && argv != nullptr ? argv[0] : nullptr);
        return 1;
    }

    double foregroundValue = 255.0;
    if (argc == 5 && !parseFiniteDouble(argv[4], &foregroundValue)) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string imageDirectory(argv[1]);
    const std::string labelDirectory(argv[2]);
    const std::string labelName(argv[3]);

    xq::GdcmItkDicomSeriesReader imageReader;
    xq::GdcmItkDicomSeriesReader labelDiscoveryReader;
    xq::DicomSeriesDescriptor imageDescriptor;
    xq::DicomSeriesDescriptor labelDescriptor;
    if (!discoverSingleSeries(&imageReader, imageDirectory, "image",
                              &imageDescriptor)
        || !discoverSingleSeries(&labelDiscoveryReader, labelDirectory,
                                 "label", &labelDescriptor)) {
        return 2;
    }

    const xq::DicomSeriesReadResult image = imageReader.read(
        imageDirectory, imageDescriptor.identity.seriesInstanceUid);
    std::printf("image.read_status=%s\n",
                xq::dicomSeriesStatusToken(image.status));
    printDiagnostics("image.read", image.diagnostics);
    if (!image.ok()) {
        return 3;
    }

    xq::DicomBinaryLabelProfile profile;
    profile.backgroundValue = 0.0;
    profile.foregroundValue = foregroundValue;
    profile.outputLabel = 1;
    profile.labelName = labelName;
    xq::GdcmItkDicomLabelReader labelReader;
    const xq::DicomLabelReadResult label = labelReader.read(
        labelDirectory, labelDescriptor.identity.seriesInstanceUid, profile);
    std::printf("label.read_status=%s\n",
                xq::dicomLabelStatusToken(label.status));
    printDiagnostics("label.read", label.diagnostics);
    if (!label.ok()) {
        return 4;
    }

    printGeometry("image.geometry", image.volume.geometry());
    printGeometry("label.geometry", label.mask->geometry());
    const bool geometryEqual = xq::sameImageGeometry(
        image.volume.geometry(), label.mask->geometry());
    std::printf("alignment.geometry_equal=%s\n",
                geometryEqual ? "true" : "false");
    const bool imageFrameMissing =
        image.volume.dicomIdentity().frameOfReferenceUid.empty();
    const bool labelFrameMissing =
        label.descriptor.identity.frameOfReferenceUid.empty();
    const bool frameCompatible =
        imageFrameMissing && labelFrameMissing
        || (!imageFrameMissing && !labelFrameMissing
            && image.volume.dicomIdentity().frameOfReferenceUid
                == label.descriptor.identity.frameOfReferenceUid);
    std::printf("alignment.frame_identity=%s\n",
                imageFrameMissing && labelFrameMissing
                    ? "missing_both"
                    : (frameCompatible ? "equal" : "mismatch"));

    const std::vector<std::uint8_t>& imageBytes = image.buffer->bytes();
    const std::vector<xq::XQSegmentationMask::LabelType>& labelBytes =
        label.mask->voxels();
    std::printf("image.series_fingerprint=%s\n",
                image.descriptor.contentFingerprint.c_str());
    std::printf("image.buffer_sha256=%s\n",
                xq::Sha256::hashHex(imageBytes.data(), imageBytes.size()).c_str());
    std::printf("label.series_fingerprint=%s\n",
                label.descriptor.contentFingerprint.c_str());
    std::printf("label.mask_sha256=%s\n",
                xq::Sha256::hashHex(labelBytes.data(), labelBytes.size()).c_str());
    std::printf("label.background_voxels=%zu\n", label.backgroundVoxelCount);
    std::printf("label.foreground_voxels=%zu\n", label.foregroundVoxelCount);
    std::printf("label.foreground_count_matches_mask=%s\n",
                label.foregroundVoxelCount == label.mask->foregroundVoxelCount()
                    ? "true"
                    : "false");

    const bool imageWorldOk =
        printWorldEvidence("image.world", image.volume.geometry());
    const bool labelWorldOk =
        printWorldEvidence("label.world", label.mask->geometry());
    std::printf("alignment.world_evidence_ok=%s\n",
                imageWorldOk && labelWorldOk ? "true" : "false");
    return geometryEqual && frameCompatible && imageWorldOk && labelWorldOk
            && label.foregroundVoxelCount == label.mask->foregroundVoxelCount()
        ? 0
        : 5;
}
