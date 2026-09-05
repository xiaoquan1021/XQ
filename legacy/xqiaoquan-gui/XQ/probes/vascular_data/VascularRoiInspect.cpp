#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "adapters/itk/ItkVascularRoiPriorReader.h"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 4 || argc > 5 || argv == nullptr) {
        std::fprintf(
            stderr,
            "Usage: xq_vascular_roi_inspect <ct-dicom-directory> <organ-roi.nii.gz> <coarse-vessel-roi.nii.gz> [series-uid]\n");
        return 1;
    }

    xq::GdcmItkDicomSeriesReader imageReader;
    const xq::DicomSeriesDiscoveryResult discovery = imageReader.discover(argv[1]);
    if (!discovery.ok()) {
        return 2;
    }
    std::string seriesUid = argc == 5 ? argv[4] : std::string();
    if (seriesUid.empty()) {
        if (discovery.series.size() != 1) {
            return 3;
        }
        seriesUid = discovery.series.front().identity.seriesInstanceUid;
    }
    const xq::DicomSeriesReadResult image = imageReader.read(argv[1], seriesUid);
    if (!image.ok()) {
        return 4;
    }

    const std::vector<xq::VascularRoiFileInput> inputs = {
        {xq::VascularRoiRole::Organ, argv[2], "TotalSegmentator", "2.15.0"},
        {xq::VascularRoiRole::CoarseVessel, argv[3], "TotalSegmentator", "2.15.0"}
    };
    xq::ItkVascularRoiPriorReader reader;
    const xq::VascularRoiPriorReadResult read = reader.read(
        image.volume, image.descriptor.contentFingerprint, inputs);
    std::printf("roi.status=%s\n",
                xq::vascularRoiPriorReadStatusToken(read.status));
    std::printf("roi.diagnostic_count=%zu\n", read.diagnostics.size());
    for (std::size_t index = 0; index < read.diagnostics.size(); ++index) {
        std::printf("roi.diagnostic.%zu=%s\n", index,
                    read.diagnostics[index].code().c_str());
    }
    if (!read.ok()) {
        return 5;
    }

    std::printf("roi.ct_fingerprint=%s\n",
                read.prior->ctInputFingerprint.c_str());
    std::printf("roi.prior_fingerprint=%s\n",
                read.prior->priorFingerprint.c_str());
    for (std::size_t index = 0; index < read.prior->layers.size(); ++index) {
        const xq::XQVascularRoiLayer& layer = read.prior->layers[index];
        std::printf("roi.layer.%zu.role=%s\n", index,
                    xq::vascularRoiRoleToken(layer.role));
        std::printf("roi.layer.%zu.source_fingerprint=%s\n", index,
                    layer.sourceFingerprint.c_str());
        std::printf("roi.layer.%zu.aligned_fingerprint=%s\n", index,
                    layer.alignedFingerprint.c_str());
        std::printf("roi.layer.%zu.source_voxels=%zu\n", index,
                    layer.sourceForegroundVoxelCount);
        std::printf("roi.layer.%zu.aligned_voxels=%zu\n", index,
                    layer.alignedForegroundVoxelCount);
        std::printf("roi.layer.%zu.resampled=%s\n", index,
                    layer.resampledToReference ? "true" : "false");
        const xq::ImageGeometry& geometry = layer.mask->geometry();
        std::printf("roi.layer.%zu.dimensions=%d,%d,%d\n", index,
                    geometry.dimensions[0], geometry.dimensions[1],
                    geometry.dimensions[2]);
        std::printf("roi.layer.%zu.spacing=%.17g,%.17g,%.17g\n", index,
                    geometry.spacing[0], geometry.spacing[1], geometry.spacing[2]);
        std::printf("roi.layer.%zu.origin=%.17g,%.17g,%.17g\n", index,
                    geometry.origin[0], geometry.origin[1], geometry.origin[2]);
    }
    return 0;
}
