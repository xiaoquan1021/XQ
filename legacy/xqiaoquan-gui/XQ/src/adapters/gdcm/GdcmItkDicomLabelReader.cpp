#include "adapters/gdcm/GdcmItkDicomLabelReader.h"

#include "core/XQMemoryImageBufferHandle.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace xq {
namespace {

void addDiagnostic(std::vector<Diagnostic>* diagnostics,
                   DiagnosticSeverity severity,
                   const std::string& code,
                   const std::string& message)
{
    if (diagnostics != nullptr) {
        diagnostics->emplace_back(severity, code, message);
    }
}

bool validProfile(const DicomBinaryLabelProfile& profile)
{
    return std::isfinite(profile.backgroundValue)
        && std::isfinite(profile.foregroundValue)
        && profile.backgroundValue != profile.foregroundValue
        && profile.outputLabel != 0
        && !profile.labelName.empty();
}

} // namespace

const char* dicomLabelStatusToken(DicomLabelStatus status)
{
    switch (status) {
    case DicomLabelStatus::Ok: return "ok";
    case DicomLabelStatus::InvalidProfile: return "invalid_profile";
    case DicomLabelStatus::SourceReadFailed: return "source_read_failed";
    case DicomLabelStatus::InvalidSourceBuffer: return "invalid_source_buffer";
    case DicomLabelStatus::UnsupportedLabelValues: return "unsupported_label_values";
    case DicomLabelStatus::EmptyForeground: return "empty_foreground";
    }
    return "invalid_profile";
}

DicomLabelReadResult GdcmItkDicomLabelReader::read(
    const std::string& directory,
    const std::string& seriesInstanceUid,
    const DicomBinaryLabelProfile& profile)
{
    DicomLabelReadResult result;
    if (!validProfile(profile)) {
        addDiagnostic(&result.diagnostics,
                      DiagnosticSeverity::Error,
                      "dicom_label.invalid_profile",
                      "Binary DICOM label interpretation requires two distinct finite values, a non-zero output label, and a label name.");
        return result;
    }

    DicomSeriesReadResult source = imageReader_.read(directory, seriesInstanceUid);
    result.sourceStatus = source.status;
    result.descriptor = source.descriptor;
    result.diagnostics = source.diagnostics;
    if (!source.ok()) {
        result.status = DicomLabelStatus::SourceReadFailed;
        addDiagnostic(&result.diagnostics,
                      DiagnosticSeverity::Error,
                      "dicom_label.source_read_failed",
                      "The production DICOM reader could not decode the label series.");
        return result;
    }

    const std::vector<std::uint8_t>& bytes = source.buffer->bytes();
    const std::size_t voxelCount = source.buffer->voxelCount();
    if (source.buffer->scalarType() != ScalarType::Float32
        || source.buffer->componentCount() != 1
        || voxelCount == 0
        || voxelCount > (std::numeric_limits<std::size_t>::max)() / sizeof(float)
        || bytes.size() != voxelCount * sizeof(float)) {
        result.status = DicomLabelStatus::InvalidSourceBuffer;
        addDiagnostic(&result.diagnostics,
                      DiagnosticSeverity::Error,
                      "dicom_label.invalid_source_buffer",
                      "The decoded DICOM label buffer is not one finite float scalar per voxel.");
        return result;
    }

    const ImageGeometry& geometry = source.volume.geometry();
    std::shared_ptr<XQSegmentationMask> mask =
        std::make_shared<XQSegmentationMask>(geometry.dimensions);
    if (!mask->is_valid() || mask->voxelCount() != voxelCount) {
        result.status = DicomLabelStatus::InvalidSourceBuffer;
        addDiagnostic(&result.diagnostics,
                      DiagnosticSeverity::Error,
                      "dicom_label.invalid_mask_dimensions",
                      "The DICOM label geometry could not allocate a matching XQ mask.");
        return result;
    }
    mask->setGeometry(geometry);
    mask->setLabels(std::vector<SegmentationLabel>{
        SegmentationLabel{static_cast<int>(profile.outputLabel), profile.labelName}
    });

    for (std::size_t voxel = 0; voxel < voxelCount; ++voxel) {
        float stored = 0.0f;
        std::memcpy(&stored,
                    bytes.data() + voxel * sizeof(float),
                    sizeof(float));
        const double value = static_cast<double>(stored);
        if (value == profile.backgroundValue) {
            ++result.backgroundVoxelCount;
            continue;
        }
        if (value == profile.foregroundValue) {
            mask->setLabelAt(voxel, profile.outputLabel);
            ++result.foregroundVoxelCount;
            continue;
        }
        result.status = DicomLabelStatus::UnsupportedLabelValues;
        addDiagnostic(&result.diagnostics,
                      DiagnosticSeverity::Error,
                      "dicom_label.unsupported_values",
                      "The DICOM label series contains a scalar outside the frozen binary-label profile.");
        return result;
    }

    if (result.foregroundVoxelCount == 0) {
        result.status = DicomLabelStatus::EmptyForeground;
        addDiagnostic(&result.diagnostics,
                      DiagnosticSeverity::Error,
                      "dicom_label.empty_foreground",
                      "The DICOM label series contains no foreground voxels.");
        return result;
    }

    result.mask = std::move(mask);
    result.status = DicomLabelStatus::Ok;
    return result;
}

} // namespace xq
