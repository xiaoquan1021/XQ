#include <core/NodeId.h>
#include <core/XQContourGroup.h>
#include <core/XQPath.h>
#include <core/XQVesselProfile.h>
#include <services/profile/VesselProfileAssembler.h>

#include <cmath>
#include <array>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

const xq::NodeId kPathNode(101);
const xq::NodeId kContourNode(102);

bool near(double left, double right, double tolerance = 1.0e-10)
{
    return std::abs(left - right) <= tolerance;
}

xq::XQPath make_path()
{
    xq::XQPath path;
    path.setId(kPathNode);
    path.setControlPoints({
        {{0.0, 0.0, 0.0}},
        {{0.0, 0.0, 30.0}},
    });
    path.resample(1.0);
    return path;
}

xq::XQContour make_square(const xq::XQPath& path,
                          xq::ContourId contourId,
                          double arcLength,
                          double halfWidth)
{
    xq::PathFrame pathFrame = {};
    path.frameAtArcLength(arcLength, &pathFrame);

    xq::XQContour contour;
    contour.contourId = contourId;
    contour.pathArcLength = arcLength;
    contour.frame.origin = pathFrame.position;
    contour.frame.normal = pathFrame.tangent;
    contour.frame.xAxis = pathFrame.normal;
    contour.frame.yAxis = pathFrame.binormal;
    contour.type = xq::ContourType::SplinePolygon;
    contour.closed = true;
    contour.points.push_back(xq::XQContourGroup::unprojectFromFrame(
        contour.frame, -halfWidth, -halfWidth));
    contour.points.push_back(xq::XQContourGroup::unprojectFromFrame(
        contour.frame, halfWidth, -halfWidth));
    contour.points.push_back(xq::XQContourGroup::unprojectFromFrame(
        contour.frame, halfWidth, halfWidth));
    contour.points.push_back(xq::XQContourGroup::unprojectFromFrame(
        contour.frame, -halfWidth, halfWidth));
    return contour;
}

xq::VesselProfileAssembler::Input make_input()
{
    xq::VesselProfileAssembler::Input input;
    input.pathSource.nodeId = kPathNode;
    input.pathSource.contentRevision = 7;
    input.path = make_path();
    input.contourSource.nodeId = kContourNode;
    input.contourSource.contentRevision = 11;
    input.contourGroup.setId(kContourNode);
    input.contourGroup.setSourcePathNode(kPathNode);

    // Deliberately add out of order; output must be ordered by arc length while
    // retaining the contour-derived stable sample ids.
    xq::XQContour reversed =
        make_square(input.path, xq::ContourId(303), 25.0, 3.0);
    reversed.frame.normal = xq::scale(reversed.frame.normal, -1.0);
    reversed.frame.yAxis = xq::scale(reversed.frame.yAxis, -1.0);
    input.contourGroup.addContour(reversed);
    xq::XQContour first =
        make_square(input.path, xq::ContourId(301), 5.0, 1.0);
    for (xq::Point3& point : first.points) {
        point = xq::add(point, xq::scale(first.frame.xAxis, 4.0));
    }
    first.points.push_back(first.points.front()); // accepted closure duplicate
    input.contourGroup.addContour(first);
    input.contourGroup.addContour(
        make_square(input.path, xq::ContourId(302), 15.0, 2.0));
    input.frameOfReferenceId = "1.2.840.shell.profile.frame";
    return input;
}

int expect_issue(const xq::VesselProfileAssembler::Input& input,
                 xq::VesselProfileAssembler::IssueCode code)
{
    const xq::VesselProfileAssembler::Result result =
        xq::VesselProfileAssembler::assemble(input);
    if (result.ok() || result.profile.has_value() || result.issues.empty()
        || result.issues.front().code != code) {
        return fail("strict assembly failure issue", __LINE__);
    }
    return 0;
}

void replace_first_contour_points(
    xq::VesselProfileAssembler::Input* input,
    const std::vector<std::array<double, 2>>& points)
{
    std::vector<xq::XQContour> contours = input->contourGroup.contours();
    contours[0].points.clear();
    for (const std::array<double, 2>& point : points) {
        contours[0].points.push_back(xq::XQContourGroup::unprojectFromFrame(
            contours[0].frame, point[0], point[1]));
    }
    xq::XQContourGroup changed;
    changed.setId(kContourNode);
    changed.setSourcePathNode(kPathNode);
    for (const xq::XQContour& contour : contours) {
        changed.addContour(contour);
    }
    input->contourGroup = changed;
}

