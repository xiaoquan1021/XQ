#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "io/blob/Sha256.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

const char* severityToken(xq::DiagnosticSeverity severity)
{
    switch (severity) {
    case xq::DiagnosticSeverity::Info:
        return "info";
    case xq::DiagnosticSeverity::Warning:
        return "warning";
    case xq::DiagnosticSeverity::Error:
        return "error";
    }
    return "unknown";
}

const char* modalityToken(xq::ImageModality modality)
{
    switch (modality) {
    case xq::ImageModality::CT:
        return "ct";
    case xq::ImageModality::MR:
        return "mr";
    case xq::ImageModality::Other:
        return "other";
    case xq::ImageModality::Unknown:
        return "unknown";
    }
    return "unknown";
}

const char* scalarTypeToken(xq::ScalarType type)
{
    switch (type) {
    case xq::ScalarType::Int8:
        return "int8";
    case xq::ScalarType::UInt8:
        return "uint8";
    case xq::ScalarType::Int16:
        return "int16";
    case xq::ScalarType::UInt16:
        return "uint16";
    case xq::ScalarType::Int32:
        return "int32";
    case xq::ScalarType::UInt32:
        return "uint32";
    case xq::ScalarType::Float32:
        return "float32";
    case xq::ScalarType::Float64:
        return "float64";
    case xq::ScalarType::Unknown:
        return "unknown";
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

void printWorldSample(const xq::XQImageVolume& volume,
                      const char* name,
                      const double voxel[3])
{
    double world[3] = {};
    if (volume.voxelToWorld(voxel, world)
        != xq::XQImageVolume::TransformStatus::Ok) {
        std::printf("read.world.%s.status=geometry_not_set\n", name);
        return;
    }
    std::printf("read.world.%s.voxel=%.17g,%.17g,%.17g\n", name,
                voxel[0], voxel[1], voxel[2]);
    std::printf("read.world.%s.lps_mm=%.17g,%.17g,%.17g\n", name,
                world[0], world[1], world[2]);
}

bool printRead(xq::GdcmItkDicomSeriesReader& reader,
               const std::string& directory,
               const std::string& seriesUid,
               std::size_t readIndex)
{
    const xq::DicomSeriesReadResult result = reader.read(directory, seriesUid);
    std::printf("read.%zu.status=%s\n", readIndex,
                xq::dicomSeriesStatusToken(result.status));
    printDiagnostics("read", result.diagnostics);
    if (!result.ok()) {
        return false;
    }

    const xq::ImageGeometry& geometry = result.volume.geometry();
    std::printf("read.series_uid=%s\n",
                result.volume.dicomIdentity().seriesInstanceUid.c_str());
    std::printf("read.frame_uid=%s\n",
                result.volume.dicomIdentity().frameOfReferenceUid.c_str());
    std::printf("read.modality=%s\n", modalityToken(result.volume.modality()));
    std::printf("read.dimensions=%d,%d,%d\n", geometry.dimensions[0],
                geometry.dimensions[1], geometry.dimensions[2]);
    std::printf("read.spacing_mm=%.17g,%.17g,%.17g\n", geometry.spacing[0],
                geometry.spacing[1], geometry.spacing[2]);
    std::printf("read.origin_lps_mm=%.17g,%.17g,%.17g\n", geometry.origin[0],
                geometry.origin[1], geometry.origin[2]);
    for (int row = 0; row < 3; ++row) {
        std::printf("read.direction.%d=%.17g,%.17g,%.17g\n", row,
                    geometry.direction[row][0], geometry.direction[row][1],
                    geometry.direction[row][2]);
    }
    std::printf("read.scalar_type=%s\n",
                scalarTypeToken(result.volume.scalarType()));
    std::printf("read.component_count=%d\n", result.volume.componentCount());
    std::printf("read.intensity_range=%.17g,%.17g\n",
                result.volume.intensityRange().minimum,
                result.volume.intensityRange().maximum);
    std::printf("read.rescale=%.17g,%.17g\n", result.volume.rescaleSlope(),
                result.volume.rescaleIntercept());
    std::printf("read.voxel_count=%zu\n", result.buffer->voxelCount());
    std::printf("read.byte_count=%zu\n", result.buffer->bytes().size());
    std::printf("read.buffer_sha256=%s\n",
                xq::Sha256::hashHex(result.buffer->bytes().data(),
                                    result.buffer->bytes().size()).c_str());

    std::size_t finiteCount = 0;
    std::size_t nonZeroCount = 0;
    for (std::size_t voxel = 0; voxel < result.buffer->voxelCount(); ++voxel) {
        const double value = result.buffer->scalarAt(voxel);
        if (std::isfinite(value)) {
            ++finiteCount;
        }
        if (value != 0.0) {
            ++nonZeroCount;
        }
    }
    std::printf("read.finite_voxel_count=%zu\n", finiteCount);
    std::printf("read.nonzero_voxel_count=%zu\n", nonZeroCount);

    const double first[3] = {0.0, 0.0, 0.0};
    const double center[3] = {
        (geometry.dimensions[0] - 1) * 0.5,
        (geometry.dimensions[1] - 1) * 0.5,
        (geometry.dimensions[2] - 1) * 0.5
    };
    const double last[3] = {
        static_cast<double>(geometry.dimensions[0] - 1),
        static_cast<double>(geometry.dimensions[1] - 1),
        static_cast<double>(geometry.dimensions[2] - 1)
    };
    printWorldSample(result.volume, "first", first);
    printWorldSample(result.volume, "center", center);
    printWorldSample(result.volume, "last", last);
    return true;
}

void printUsage(const char* executable)
{
    std::fprintf(stderr,
                 "Usage: %s <dicom-directory> [--read <series-uid> | --read-all]\n",
                 executable == nullptr ? "xq_vascular_dicom_probe" : executable);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || argv == nullptr || argv[1] == nullptr) {
        printUsage(argc > 0 && argv != nullptr ? argv[0] : nullptr);
        return 1;
    }

    const std::string directory(argv[1]);
    bool readAll = false;
    std::string requestedUid;
    for (int index = 2; index < argc; ++index) {
        const std::string argument(argv[index] == nullptr ? "" : argv[index]);
        if (argument == "--read-all") {
            readAll = true;
        } else if (argument == "--read" && index + 1 < argc
                   && argv[index + 1] != nullptr) {
            requestedUid = argv[++index];
        } else {
            printUsage(argv[0]);
            return 1;
        }
    }
    if (readAll && !requestedUid.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    xq::GdcmItkDicomSeriesReader reader;
    const xq::DicomSeriesDiscoveryResult discovery = reader.discover(directory);
    std::printf("discovery.status=%s\n",
                xq::dicomSeriesStatusToken(discovery.status));
    std::printf("discovery.series_count=%zu\n", discovery.series.size());
    printDiagnostics("discovery", discovery.diagnostics);
    for (std::size_t index = 0; index < discovery.series.size(); ++index) {
        const xq::DicomSeriesDescriptor& descriptor = discovery.series[index];
        std::printf("series.%zu.uid=%s\n", index,
                    descriptor.identity.seriesInstanceUid.c_str());
        std::printf("series.%zu.frame_uid=%s\n", index,
                    descriptor.identity.frameOfReferenceUid.c_str());
        std::printf("series.%zu.safe_name=%s\n", index,
                    descriptor.safeDisplayName.c_str());
        std::printf("series.%zu.modality=%s\n", index,
                    modalityToken(descriptor.modality));
        std::printf("series.%zu.dimensions=%d,%d,%d\n", index,
                    descriptor.dimensions[0], descriptor.dimensions[1],
                    descriptor.dimensions[2]);
        std::printf("series.%zu.slice_count=%zu\n", index,
                    descriptor.sliceCount);
        std::printf("series.%zu.fingerprint=%s\n", index,
                    descriptor.contentFingerprint.c_str());
    }
    if (!discovery.ok()) {
        return 2;
    }

    std::vector<std::string> uids;
    if (readAll) {
        for (const xq::DicomSeriesDescriptor& descriptor : discovery.series) {
            uids.push_back(descriptor.identity.seriesInstanceUid);
        }
    } else if (!requestedUid.empty()) {
        uids.push_back(requestedUid);
    } else if (discovery.series.size() == 1) {
        uids.push_back(discovery.series.front().identity.seriesInstanceUid);
    }

    bool allReadsOk = true;
    for (std::size_t index = 0; index < uids.size(); ++index) {
        allReadsOk = printRead(reader, directory, uids[index], index) && allReadsOk;
    }
    std::printf("read.requested_count=%zu\n", uids.size());
    return allReadsOk ? 0 : 3;
}
