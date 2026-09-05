#ifndef XQ_SERVICES_PROFILE_VESSEL_PROFILE_ASSEMBLER_H
#define XQ_SERVICES_PROFILE_VESSEL_PROFILE_ASSEMBLER_H

#include "core/XQContourGroup.h"
#include "core/XQDerivationStamp.h"
#include "core/XQPath.h"
#include "core/XQVesselProfile.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace xq {

// Pure contour-evidence -> solver-profile assembly. This service owns strict
// geometry validation only; Scene freshness and project mutation remain in the
// controller/command layer.
class VesselProfileAssembler {
public:
    struct Options {
        double arcLengthToleranceMm = 1.0e-9;
        double pointToleranceMm = 1.0e-9;
        double minimumAreaMm2 = 1.0e-8;
        double frameAxisTolerance = 1.0e-6;
        double frameOriginToleranceMm = 1.0e-3;
        double minimumNormalTangentAlignment = 1.0 - 1.0e-6;
        double pointPlaneToleranceMm = 1.0e-3;
    };

    struct Input {
        DerivationInputStamp pathSource;
        XQPath path;
        DerivationInputStamp contourSource;
        XQContourGroup contourGroup;
        std::string frameOfReferenceId;
    };

    enum class Status {
        Ok,
        InvalidOptions,
        InvalidSources,
        InvalidPath,
        InvalidContourGroup,
        InvalidContour,
        ProfileValidationFailed
    };

    enum class IssueCode {
        InvalidTolerance,
        InvalidPathSource,
        InvalidContourSource,
        DuplicateSourceNode,
        PathIdentityMismatch,
        ContourGroupIdentityMismatch,
        MissingFrameOfReference,
        PathNotResampled,
        InvalidPathFrame,
        MissingContourSourcePath,
        ContourSourcePathMismatch,
        TooFewContours,
        InvalidContourId,
        DuplicateContourId,
        NonFiniteArcLength,
        ArcLengthOutOfRange,
        DuplicateArcLength,
        OpenContour,
        TooFewContourPoints,
        NonFiniteContourPoint,
        InvalidContourFrame,
        ContourFrameOriginMismatch,
        ContourFrameNormalMismatch,
        ContourPointOffPlane,
        DuplicateContourPoint,
        SelfIntersectingContour,
        DegenerateContour,
        ProfileValidationIssue
    };

    struct Issue {
        IssueCode code = IssueCode::InvalidTolerance;
        bool hasContourIndex = false;
        std::size_t contourIndex = 0;
        ContourId contourId;
        bool hasValidationCode = false;
        VesselProfileValidationCode validationCode =
            VesselProfileValidationCode::UnsupportedContractVersion;
    };

    struct Result {
        Status status = Status::InvalidSources;
        std::optional<VesselProfileV1> profile;
        std::vector<Issue> issues;

        bool ok() const
        {
            return status == Status::Ok && profile.has_value() && issues.empty();
        }
    };

    static const char* algorithmId();
    static const char* algorithmVersion();
    static std::string parameterSummary(const Options& options);

    static Result assemble(const Input& input);
    static Result assemble(const Input& input, const Options& options);
};

} // namespace xq

#endif // XQ_SERVICES_PROFILE_VESSEL_PROFILE_ASSEMBLER_H
