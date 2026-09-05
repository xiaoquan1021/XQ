#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "adapters/itk/ItkVascularPreprocessor.h"
#include "core/source/ResidentVoxelSource.h"

#include <cstdio>
#include <string>

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

const char* sigmaStepToken(xq::VascularSigmaStepMethod method)
{
    switch (method) {
    case xq::VascularSigmaStepMethod::Logarithmic: return "logarithmic";
    case xq::VascularSigmaStepMethod::Equispaced: return "equispaced";
    }
    return "unknown";
}

void printUsage(const char* executable)
{
    std::fprintf(stderr, "Usage: %s <dicom-directory> [series-uid]\n",
                 executable == nullptr
                     ? "xq_vascular_preprocess_probe"
                     : executable);
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
                    geometry.direction[row][0], geometry.direction[row][1],
                    geometry.direction[row][2]);
    }
}

bool sameGeometry(const xq::ImageGeometry& left, const xq::ImageGeometry& right)
{
    if (left.coordinateSystem != right.coordinateSystem) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (left.dimensions[axis] != right.dimensions[axis]
            || left.spacing[axis] != right.spacing[axis]
            || left.origin[axis] != right.origin[axis]) {
            return false;
        }
        for (int column = 0; column < 3; ++column) {
            if (left.direction[axis][column] != right.direction[axis][column]) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3 || argv == nullptr || argv[1] == nullptr) {
        printUsage(argc > 0 && argv != nullptr ? argv[0] : nullptr);
        return 1;
    }

    const std::string directory(argv[1]);
    std::string requestedUid;
    if (argc == 3 && argv[2] != nullptr) {
        requestedUid = argv[2];
    }

    xq::GdcmItkDicomSeriesReader reader;
    const xq::DicomSeriesDiscoveryResult discovery = reader.discover(directory);
    std::printf("dicom.discovery_status=%s\n",
                xq::dicomSeriesStatusToken(discovery.status));
    std::printf("dicom.series_count=%zu\n", discovery.series.size());
    if (!discovery.ok()) {
        return 2;
    }
    if (requestedUid.empty()) {
        if (discovery.series.size() != 1) {
            std::fprintf(stderr,
                         "A series UID is required when the directory does not contain exactly one series.\n");
            return 3;
        }
        requestedUid = discovery.series.front().identity.seriesInstanceUid;
    }

    const xq::DicomSeriesReadResult read = reader.read(directory, requestedUid);
    std::printf("dicom.read_status=%s\n", xq::dicomSeriesStatusToken(read.status));
    std::printf("dicom.diagnostic_count=%zu\n", read.diagnostics.size());
    for (std::size_t index = 0; index < read.diagnostics.size(); ++index) {
        std::printf("dicom.diagnostic.%zu.severity=%s\n", index,
                    severityToken(read.diagnostics[index].severity()));
        std::printf("dicom.diagnostic.%zu.code=%s\n", index,
                    read.diagnostics[index].code().c_str());
    }
    if (!read.ok()) {
        return 4;
    }

    std::printf("dicom.series_uid=%s\n",
                read.descriptor.identity.seriesInstanceUid.c_str());
    std::printf("dicom.frame_uid=%s\n",
                read.descriptor.identity.frameOfReferenceUid.c_str());
    std::printf("dicom.series_fingerprint=%s\n",
                read.descriptor.contentFingerprint.c_str());
    std::printf("dicom.input_range=%.17g,%.17g\n",
                read.volume.intensityRange().minimum,
                read.volume.intensityRange().maximum);
    printGeometry("dicom.geometry", read.volume.geometry());

    const xq::VascularPreprocessProfileV1 profile =
        xq::portalVenousCtPreprocessProfileV1();
    std::printf("profile.id=%s\n", profile.profileId.c_str());
    std::printf("profile.schema_version=%u\n", profile.schemaVersion);
    std::printf("profile.diffusion_iterations=%u\n",
                profile.diffusionIterations);
    std::printf("profile.diffusion_time_step=%.17g\n",
                profile.diffusionTimeStep);
    std::printf("profile.diffusion_conductance=%.17g\n",
                profile.diffusionConductance);
    std::printf("profile.sigma_mm=%.17g,%.17g\n",
                profile.sigmaMinimumMm, profile.sigmaMaximumMm);
    std::printf("profile.sigma_steps=%u\n", profile.sigmaSteps);
    std::printf("profile.sigma_step_method=%s\n",
                sigmaStepToken(profile.sigmaStepMethod));
    std::printf("profile.objectness=alpha:%.17g,beta:%.17g,gamma:%.17g,bright:%s,scale:%s\n",
                profile.alpha, profile.beta, profile.gamma,
                profile.brightObject ? "true" : "false",
                profile.scaleObjectness ? "true" : "false");

    xq::ResidentVoxelSource source(read.buffer);
    xq::ItkVascularPreprocessor preprocessor;
    std::printf("preprocess.begin=true\n");
    std::fflush(stdout);
    const xq::VascularPreprocessResult result =
        preprocessor.run(read.volume, source, profile);
    std::printf("preprocess.status=%s\n",
                xq::vascularPreprocessStatusToken(result.status));
    std::printf("preprocess.stage=%s\n",
                xq::vascularPreprocessStageToken(result.stage));
    std::printf("preprocess.elapsed_ms=%.17g\n", result.elapsedMilliseconds);
    std::printf("preprocess.diagnostic_count=%zu\n", result.diagnostics.size());
    for (std::size_t index = 0; index < result.diagnostics.size(); ++index) {
        const xq::VascularPreprocessDiagnostic& diagnostic =
            result.diagnostics[index];
        std::printf("preprocess.diagnostic.%zu.severity=%s\n", index,
                    severityToken(diagnostic.severity));
        std::printf("preprocess.diagnostic.%zu.code=%s\n", index,
                    diagnostic.messageKey.c_str());
        std::printf("preprocess.diagnostic.%zu.stage=%s\n", index,
                    xq::vascularPreprocessStageToken(diagnostic.stage));
        std::printf("preprocess.diagnostic.%zu.numeric_count=%zu\n", index,
                    diagnostic.numericContext.size());
        for (std::size_t value = 0; value < diagnostic.numericContext.size(); ++value) {
            std::printf("preprocess.diagnostic.%zu.numeric.%zu=%.17g\n",
                        index, value, diagnostic.numericContext[value]);
        }
    }
    if (!result.ok()) {
        return 5;
    }

    const xq::XQVesselnessVolume& output = *result.output;
    std::printf("output.algorithm_id=%s\n", output.algorithmId.c_str());
    std::printf("output.algorithm_version=%s\n",
                output.algorithmVersion.c_str());
    std::printf("output.itk_version=%s\n", output.itkVersion.c_str());
    std::printf("output.input_fingerprint=%s\n",
                output.inputFingerprint.c_str());
    std::printf("output.profile_fingerprint=%s\n",
                output.profileFingerprint.c_str());
    std::printf("output.fingerprint=%s\n", output.outputFingerprint.c_str());
    std::printf("output.input_range=%.17g,%.17g\n",
                output.inputScalarMinimum, output.inputScalarMaximum);
    std::printf("output.range=%.17g,%.17g\n",
                output.scalarMinimum, output.scalarMaximum);
    std::printf("output.positive_voxel_count=%zu\n",
                output.positiveVoxelCount);
    std::printf("output.voxel_count=%zu\n", output.buffer->voxelCount());
    printGeometry("output.geometry", output.image.geometry());
    const bool geometryEqual = sameGeometry(read.volume.geometry(),
                                            output.image.geometry());
    std::printf("output.geometry_equal_input=%s\n",
                geometryEqual ? "true" : "false");
    return geometryEqual ? 0 : 6;
}
