#include <io/project/CTGRContourReader.h>

#include <core/GeometryTypes.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

bool close(double a, double b, double epsilon)
{
    return std::abs(a - b) <= epsilon;
}

bool close_point(const xq::Point3& a, const xq::Point3& b, double epsilon)
{
    return close(a.x, b.x, epsilon) && close(a.y, b.y, epsilon) && close(a.z, b.z, epsilon);
}

bool unit_vector(const xq::Vec3& value)
{
    return close(xq::norm(value), 1.0, 1e-6);
}

std::string ctgr_path(const std::string& name)
{
    return std::string(XQ_CTGR_DIR) + "/" + name + ".ctgr";
}

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

int validate_frame(const xq::ContourFrame& frame)
{
    if (!unit_vector(frame.normal)) {
        return fail("ctgr frame normal unit vector", __LINE__);
    }
    if (!unit_vector(frame.xAxis)) {
        return fail("ctgr frame xAxis unit vector", __LINE__);
    }
    if (!unit_vector(frame.yAxis)) {
        return fail("ctgr frame yAxis unit vector", __LINE__);
    }
    if (!close(xq::dot(frame.normal, frame.xAxis), 0.0, 1e-6)) {
        return fail("ctgr frame normal xAxis dot", __LINE__);
    }
    if (!close(xq::dot(frame.normal, frame.yAxis), 0.0, 1e-6)) {
        return fail("ctgr frame normal yAxis dot", __LINE__);
    }
    if (!close(xq::dot(frame.xAxis, frame.yAxis), 0.0, 1e-6)) {
        return fail("ctgr frame xAxis yAxis dot", __LINE__);
    }
    return 0;
}

int validate_contours(const xq::XQContourGroup& group)
{
    const std::vector<xq::XQContour>& contours = group.contours();
    if (contours.empty()) {
        return fail("ctgr contours not empty", __LINE__);
    }

    for (const xq::XQContour& contour : contours) {
        if (contour.points.empty()) {
            return fail("ctgr contour points not empty", __LINE__);
        }
        const int frameStatus = validate_frame(contour.frame);
        if (frameStatus != 0) {
            return frameStatus;
        }
    }

    const std::vector<xq::XQContour> ordered = group.orderedByPathPosition();
    if (ordered.size() != contours.size()) {
        return fail("ctgr ordered contour count", __LINE__);
    }
    for (std::size_t i = 1; i < ordered.size(); ++i) {
        if (ordered[i].pathArcLength < ordered[i - 1].pathArcLength) {
            return fail("ctgr ordered path arc length", __LINE__);
        }
    }
    return 0;
}

} // namespace

int main()
{
    xq::CTGRReadResult aorta = {};
    const xq::CTGRContourReader::Status aortaStatus =
        xq::CTGRContourReader::read(ctgr_path("aorta_final"), &aorta);
    if (aortaStatus != xq::CTGRContourReader::Status::Ok) {
        return fail("ctgr aorta read status", __LINE__);
    }
    const std::vector<xq::XQContour>& aortaContours = aorta.group.contours();
    if (aortaContours.size() < 10) {
        return fail("ctgr aorta contour count", __LINE__);
    }
    if (aorta.groupName != "aorta_final") {
        return fail("ctgr aorta group name", __LINE__);
    }
    if (aorta.sourceRelativePath != "Segmentations/aorta_final.ctgr") {
        return fail("ctgr aorta source relative path", __LINE__);
    }
    if (aortaContours.front().type != xq::ContourType::Manual) {
        return fail("ctgr aorta first contour type", __LINE__);
    }
    const int aortaValidationStatus = validate_contours(aorta.group);
    if (aortaValidationStatus != 0) {
        return aortaValidationStatus;
    }

    const xq::ContourFrame& frame = aortaContours.front().frame;
    const xq::Point3 synthetic =
        xq::add(xq::add(frame.origin, xq::scale(frame.xAxis, 3.0)), xq::scale(frame.yAxis, 4.0));
    double u = 0.0;
    double v = 0.0;
    xq::XQContourGroup::projectToFrame(frame, synthetic, &u, &v);
    if (!close(u, 3.0, 1e-9)) {
        return fail("ctgr projected u", __LINE__);
    }
    if (!close(v, 4.0, 1e-9)) {
        return fail("ctgr projected v", __LINE__);
    }
    if (!close_point(xq::XQContourGroup::unprojectFromFrame(frame, u, v), synthetic, 1e-9)) {
        return fail("ctgr unprojected point", __LINE__);
    }

    const std::vector<std::string> names = {
        "aorta_final",
        "btrunk_final",
        "carotid_final",
        "rt_carotid_final",
        "subclavian_final",
    };
    for (const std::string& name : names) {
        xq::CTGRReadResult result = {};
        const xq::CTGRContourReader::Status status =
            xq::CTGRContourReader::read(ctgr_path(name), &result);
        if (status != xq::CTGRContourReader::Status::Ok) {
            return fail("ctgr read status", __LINE__);
        }
        const std::vector<xq::XQContour>& contours = result.group.contours();
        if (contours.empty()) {
            return fail("ctgr contours not empty", __LINE__);
        }
        const int validationStatus = validate_contours(result.group);
        if (validationStatus != 0) {
            return validationStatus;
        }
        std::printf("%s contours=%zu first_points=%zu\n",
                    name.c_str(),
                    contours.size(),
                    contours.front().points.size());
    }

    xq::CTGRReadResult missing = {};
    const xq::CTGRContourReader::Status missingStatus =
        xq::CTGRContourReader::read(ctgr_path("missing_file_does_not_exist"), &missing);
    if (missingStatus != xq::CTGRContourReader::Status::FileNotFound) {
        return fail("ctgr missing file status", __LINE__);
    }

    std::fflush(stdout);
    return 0;
}
