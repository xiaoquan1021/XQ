#ifndef XQ_CORE_XQ_VESSEL_PROFILE_H
#define XQ_CORE_XQ_VESSEL_PROFILE_H

#include "core/GeometryTypes.h"
#include "core/NodeId.h"
#include "core/XQDerivationStamp.h"

#include <cstddef>
#include <string>
#include <vector>

namespace xq {

// Strong sample identity, intentionally distinct from NodeId and AssetId.
class VesselSampleId {
public:
    using ValueType = unsigned long long;

    VesselSampleId();
    explicit VesselSampleId(ValueType value);

    static VesselSampleId invalid();

    bool is_valid() const;
    ValueType value() const;

    std::string serialize() const;
    static bool parse(const std::string& text, VesselSampleId* out);
    static bool deserialize(const std::string& text, VesselSampleId* out);

    bool operator==(const VesselSampleId& other) const;
    bool operator!=(const VesselSampleId& other) const;
    bool operator<(const VesselSampleId& other) const;

private:
    ValueType value_;
};

enum class VesselProfileCoordinateSystem {
    Unknown,
    LPS
};

enum class VesselProfileLengthUnit {
    Unknown,
    Millimeter
};

enum class VesselProfileAreaUnit {
    Unknown,
    SquareMillimeter
};

enum class VesselEvidenceKind {
    Unknown,
    MeasuredContour,
    SegmentationDerived,
    ImportedGold
};

enum class VesselSampleQuality {
    Unknown,
    Accepted,
    ReviewRequired
};

struct VesselProfileSample {
    VesselSampleId sampleId;
    double arcLengthMm = 0.0;
    Point3 positionMm{0.0, 0.0, 0.0};
    Vec3 unitTangent{0.0, 0.0, 0.0};
    double areaMm2 = 0.0;
    VesselEvidenceKind evidenceKind = VesselEvidenceKind::Unknown;
    VesselSampleQuality quality = VesselSampleQuality::Unknown;

    // Required for node-derived evidence (MeasuredContour or
    // SegmentationDerived) and invalid for ImportedGold.
    NodeId sourceEvidenceNode;
};

struct VesselProfileV1 {
    static constexpr unsigned int ContractVersion = 1;

    unsigned int contractVersion = ContractVersion;
    VesselProfileCoordinateSystem coordinateSystem = VesselProfileCoordinateSystem::Unknown;
    VesselProfileLengthUnit lengthUnit = VesselProfileLengthUnit::Unknown;
    VesselProfileAreaUnit areaUnit = VesselProfileAreaUnit::Unknown;
    std::string frameOfReferenceId;

    NodeId sourcePathNode;
    std::vector<NodeId> sourceEvidenceNodes;

    // ImportedGold provenance. Both are required for ImportedGold and both must
    // be empty for MeasuredContour.
    std::string externalEvidenceId;
    std::string externalEvidenceFingerprint;

    DerivationStamp derivationStamp;
    std::vector<VesselProfileSample> samples;
};

enum class VesselProfileValidationCode {
    UnsupportedContractVersion,
    InvalidCoordinateSystem,
    InvalidLengthUnit,
    InvalidAreaUnit,
    MissingFrameOfReference,
    MissingSourcePath,
    MissingDerivationAlgorithmId,
    MissingDerivationAlgorithmVersion,
    InvalidDerivationInputNode,
    DuplicateDerivationInputNode,
    InvalidSourceEvidenceNode,
    DuplicateSourceEvidenceNode,
    TooFewSamples,
    InvalidSampleId,
    DuplicateSampleId,
    NonFiniteArcLength,
    NonIncreasingArcLength,
    NonFinitePosition,
    NonFiniteTangent,
    NonUnitTangent,
    NonFiniteArea,
    NonPositiveArea,
    InvalidEvidenceKind,
    InvalidQuality,
    MissingMeasuredEvidenceNodes,
    MissingMeasuredSampleEvidence,
    MeasuredSampleEvidenceNotDeclared,
    MissingSegmentationEvidenceNodes,
    MissingSegmentationSampleEvidence,
    SegmentationSampleEvidenceNotDeclared,
    UnexpectedExternalEvidence,
    MissingExternalEvidenceId,
    MissingExternalEvidenceFingerprint,
    UnexpectedImportedNodeEvidence,
    UnexpectedImportedSampleEvidence
};

struct VesselProfileValidationIssue {
    VesselProfileValidationCode code = VesselProfileValidationCode::UnsupportedContractVersion;
    bool hasSampleIndex = false;
    std::size_t sampleIndex = 0;
};

class VesselProfileValidationResult {
public:
    bool ok() const;
    bool hasIssue(VesselProfileValidationCode code) const;

    std::vector<VesselProfileValidationIssue> issues;
};

class VesselProfileValidator {
public:
    static VesselProfileValidationResult validate(const VesselProfileV1& profile);
};

} // namespace xq

#endif // XQ_CORE_XQ_VESSEL_PROFILE_H
