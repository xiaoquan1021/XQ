#include "core/XQVesselProfile.h"

#include <cmath>
#include <limits>
#include <set>

namespace xq {
namespace {

constexpr VesselSampleId::ValueType kInvalidSampleId = 0;
constexpr double kUnitTangentTolerance = 1e-6;

void add_profile_issue(VesselProfileValidationResult* result,
                       VesselProfileValidationCode code)
{
    VesselProfileValidationIssue issue;
    issue.code = code;
    result->issues.push_back(issue);
}

void add_sample_issue(VesselProfileValidationResult* result,
                      VesselProfileValidationCode code,
                      std::size_t sampleIndex)
{
    VesselProfileValidationIssue issue;
    issue.code = code;
    issue.hasSampleIndex = true;
    issue.sampleIndex = sampleIndex;
    result->issues.push_back(issue);
}

bool finite_point(const Point3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool valid_quality(VesselSampleQuality quality)
{
    return quality == VesselSampleQuality::Accepted
        || quality == VesselSampleQuality::ReviewRequired;
}

bool valid_evidence_kind(VesselEvidenceKind kind)
{
    return kind == VesselEvidenceKind::MeasuredContour
        || kind == VesselEvidenceKind::SegmentationDerived
        || kind == VesselEvidenceKind::ImportedGold;
}

} // namespace

VesselSampleId::VesselSampleId()
    : value_(kInvalidSampleId)
{
}

VesselSampleId::VesselSampleId(ValueType value)
    : value_(value)
{
}

VesselSampleId VesselSampleId::invalid()
{
    return VesselSampleId(kInvalidSampleId);
}

bool VesselSampleId::is_valid() const
{
    return value_ != kInvalidSampleId;
}

VesselSampleId::ValueType VesselSampleId::value() const
{
    return value_;
}

std::string VesselSampleId::serialize() const
{
    if (!is_valid()) {
        return "invalid";
    }
    return std::to_string(value_);
}

bool VesselSampleId::parse(const std::string& text, VesselSampleId* out)
{
    if (out == nullptr) {
        return false;
    }

    if (text == "invalid") {
        *out = VesselSampleId::invalid();
        return true;
    }

    if (text.empty()) {
        return false;
    }

    ValueType value = 0;
    const ValueType maxValue = std::numeric_limits<ValueType>::max();
    for (const char character : text) {
        if (character < '0' || character > '9') {
            return false;
        }
        const ValueType digit = static_cast<ValueType>(character - '0');
        if (value > (maxValue - digit) / 10) {
            return false;
        }
        value = (value * 10) + digit;
    }

    *out = VesselSampleId(value);
    return true;
}

bool VesselSampleId::deserialize(const std::string& text, VesselSampleId* out)
{
    return parse(text, out);
}

bool VesselSampleId::operator==(const VesselSampleId& other) const
{
    return value_ == other.value_;
}

bool VesselSampleId::operator!=(const VesselSampleId& other) const
{
    return !(*this == other);
}

bool VesselSampleId::operator<(const VesselSampleId& other) const
{
    return value_ < other.value_;
}

bool VesselProfileValidationResult::ok() const
{
    return issues.empty();
}

bool VesselProfileValidationResult::hasIssue(VesselProfileValidationCode code) const
{
    for (const VesselProfileValidationIssue& issue : issues) {
        if (issue.code == code) {
            return true;
        }
    }
    return false;
}

VesselProfileValidationResult VesselProfileValidator::validate(const VesselProfileV1& profile)
{
    VesselProfileValidationResult result;

    if (profile.contractVersion != VesselProfileV1::ContractVersion) {
        add_profile_issue(&result, VesselProfileValidationCode::UnsupportedContractVersion);
    }
    if (profile.coordinateSystem != VesselProfileCoordinateSystem::LPS) {
        add_profile_issue(&result, VesselProfileValidationCode::InvalidCoordinateSystem);
    }
    if (profile.lengthUnit != VesselProfileLengthUnit::Millimeter) {
        add_profile_issue(&result, VesselProfileValidationCode::InvalidLengthUnit);
    }
    if (profile.areaUnit != VesselProfileAreaUnit::SquareMillimeter) {
        add_profile_issue(&result, VesselProfileValidationCode::InvalidAreaUnit);
    }
    if (profile.frameOfReferenceId.empty()) {
        add_profile_issue(&result, VesselProfileValidationCode::MissingFrameOfReference);
    }
    if (!profile.sourcePathNode.is_valid()) {
        add_profile_issue(&result, VesselProfileValidationCode::MissingSourcePath);
    }
    if (profile.derivationStamp.algorithmId.empty()) {
        add_profile_issue(&result, VesselProfileValidationCode::MissingDerivationAlgorithmId);
    }
    if (profile.derivationStamp.algorithmVersion.empty()) {
        add_profile_issue(&result, VesselProfileValidationCode::MissingDerivationAlgorithmVersion);
    }

    std::set<NodeId> derivationInputs;
    for (const DerivationInputStamp& input : profile.derivationStamp.inputs) {
        if (!input.nodeId.is_valid()) {
            add_profile_issue(&result, VesselProfileValidationCode::InvalidDerivationInputNode);
            continue;
        }
        if (!derivationInputs.insert(input.nodeId).second) {
            add_profile_issue(&result, VesselProfileValidationCode::DuplicateDerivationInputNode);
        }
    }

    std::set<NodeId> declaredEvidence;
    for (const NodeId& evidenceNode : profile.sourceEvidenceNodes) {
        if (!evidenceNode.is_valid()) {
            add_profile_issue(&result, VesselProfileValidationCode::InvalidSourceEvidenceNode);
            continue;
        }
        if (!declaredEvidence.insert(evidenceNode).second) {
            add_profile_issue(&result, VesselProfileValidationCode::DuplicateSourceEvidenceNode);
        }
    }

    if (profile.samples.size() < 3) {
        add_profile_issue(&result, VesselProfileValidationCode::TooFewSamples);
    }

    std::set<VesselSampleId> sampleIds;
    bool hasMeasuredEvidence = false;
    bool hasSegmentationEvidence = false;
    bool hasImportedEvidence = false;
    double previousFiniteArc = 0.0;
    bool hasPreviousFiniteArc = false;

    for (std::size_t i = 0; i < profile.samples.size(); ++i) {
        const VesselProfileSample& sample = profile.samples[i];

        if (!sample.sampleId.is_valid()) {
            add_sample_issue(&result, VesselProfileValidationCode::InvalidSampleId, i);
        } else if (!sampleIds.insert(sample.sampleId).second) {
            add_sample_issue(&result, VesselProfileValidationCode::DuplicateSampleId, i);
        }

        if (!std::isfinite(sample.arcLengthMm)) {
            add_sample_issue(&result, VesselProfileValidationCode::NonFiniteArcLength, i);
        } else {
            if (hasPreviousFiniteArc && !(sample.arcLengthMm > previousFiniteArc)) {
                add_sample_issue(&result, VesselProfileValidationCode::NonIncreasingArcLength, i);
            }
            previousFiniteArc = sample.arcLengthMm;
            hasPreviousFiniteArc = true;
        }

        if (!finite_point(sample.positionMm)) {
            add_sample_issue(&result, VesselProfileValidationCode::NonFinitePosition, i);
        }

        if (!finite_point(sample.unitTangent)) {
            add_sample_issue(&result, VesselProfileValidationCode::NonFiniteTangent, i);
        } else {
            const double tangentNorm = norm(sample.unitTangent);
            if (!std::isfinite(tangentNorm)) {
                add_sample_issue(&result, VesselProfileValidationCode::NonFiniteTangent, i);
            } else if (std::abs(tangentNorm - 1.0) > kUnitTangentTolerance) {
                add_sample_issue(&result, VesselProfileValidationCode::NonUnitTangent, i);
            }
        }

        if (!std::isfinite(sample.areaMm2)) {
            add_sample_issue(&result, VesselProfileValidationCode::NonFiniteArea, i);
        } else if (!(sample.areaMm2 > 0.0)) {
            add_sample_issue(&result, VesselProfileValidationCode::NonPositiveArea, i);
        }

        if (!valid_evidence_kind(sample.evidenceKind)) {
            add_sample_issue(&result, VesselProfileValidationCode::InvalidEvidenceKind, i);
        } else if (sample.evidenceKind == VesselEvidenceKind::MeasuredContour) {
            hasMeasuredEvidence = true;
        } else if (sample.evidenceKind == VesselEvidenceKind::SegmentationDerived) {
            hasSegmentationEvidence = true;
        } else if (sample.evidenceKind == VesselEvidenceKind::ImportedGold) {
            hasImportedEvidence = true;
        }

        if (!valid_quality(sample.quality)) {
            add_sample_issue(&result, VesselProfileValidationCode::InvalidQuality, i);
        }
    }

    if (hasMeasuredEvidence) {
        if (declaredEvidence.empty()) {
            add_profile_issue(&result, VesselProfileValidationCode::MissingMeasuredEvidenceNodes);
        }
        for (std::size_t i = 0; i < profile.samples.size(); ++i) {
            const VesselProfileSample& sample = profile.samples[i];
            if (sample.evidenceKind != VesselEvidenceKind::MeasuredContour) {
                continue;
            }
            if (!sample.sourceEvidenceNode.is_valid()) {
                add_sample_issue(
                    &result, VesselProfileValidationCode::MissingMeasuredSampleEvidence, i);
            } else if (declaredEvidence.find(sample.sourceEvidenceNode) == declaredEvidence.end()) {
                add_sample_issue(
                    &result, VesselProfileValidationCode::MeasuredSampleEvidenceNotDeclared, i);
            }
        }
    }

    if (hasSegmentationEvidence) {
        if (declaredEvidence.empty()) {
            add_profile_issue(
                &result, VesselProfileValidationCode::MissingSegmentationEvidenceNodes);
        }
        for (std::size_t i = 0; i < profile.samples.size(); ++i) {
            const VesselProfileSample& sample = profile.samples[i];
            if (sample.evidenceKind != VesselEvidenceKind::SegmentationDerived) {
                continue;
            }
            if (!sample.sourceEvidenceNode.is_valid()) {
                add_sample_issue(
                    &result, VesselProfileValidationCode::MissingSegmentationSampleEvidence, i);
            } else if (declaredEvidence.find(sample.sourceEvidenceNode) == declaredEvidence.end()) {
                add_sample_issue(
                    &result,
                    VesselProfileValidationCode::SegmentationSampleEvidenceNotDeclared,
                    i);
            }
        }
    }

    if (hasImportedEvidence) {
        if (profile.externalEvidenceId.empty()) {
            add_profile_issue(&result, VesselProfileValidationCode::MissingExternalEvidenceId);
        }
        if (profile.externalEvidenceFingerprint.empty()) {
            add_profile_issue(&result, VesselProfileValidationCode::MissingExternalEvidenceFingerprint);
        }
        if (!hasMeasuredEvidence && !hasSegmentationEvidence
            && !profile.sourceEvidenceNodes.empty()) {
            add_profile_issue(&result, VesselProfileValidationCode::UnexpectedImportedNodeEvidence);
        }
        for (std::size_t i = 0; i < profile.samples.size(); ++i) {
            const VesselProfileSample& sample = profile.samples[i];
            if (sample.evidenceKind == VesselEvidenceKind::ImportedGold
                && sample.sourceEvidenceNode.is_valid()) {
                add_sample_issue(
                    &result, VesselProfileValidationCode::UnexpectedImportedSampleEvidence, i);
            }
        }
    }

    if (!hasImportedEvidence
        && (!profile.externalEvidenceId.empty()
            || !profile.externalEvidenceFingerprint.empty())) {
        add_profile_issue(&result, VesselProfileValidationCode::UnexpectedExternalEvidence);
    }

    return result;
}

} // namespace xq