int test_valid_deterministic_profile()
{
    const xq::VesselProfileAssembler::Input input = make_input();
    const xq::VesselProfileAssembler::Result first =
        xq::VesselProfileAssembler::assemble(input);
    const xq::VesselProfileAssembler::Result second =
        xq::VesselProfileAssembler::assemble(input);
    CHECK(first.ok());
    CHECK(second.ok());
    CHECK(xq::VesselProfileValidator::validate(first.profile.value()).ok());

    const xq::VesselProfileV1& profile = first.profile.value();
    const xq::VesselProfileV1& repeated = second.profile.value();
    CHECK(profile.coordinateSystem == xq::VesselProfileCoordinateSystem::LPS);
    CHECK(profile.lengthUnit == xq::VesselProfileLengthUnit::Millimeter);
    CHECK(profile.areaUnit == xq::VesselProfileAreaUnit::SquareMillimeter);
    CHECK(profile.frameOfReferenceId == input.frameOfReferenceId);
    CHECK(profile.sourcePathNode == kPathNode);
    CHECK(profile.sourceEvidenceNodes.size() == 1);
    CHECK(profile.sourceEvidenceNodes[0] == kContourNode);
    CHECK(profile.derivationStamp.algorithmId
          == xq::VesselProfileAssembler::algorithmId());
    CHECK(profile.derivationStamp.algorithmVersion
          == xq::VesselProfileAssembler::algorithmVersion());
    CHECK(profile.derivationStamp.inputs.size() == 2);
    CHECK(profile.derivationStamp.inputs[0].nodeId == kPathNode);
    CHECK(profile.derivationStamp.inputs[0].contentRevision == 7);
    CHECK(profile.derivationStamp.inputs[1].nodeId == kContourNode);
    CHECK(profile.derivationStamp.inputs[1].contentRevision == 11);
    CHECK(profile.samples.size() == 3);

    const unsigned long long expectedIds[] = {301, 302, 303};
    const double expectedArcs[] = {5.0, 15.0, 25.0};
    const double expectedAreas[] = {4.0, 16.0, 36.0};
    for (std::size_t i = 0; i < profile.samples.size(); ++i) {
        const xq::VesselProfileSample& sample = profile.samples[i];
        const xq::VesselProfileSample& repeatedSample = repeated.samples[i];
        CHECK(sample.sampleId.value() == expectedIds[i]);
        CHECK(near(sample.arcLengthMm, expectedArcs[i]));
        CHECK(near(sample.positionMm.x, 0.0));
        CHECK(near(sample.positionMm.y, 0.0));
        CHECK(near(sample.positionMm.z, expectedArcs[i]));
        CHECK(near(sample.unitTangent.x, 0.0));
        CHECK(near(sample.unitTangent.y, 0.0));
        CHECK(near(sample.unitTangent.z, 1.0));
        CHECK(near(sample.areaMm2, expectedAreas[i]));
        CHECK(sample.evidenceKind == xq::VesselEvidenceKind::MeasuredContour);
        CHECK(sample.quality == xq::VesselSampleQuality::Accepted);
        CHECK(sample.sourceEvidenceNode == kContourNode);
        CHECK(repeatedSample.sampleId == sample.sampleId);
        CHECK(near(repeatedSample.arcLengthMm, sample.arcLengthMm));
        CHECK(near(repeatedSample.areaMm2, sample.areaMm2));
    }
    CHECK(repeated.derivationStamp.parameterSummary
          == profile.derivationStamp.parameterSummary);

    // Reordering the evidence vector cannot renumber samples or alter geometry.
    xq::VesselProfileAssembler::Input reordered = input;
    const std::vector<xq::XQContour> contours = reordered.contourGroup.contours();
    xq::XQContourGroup group;
    group.setId(kContourNode);
    group.setSourcePathNode(kPathNode);
    group.addContour(contours[1]);
    group.addContour(contours[2]);
    group.addContour(contours[0]);
    reordered.contourGroup = group;
    const xq::VesselProfileAssembler::Result third =
        xq::VesselProfileAssembler::assemble(reordered);
    CHECK(third.ok());
    for (std::size_t i = 0; i < profile.samples.size(); ++i) {
        CHECK(third.profile->samples[i].sampleId == profile.samples[i].sampleId);
        CHECK(near(third.profile->samples[i].arcLengthMm,
                   profile.samples[i].arcLengthMm));
        CHECK(near(third.profile->samples[i].areaMm2,
                   profile.samples[i].areaMm2));
    }
    CHECK(third.profile->derivationStamp.parameterSummary
          == profile.derivationStamp.parameterSummary);
    return 0;
}

