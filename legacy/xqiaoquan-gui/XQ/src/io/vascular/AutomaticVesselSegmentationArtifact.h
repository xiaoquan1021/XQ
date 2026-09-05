#ifndef XQ_IO_VASCULAR_AUTOMATIC_VESSEL_SEGMENTATION_ARTIFACT_H
#define XQ_IO_VASCULAR_AUTOMATIC_VESSEL_SEGMENTATION_ARTIFACT_H

#include "core/image/IAutomaticVesselSegmenter.h"

#include <optional>
#include <cstdint>
#include <string>

namespace xq {

enum class AutomaticVesselSegmentationArtifactStatus {
    Ok,
    InvalidArgument,
    InvalidSegmentation,
    TargetExists,
    IoError,
    InvalidFormat,
    UnsupportedVersion,
    IntegrityError
};

const char* automaticVesselSegmentationArtifactStatusToken(
    AutomaticVesselSegmentationArtifactStatus status);

struct AutomaticVesselSegmentationArtifactWriteResult {
    AutomaticVesselSegmentationArtifactStatus status =
        AutomaticVesselSegmentationArtifactStatus::InvalidArgument;
    std::string artifactSha256;

    bool ok() const;
};

struct AutomaticVesselSegmentationArtifactReadResult {
    AutomaticVesselSegmentationArtifactStatus status =
        AutomaticVesselSegmentationArtifactStatus::InvalidArgument;
    std::uint32_t formatVersion = 0;
    std::string artifactSha256;
    std::optional<XQAutomaticVesselSegmentationV2> segmentation;
    std::optional<XQAutomaticVesselSegmentation> legacySegmentation;
    bool legacyProductionValid = false;

    bool ok() const;
    bool isLegacy() const;
};

// Writes through a sibling .part file and renames only after all bytes are
// closed. Existing targets and existing .part files are never overwritten.
AutomaticVesselSegmentationArtifactWriteResult
writeAutomaticVesselSegmentationArtifact(
    const std::string& path,
    const XQAutomaticVesselSegmentationV2& segmentation);

AutomaticVesselSegmentationArtifactReadResult
readAutomaticVesselSegmentationArtifact(const std::string& path);

} // namespace xq

#endif // XQ_IO_VASCULAR_AUTOMATIC_VESSEL_SEGMENTATION_ARTIFACT_H
