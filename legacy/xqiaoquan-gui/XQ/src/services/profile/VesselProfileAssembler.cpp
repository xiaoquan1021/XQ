#include "services/profile/VesselProfileAssembler.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <utility>

namespace xq {
namespace {

struct Point2 {
    double u = 0.0;
    double v = 0.0;
};

bool finite_point(const Point3& point)
{
    return std::isfinite(point.x)
        && std::isfinite(point.y)
        && std::isfinite(point.z);
}

bool finite_point(const Point2& point)
{
    return std::isfinite(point.u) && std::isfinite(point.v);
}

bool exact_point_equal(const Point3& left, const Point3& right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

double distance_squared(const Point2& left, const Point2& right)
{
    const double du = left.u - right.u;
    const double dv = left.v - right.v;
    return (du * du) + (dv * dv);
}

double cross_2d(const Point2& a, const Point2& b, const Point2& c)
{
    return ((b.u - a.u) * (c.v - a.v))
        - ((b.v - a.v) * (c.u - a.u));
}

bool within(double value, double left, double right, double tolerance)
{
    return value >= std::min(left, right) - tolerance
        && value <= std::max(left, right) + tolerance;
}

bool point_on_segment(const Point2& point,
                      const Point2& a,
                      const Point2& b,
                      double tolerance)
{
    const double edgeLength = std::hypot(b.u - a.u, b.v - a.v);
    const double crossTolerance = tolerance * edgeLength;
    return std::abs(cross_2d(a, b, point)) <= crossTolerance
        && within(point.u, a.u, b.u, tolerance)
        && within(point.v, a.v, b.v, tolerance);
}

int orientation_sign(double value, double tolerance)
{
    if (value > tolerance) {
        return 1;
    }
    if (value < -tolerance) {
        return -1;
    }
    return 0;
}

bool segments_intersect(const Point2& a,
                        const Point2& b,
                        const Point2& c,
                        const Point2& d,
                        double tolerance)
{
    const double abC = cross_2d(a, b, c);
    const double abD = cross_2d(a, b, d);
    const double cdA = cross_2d(c, d, a);
    const double cdB = cross_2d(c, d, b);
    // cross_2d is measured in mm^2. Scale the caller's distance tolerance
    // (mm) by the tested edge length (mm), so collinearity has the same physical
    // meaning for both short and long contour edges.
    const double abCrossTolerance =
        tolerance * std::hypot(b.u - a.u, b.v - a.v);
    const double cdCrossTolerance =
        tolerance * std::hypot(d.u - c.u, d.v - c.v);
    const int firstC = orientation_sign(abC, abCrossTolerance);
    const int firstD = orientation_sign(abD, abCrossTolerance);
    const int secondA = orientation_sign(cdA, cdCrossTolerance);
    const int secondB = orientation_sign(cdB, cdCrossTolerance);

    if (firstC != 0 && firstD != 0 && secondA != 0 && secondB != 0) {
        return firstC != firstD && secondA != secondB;
    }
    return (firstC == 0 && point_on_segment(c, a, b, tolerance))
        || (firstD == 0 && point_on_segment(d, a, b, tolerance))
        || (secondA == 0 && point_on_segment(a, c, d, tolerance))
        || (secondB == 0 && point_on_segment(b, c, d, tolerance));
}

bool adjacent_edges(std::size_t first, std::size_t second, std::size_t count)
{
    return first == second
        || ((first + 1) % count) == second
        || ((second + 1) % count) == first;
}

bool valid_unit_axis(const Vec3& axis, double tolerance)
{
    if (!finite_point(axis)) {
        return false;
    }
    const double length = norm(axis);
    return std::isfinite(length) && std::abs(length - 1.0) <= tolerance;
}

bool valid_contour_frame(const ContourFrame& frame, double tolerance)
{
    return finite_point(frame.origin)
        && valid_unit_axis(frame.normal, tolerance)
        && valid_unit_axis(frame.xAxis, tolerance)
        && valid_unit_axis(frame.yAxis, tolerance)
        && std::abs(dot(frame.normal, frame.xAxis)) <= tolerance
        && std::abs(dot(frame.normal, frame.yAxis)) <= tolerance
        && std::abs(dot(frame.xAxis, frame.yAxis)) <= tolerance
        && std::abs(std::abs(dot(normalized(cross(frame.xAxis, frame.yAxis)),
                                 frame.normal)) - 1.0) <= tolerance;
}

bool valid_path_frame(const PathFrame& frame, double tolerance)
{
    return finite_point(frame.position)
        && std::isfinite(frame.arcLength)
        && valid_unit_axis(frame.tangent, tolerance)
        && valid_unit_axis(frame.normal, tolerance)
        && valid_unit_axis(frame.binormal, tolerance)
        && std::abs(dot(frame.tangent, frame.normal)) <= tolerance
        && std::abs(dot(frame.tangent, frame.binormal)) <= tolerance
        && std::abs(dot(frame.normal, frame.binormal)) <= tolerance;
}

bool valid_options(const VesselProfileAssembler::Options& options)
{
    return std::isfinite(options.arcLengthToleranceMm)
        && options.arcLengthToleranceMm >= 0.0
        && std::isfinite(options.pointToleranceMm)
        && options.pointToleranceMm >= 0.0
        && options.pointToleranceMm
            <= std::sqrt(std::numeric_limits<double>::max())
        && std::isfinite(options.minimumAreaMm2)
        && options.minimumAreaMm2 > 0.0
        && std::isfinite(options.frameAxisTolerance)
        && options.frameAxisTolerance >= 0.0
        && options.frameAxisTolerance < 1.0
        && std::isfinite(options.frameOriginToleranceMm)
        && options.frameOriginToleranceMm >= 0.0
        && std::isfinite(options.minimumNormalTangentAlignment)
        && options.minimumNormalTangentAlignment >= 0.0
        && options.minimumNormalTangentAlignment <= 1.0
        && std::isfinite(options.pointPlaneToleranceMm)
        && options.pointPlaneToleranceMm >= 0.0;
}

bool valid_stamp(const DerivationInputStamp& stamp)
{
    if (!stamp.nodeId.is_valid()) {
        return false;
    }
    if (stamp.assetId.has_value() && !stamp.assetId->is_valid()) {
        return false;
    }
    return stamp.assetId.has_value() == !stamp.assetFingerprint.empty();
}

void add_issue(VesselProfileAssembler::Result* result,
               VesselProfileAssembler::Status status,
               VesselProfileAssembler::IssueCode code)
{
    result->status = status;
    VesselProfileAssembler::Issue issue;
    issue.code = code;
    result->issues.push_back(issue);
}

void add_contour_issue(VesselProfileAssembler::Result* result,
                       VesselProfileAssembler::IssueCode code,
                       std::size_t contourIndex,
                       ContourId contourId)
{
    result->status = VesselProfileAssembler::Status::InvalidContour;
    VesselProfileAssembler::Issue issue;
    issue.code = code;
    issue.hasContourIndex = true;
    issue.contourIndex = contourIndex;
    issue.contourId = contourId;
    result->issues.push_back(issue);
}

double normalized_arc_length(double arcLength,
                             double start,
                             double end,
                             double tolerance)
{
    if (arcLength < start && start - arcLength <= tolerance) {
        return start;
    }
    if (arcLength > end && arcLength - end <= tolerance) {
        return end;
    }
    return arcLength;
}

} // namespace

const char* VesselProfileAssembler::algorithmId()
{
    return "xq.vessel_profile.contour_assembler";
}

const char* VesselProfileAssembler::algorithmVersion()
{
    return "1.0.0";
}

std::string VesselProfileAssembler::parameterSummary(const Options& options)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::scientific << std::setprecision(17)
           << "policy=fail_closed"
           << ";arc_length_tolerance_mm=" << options.arcLengthToleranceMm
           << ";point_tolerance_mm=" << options.pointToleranceMm
           << ";minimum_area_mm2=" << options.minimumAreaMm2
           << ";frame_axis_tolerance=" << options.frameAxisTolerance
           << ";frame_origin_tolerance_mm=" << options.frameOriginToleranceMm
           << ";minimum_normal_tangent_alignment="
           << options.minimumNormalTangentAlignment
           << ";point_plane_tolerance_mm=" << options.pointPlaneToleranceMm;
    return stream.str();
}

VesselProfileAssembler::Result VesselProfileAssembler::assemble(
    const Input& input)
{
    return assemble(input, Options());
}

VesselProfileAssembler::Result VesselProfileAssembler::assemble(
    const Input& input,
    const Options& options)
{
    Result result;
    if (!valid_options(options)) {
        add_issue(&result, Status::InvalidOptions, IssueCode::InvalidTolerance);
        return result;
    }
    if (!valid_stamp(input.pathSource)) {
        add_issue(&result, Status::InvalidSources, IssueCode::InvalidPathSource);
        return result;
    }
    if (!valid_stamp(input.contourSource)) {
        add_issue(&result, Status::InvalidSources, IssueCode::InvalidContourSource);
        return result;
    }
    if (input.pathSource.nodeId == input.contourSource.nodeId) {
        add_issue(&result, Status::InvalidSources, IssueCode::DuplicateSourceNode);
        return result;
    }
    if (input.path.id() != input.pathSource.nodeId) {
        add_issue(&result, Status::InvalidSources, IssueCode::PathIdentityMismatch);
        return result;
    }
    if (input.contourGroup.id() != input.contourSource.nodeId) {
        add_issue(&result, Status::InvalidSources, IssueCode::ContourGroupIdentityMismatch);
        return result;
    }
    if (input.frameOfReferenceId.empty()) {
        add_issue(&result, Status::InvalidSources, IssueCode::MissingFrameOfReference);
        return result;
    }

    const std::vector<PathSamplePoint>& pathSamples = input.path.samplePoints();
    if (pathSamples.size() < 2 || !std::isfinite(input.path.sampleSpacing())
        || !(input.path.sampleSpacing() > 0.0)) {
        add_issue(&result, Status::InvalidPath, IssueCode::PathNotResampled);
        return result;
    }
    double previousPathArc = 0.0;
    bool hasPreviousPathArc = false;
    for (const PathSamplePoint& sample : pathSamples) {
        PathFrame frame = {
            sample.position,
            sample.tangent,
            sample.normal,
            sample.binormal,
            sample.arcLength,
        };
        if (!valid_path_frame(frame, options.frameAxisTolerance)
            || (hasPreviousPathArc && !(sample.arcLength > previousPathArc))) {
            add_issue(&result, Status::InvalidPath, IssueCode::InvalidPathFrame);
            return result;
        }
        previousPathArc = sample.arcLength;
        hasPreviousPathArc = true;
    }
    const double pathStart = pathSamples.front().arcLength;
    const double pathEnd = pathSamples.back().arcLength;

    if (!input.contourGroup.hasSourcePathNode()) {
        add_issue(
            &result, Status::InvalidContourGroup, IssueCode::MissingContourSourcePath);
        return result;
    }
    if (input.contourGroup.sourcePathNode() != input.pathSource.nodeId) {
        add_issue(
            &result, Status::InvalidContourGroup, IssueCode::ContourSourcePathMismatch);
        return result;
    }
    if (input.contourGroup.contours().size() < 3) {
        add_issue(&result, Status::InvalidContourGroup, IssueCode::TooFewContours);
        return result;
    }

    std::set<ContourId> contourIds;
    for (std::size_t i = 0; i < input.contourGroup.contours().size(); ++i) {
        const XQContour& contour = input.contourGroup.contours()[i];
        if (!contour.contourId.is_valid()) {
            add_contour_issue(&result, IssueCode::InvalidContourId, i, contour.contourId);
            return result;
        }
        if (!contourIds.insert(contour.contourId).second) {
            add_contour_issue(&result, IssueCode::DuplicateContourId, i, contour.contourId);
            return result;
        }
        if (!std::isfinite(contour.pathArcLength)) {
            add_contour_issue(
                &result, IssueCode::NonFiniteArcLength, i, contour.contourId);
            return result;
        }
    }

    const std::vector<XQContour> ordered = input.contourGroup.orderedByPathPosition();
    VesselProfileV1 profile;
    profile.coordinateSystem = VesselProfileCoordinateSystem::LPS;
    profile.lengthUnit = VesselProfileLengthUnit::Millimeter;
    profile.areaUnit = VesselProfileAreaUnit::SquareMillimeter;
    profile.frameOfReferenceId = input.frameOfReferenceId;
    profile.sourcePathNode = input.pathSource.nodeId;
    profile.sourceEvidenceNodes.push_back(input.contourSource.nodeId);
    profile.derivationStamp.algorithmId = algorithmId();
    profile.derivationStamp.algorithmVersion = algorithmVersion();
    profile.derivationStamp.parameterSummary = parameterSummary(options);
    profile.derivationStamp.inputs.push_back(input.pathSource);
    profile.derivationStamp.inputs.push_back(input.contourSource);
    profile.samples.reserve(ordered.size());

    double previousArc = 0.0;
    bool hasPreviousArc = false;
    const double pointToleranceSquared =
        options.pointToleranceMm * options.pointToleranceMm;

    for (std::size_t i = 0; i < ordered.size(); ++i) {
        const XQContour& contour = ordered[i];
        const double arcLength = normalized_arc_length(
            contour.pathArcLength,
            pathStart,
            pathEnd,
            options.arcLengthToleranceMm);
        if (contour.pathArcLength < pathStart - options.arcLengthToleranceMm
            || contour.pathArcLength > pathEnd + options.arcLengthToleranceMm) {
            add_contour_issue(
                &result, IssueCode::ArcLengthOutOfRange, i, contour.contourId);
            return result;
        }
        if (hasPreviousArc
            && !(arcLength > previousArc + options.arcLengthToleranceMm)) {
            add_contour_issue(
                &result, IssueCode::DuplicateArcLength, i, contour.contourId);
            return result;
        }
        previousArc = arcLength;
        hasPreviousArc = true;

        if (!contour.closed) {
            add_contour_issue(&result, IssueCode::OpenContour, i, contour.contourId);
            return result;
        }
        if (!valid_contour_frame(contour.frame, options.frameAxisTolerance)) {
            add_contour_issue(
                &result, IssueCode::InvalidContourFrame, i, contour.contourId);
            return result;
        }

        PathFrame pathFrame = {};
        if (input.path.frameAtArcLength(arcLength, &pathFrame)
                != XQPath::FrameStatus::Ok
            || !valid_path_frame(pathFrame, options.frameAxisTolerance)) {
            add_contour_issue(
                &result, IssueCode::InvalidContourFrame, i, contour.contourId);
            return result;
        }
        if (distance(contour.frame.origin, pathFrame.position)
            > options.frameOriginToleranceMm) {
            add_contour_issue(
                &result, IssueCode::ContourFrameOriginMismatch, i, contour.contourId);
            return result;
        }
        if (std::abs(dot(contour.frame.normal, pathFrame.tangent))
            < options.minimumNormalTangentAlignment) {
            add_contour_issue(
                &result, IssueCode::ContourFrameNormalMismatch, i, contour.contourId);
            return result;
        }

        std::vector<Point3> points = contour.points;
        if (points.size() >= 2 && exact_point_equal(points.front(), points.back())) {
            points.pop_back();
        }
        if (points.size() < 3) {
            add_contour_issue(
                &result, IssueCode::TooFewContourPoints, i, contour.contourId);
            return result;
        }

        std::vector<Point2> projected;
        projected.reserve(points.size());
        for (const Point3& point : points) {
            if (!finite_point(point)) {
                add_contour_issue(
                    &result, IssueCode::NonFiniteContourPoint, i, contour.contourId);
                return result;
            }
            const double planeDistance =
                std::abs(dot(sub(point, contour.frame.origin), contour.frame.normal));
            if (!std::isfinite(planeDistance)
                || planeDistance > options.pointPlaneToleranceMm) {
                add_contour_issue(
                    &result, IssueCode::ContourPointOffPlane, i, contour.contourId);
                return result;
            }
            Point2 point2;
            XQContourGroup::projectToFrame(
                contour.frame, point, &point2.u, &point2.v);
            if (!finite_point(point2)) {
                add_contour_issue(
                    &result, IssueCode::NonFiniteContourPoint, i, contour.contourId);
                return result;
            }
            projected.push_back(point2);
        }

        for (std::size_t left = 0; left < projected.size(); ++left) {
            for (std::size_t right = left + 1; right < projected.size(); ++right) {
                if (distance_squared(projected[left], projected[right])
                    <= pointToleranceSquared) {
                    add_contour_issue(
                        &result,
                        IssueCode::DuplicateContourPoint,
                        i,
                        contour.contourId);
                    return result;
                }
            }
        }

        const double turnTolerance =
            options.pointToleranceMm * options.pointToleranceMm;
        if (projected.size() > 3) {
            for (std::size_t current = 0; current < projected.size(); ++current) {
                const Point2& previous = projected[
                    (current + projected.size() - 1) % projected.size()];
                const Point2& point = projected[current];
                const Point2& next = projected[(current + 1) % projected.size()];
                const double turn = cross_2d(previous, point, next);
                const double incomingU = point.u - previous.u;
                const double incomingV = point.v - previous.v;
                const double outgoingU = next.u - point.u;
                const double outgoingV = next.v - point.v;
                if (std::abs(turn) <= turnTolerance
                    && ((incomingU * outgoingU) + (incomingV * outgoingV)) < 0.0) {
                    add_contour_issue(
                        &result,
                        IssueCode::SelfIntersectingContour,
                        i,
                        contour.contourId);
                    return result;
                }
            }
        }

        for (std::size_t first = 0; first < projected.size(); ++first) {
            const std::size_t firstEnd = (first + 1) % projected.size();
            for (std::size_t second = first + 1; second < projected.size(); ++second) {
                if (adjacent_edges(first, second, projected.size())) {
                    continue;
                }
                const std::size_t secondEnd = (second + 1) % projected.size();
                if (segments_intersect(
                        projected[first],
                        projected[firstEnd],
                        projected[second],
                        projected[secondEnd],
                        options.pointToleranceMm)) {
                    add_contour_issue(
                        &result,
                        IssueCode::SelfIntersectingContour,
                        i,
                        contour.contourId);
                    return result;
                }
            }
        }

        long double twiceArea = 0.0L;
        for (std::size_t pointIndex = 0; pointIndex < projected.size(); ++pointIndex) {
            const Point2& current = projected[pointIndex];
            const Point2& next = projected[(pointIndex + 1) % projected.size()];
            twiceArea += static_cast<long double>(current.u)
                    * static_cast<long double>(next.v)
                - static_cast<long double>(current.v)
                    * static_cast<long double>(next.u);
        }
        const long double areaLong = std::abs(twiceArea) * 0.5L;
        const double area = static_cast<double>(areaLong);
        if (!std::isfinite(area) || !(area > options.minimumAreaMm2)) {
            add_contour_issue(
                &result, IssueCode::DegenerateContour, i, contour.contourId);
            return result;
        }

        VesselProfileSample sample;
        sample.sampleId = VesselSampleId(contour.contourId.value());
        sample.arcLengthMm = arcLength;
        sample.positionMm = pathFrame.position;
        sample.unitTangent = pathFrame.tangent;
        sample.areaMm2 = area;
        sample.evidenceKind = VesselEvidenceKind::MeasuredContour;
        sample.quality = VesselSampleQuality::Accepted;
        sample.sourceEvidenceNode = input.contourSource.nodeId;
        profile.samples.push_back(sample);
    }

    const VesselProfileValidationResult validation =
        VesselProfileValidator::validate(profile);
    if (!validation.ok()) {
        result.status = Status::ProfileValidationFailed;
        for (const VesselProfileValidationIssue& validationIssue : validation.issues) {
            Issue issue;
            issue.code = IssueCode::ProfileValidationIssue;
            issue.hasValidationCode = true;
            issue.validationCode = validationIssue.code;
            issue.hasContourIndex = validationIssue.hasSampleIndex;
            issue.contourIndex = validationIssue.sampleIndex;
            if (validationIssue.hasSampleIndex
                && validationIssue.sampleIndex < ordered.size()) {
                issue.contourId = ordered[validationIssue.sampleIndex].contourId;
            }
            result.issues.push_back(issue);
        }
        return result;
    }

    result.status = Status::Ok;
    result.profile = std::move(profile);
    return result;
}

} // namespace xq