int test_source_path_and_option_failures()
{
    {
        xq::VesselProfileAssembler::Input input = make_input();
        input.pathSource.nodeId = xq::NodeId::invalid();
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::InvalidPathSource)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        input.pathSource.assetId = xq::AssetId::invalid();
        input.pathSource.assetFingerprint = "sha256:invalid-asset-id";
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::InvalidPathSource)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        input.path.setId(xq::NodeId(999));
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::PathIdentityMismatch)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        input.contourGroup.setId(xq::NodeId(999));
        CHECK(expect_issue(
                  input,
                  xq::VesselProfileAssembler::IssueCode::ContourGroupIdentityMismatch)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        input.frameOfReferenceId.clear();
        CHECK(expect_issue(
                  input,
                  xq::VesselProfileAssembler::IssueCode::MissingFrameOfReference)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        input.path = xq::XQPath();
        input.path.setId(kPathNode);
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::PathNotResampled)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        xq::XQPath invalidSpacing;
        invalidSpacing.setId(kPathNode);
        invalidSpacing.setControlPoints({
            {{0.0, 0.0, 0.0}},
            {{0.0, 0.0, 30.0}},
        });
        invalidSpacing.resample(std::numeric_limits<double>::quiet_NaN());
        input.path = invalidSpacing;
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::PathNotResampled)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        input.contourGroup.setSourcePathNode(xq::NodeId(999));
        CHECK(expect_issue(
                  input,
                  xq::VesselProfileAssembler::IssueCode::ContourSourcePathMismatch)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Options options;
        options.minimumAreaMm2 = 0.0;
        const xq::VesselProfileAssembler::Result result =
            xq::VesselProfileAssembler::assemble(make_input(), options);
        CHECK(!result.ok());
        CHECK(!result.profile.has_value());
        CHECK(result.status == xq::VesselProfileAssembler::Status::InvalidOptions);
        CHECK(result.issues.size() == 1);
        CHECK(result.issues[0].code
              == xq::VesselProfileAssembler::IssueCode::InvalidTolerance);
    }
    return 0;
}

int test_contour_error_matrix()
{
    {
        xq::VesselProfileAssembler::Input input = make_input();
        xq::XQContourGroup group;
        group.setId(kContourNode);
        group.setSourcePathNode(kPathNode);
        group.addContour(input.contourGroup.contours()[0]);
        group.addContour(input.contourGroup.contours()[1]);
        input.contourGroup = group;
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::TooFewContours)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        input.contourGroup.contours();
        xq::XQContourGroup group = input.contourGroup;
        std::vector<xq::XQContour> contours = group.contours();
        contours[0].contourId = xq::ContourId::invalid();
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::InvalidContourId)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        contours[1].contourId = contours[0].contourId;
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::DuplicateContourId)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        const xq::ContourFrame frame = contours[0].frame;
        contours[0].points = {
            xq::XQContourGroup::unprojectFromFrame(frame, -2.0, 0.0),
            xq::XQContourGroup::unprojectFromFrame(frame, 2.0, 0.0),
            xq::XQContourGroup::unprojectFromFrame(frame, 0.0, 0.0),
            xq::XQContourGroup::unprojectFromFrame(frame, 2.0, 2.0),
            xq::XQContourGroup::unprojectFromFrame(frame, -2.0, 2.0),
        };
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(
                  input,
                  xq::VesselProfileAssembler::IssueCode::SelfIntersectingContour)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        contours[0].pathArcLength = std::numeric_limits<double>::quiet_NaN();
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::NonFiniteArcLength)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        contours[0].pathArcLength = 31.0;
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::ArcLengthOutOfRange)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        contours[2].pathArcLength = contours[1].pathArcLength;
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::DuplicateArcLength)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        contours[0].closed = false;
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::OpenContour)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        contours[0].frame.yAxis = contours[0].frame.xAxis;
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::InvalidContourFrame)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        contours[0].frame.origin.x += 1.0;
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(
                  input,
                  xq::VesselProfileAssembler::IssueCode::ContourFrameOriginMismatch)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        contours[0].frame.normal = {1.0, 0.0, 0.0};
        contours[0].frame.xAxis = {0.0, 1.0, 0.0};
        contours[0].frame.yAxis = {0.0, 0.0, 1.0};
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(
                  input,
                  xq::VesselProfileAssembler::IssueCode::ContourFrameNormalMismatch)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        contours[0].points[0] = xq::add(
            contours[0].points[0], xq::scale(contours[0].frame.normal, 0.1));
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::ContourPointOffPlane)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        contours[0].points[1] = contours[0].points[0];
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(
                  input,
                  xq::VesselProfileAssembler::IssueCode::DuplicateContourPoint)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        const std::vector<xq::Point3> square = contours[0].points;
        contours[0].points = {square[0], square[2], square[1], square[3]};
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(
                  input,
                  xq::VesselProfileAssembler::IssueCode::SelfIntersectingContour)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        const xq::ContourFrame frame = contours[0].frame;
        contours[0].points = {
            xq::XQContourGroup::unprojectFromFrame(frame, 0.0, 0.0),
            xq::XQContourGroup::unprojectFromFrame(frame, 1.0, 0.0),
            xq::XQContourGroup::unprojectFromFrame(frame, 2.0, 0.0),
        };
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(input,
                           xq::VesselProfileAssembler::IssueCode::DegenerateContour)
              == 0);
    }
    {
        xq::VesselProfileAssembler::Input input = make_input();
        std::vector<xq::XQContour> contours = input.contourGroup.contours();
        contours[0].points[0].x = std::numeric_limits<double>::infinity();
        xq::XQContourGroup changed;
        changed.setId(kContourNode);
        changed.setSourcePathNode(kPathNode);
        for (const xq::XQContour& contour : contours) changed.addContour(contour);
        input.contourGroup = changed;
        CHECK(expect_issue(
                  input,
                  xq::VesselProfileAssembler::IssueCode::NonFiniteContourPoint)
              == 0);
    }
    return 0;
}

int test_edge_length_normalized_intersection_tolerance()
{
    // Regression from the independent geometry review: this polygon contains a
    // real crossing hidden among several very short edges. The old unscaled
    // orientation tolerance missed it even at the default 1e-9 mm tolerance.
    {
        xq::VesselProfileAssembler::Input input = make_input();
        replace_first_contour_points(&input, {
            {{-10.0, -10.0}},
            {{-1.0, -10.0}},
            {{-1.0, 0.0}},
            {{0.99999, 0.5}},
            {{1.00001, 0.5}},
            {{1.00002, 0.5}},
            {{1.00002, 0.49998}},
            {{1.0, 0.49998}},
            {{1.0, 0.49999}},
            {{1.0, 0.50001}},
            {{-1.0, 1.0}},
            {{-1.0, 10.0}},
            {{-10.0, 10.0}},
        });
        const xq::VesselProfileAssembler::Result result =
            xq::VesselProfileAssembler::assemble(input);
        CHECK(!result.ok());
        CHECK(!result.profile.has_value());
        CHECK(!result.issues.empty());
        CHECK(result.issues.front().code
              == xq::VesselProfileAssembler::IssueCode::SelfIntersectingContour);
    }

    xq::VesselProfileAssembler::Options options;
    options.pointToleranceMm = 1.0e-4;
    options.minimumAreaMm2 = 1.0e-10;

    // A point 2*tolerance away from a very short diagonal edge is not
    // collinear. Comparing the mm^2 cross product directly with a mm tolerance
    // used to reject this otherwise-simple polygon.
    {
        xq::VesselProfileAssembler::Input input = make_input();
        replace_first_contour_points(&input, {
            {{0.0, 0.0}},
            {{0.001, 0.001}},
            {{0.001, 0.004}},
            {{0.0003585786437626905, 0.0006414213562373095}},
            {{-0.003, 0.004}},
        });
        const xq::VesselProfileAssembler::Result result =
            xq::VesselProfileAssembler::assemble(input, options);
        CHECK(result.ok());
    }

    // Conversely, a point only 0.5*tolerance away from a long edge is a
    // tolerance-level touch and must be rejected even though its raw cross
    // product is much larger than the distance tolerance.
    {
        xq::VesselProfileAssembler::Input input = make_input();
        replace_first_contour_points(&input, {
            {{0.0, 0.0}},
            {{1000.0, 0.0}},
            {{1000.0, 10.0}},
            {{500.0, 0.00005}},
            {{0.0, 10.0}},
        });
        const xq::VesselProfileAssembler::Result result =
            xq::VesselProfileAssembler::assemble(input, options);
        CHECK(!result.ok());
        CHECK(!result.profile.has_value());
        CHECK(!result.issues.empty());
        CHECK(result.issues.front().code
              == xq::VesselProfileAssembler::IssueCode::SelfIntersectingContour);
    }
    return 0;
}

} // namespace

int main()
{
    int result = test_valid_deterministic_profile();
    if (result != 0) return result;
    result = test_source_path_and_option_failures();
    if (result != 0) return result;
    result = test_contour_error_matrix();
    if (result != 0) return result;
    result = test_edge_length_normalized_intersection_tolerance();
    if (result != 0) return result;
    std::printf("OK: deterministic fail-closed vessel profile assembler\n");
    return 0;
}
