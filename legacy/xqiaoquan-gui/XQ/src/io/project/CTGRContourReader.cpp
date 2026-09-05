#include "io/project/CTGRContourReader.h"

#include <tinyxml2.h>

#include "core/GeometryTypes.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace xq {
namespace {

const double kFrameEpsilon = 1e-12;

std::string basename_with_extension(const std::string& path)
{
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t begin = slash == std::string::npos ? 0 : slash + 1;
    return path.substr(begin);
}

std::string basename_without_extension(const std::string& path)
{
    const std::string name = basename_with_extension(path);
    const std::size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) {
        return name;
    }
    return name.substr(0, dot);
}

bool read_point3(const tinyxml2::XMLElement* element, Point3* out)
{
    if (element == 0 || out == 0) {
        return false;
    }

    return element->QueryDoubleAttribute("x", &out->x) == tinyxml2::XML_SUCCESS
        && element->QueryDoubleAttribute("y", &out->y) == tinyxml2::XML_SUCCESS
        && element->QueryDoubleAttribute("z", &out->z) == tinyxml2::XML_SUCCESS;
}

bool normalize_nonzero(Vec3 value, Vec3* out)
{
    if (out == 0) {
        return false;
    }

    const double length = norm(value);
    if (length <= kFrameEpsilon) {
        return false;
    }

    *out = scale(value, 1.0 / length);
    return true;
}

bool build_frame_from_normal_y(Point3 origin, Vec3 normalCandidate, Vec3 yCandidate, ContourFrame* frame)
{
    if (frame == 0) {
        return false;
    }

    Vec3 normal = {};
    if (!normalize_nonzero(normalCandidate, &normal)) {
        return false;
    }

    Vec3 yAxis = sub(yCandidate, scale(normal, dot(yCandidate, normal)));
    if (!normalize_nonzero(yAxis, &yAxis)) {
        return false;
    }

    Vec3 xAxis = cross(yAxis, normal);
    if (!normalize_nonzero(xAxis, &xAxis)) {
        return false;
    }

    yAxis = cross(normal, xAxis);
    if (!normalize_nonzero(yAxis, &yAxis)) {
        return false;
    }

    frame->origin = origin;
    frame->normal = normal;
    frame->xAxis = xAxis;
    frame->yAxis = yAxis;
    return true;
}

bool build_frame_from_normal_x(Point3 origin, Vec3 normalCandidate, Vec3 xCandidate, ContourFrame* frame)
{
    if (frame == 0) {
        return false;
    }

    Vec3 normal = {};
    if (!normalize_nonzero(normalCandidate, &normal)) {
        return false;
    }

    Vec3 xAxis = sub(xCandidate, scale(normal, dot(xCandidate, normal)));
    if (!normalize_nonzero(xAxis, &xAxis)) {
        return false;
    }

    Vec3 yAxis = cross(normal, xAxis);
    if (!normalize_nonzero(yAxis, &yAxis)) {
        return false;
    }

    xAxis = cross(yAxis, normal);
    if (!normalize_nonzero(xAxis, &xAxis)) {
        return false;
    }

    frame->origin = origin;
    frame->normal = normal;
    frame->xAxis = xAxis;
    frame->yAxis = yAxis;
    return true;
}

Point3 centroid(const std::vector<Point3>& points)
{
    Vec3 sum = {0.0, 0.0, 0.0};
    for (const Point3& point : points) {
        sum = add(sum, point);
    }

    const double count = static_cast<double>(points.size());
    return scale(sum, 1.0 / count);
}

bool read_frame_from_path_point(const tinyxml2::XMLElement* pathPointElement, ContourFrame* frame)
{
    if (pathPointElement == 0 || frame == 0) {
        return false;
    }

    Point3 origin = {};
    Vec3 tangent = {};
    Vec3 rotation = {};
    if (!read_point3(pathPointElement->FirstChildElement("pos"), &origin)
        || !read_point3(pathPointElement->FirstChildElement("tangent"), &tangent)
        || !read_point3(pathPointElement->FirstChildElement("rotation"), &rotation)) {
        return false;
    }

    return build_frame_from_normal_y(origin, tangent, rotation, frame);
}

bool read_frame_from_points(const std::vector<Point3>& points, ContourFrame* frame)
{
    if (points.size() < 3 || frame == 0) {
        return false;
    }

    const Point3 origin = centroid(points);
    const Vec3 normalCandidate = cross(sub(points[1], points[0]), sub(points[2], points[0]));
    const Vec3 xCandidate = sub(points.front(), origin);
    return build_frame_from_normal_x(origin, normalCandidate, xCandidate, frame);
}

bool read_contour_points(const tinyxml2::XMLElement* contourElement, std::vector<Point3>* points)
{
    if (contourElement == 0 || points == 0) {
        return false;
    }

    const tinyxml2::XMLElement* contourPointsElement = contourElement->FirstChildElement("contour_points");
    if (contourPointsElement == 0) {
        return false;
    }

    points->clear();
    for (const tinyxml2::XMLElement* pointElement = contourPointsElement->FirstChildElement("point");
         pointElement != 0;
         pointElement = pointElement->NextSiblingElement("point")) {
        Point3 point = {};
        if (!read_point3(pointElement, &point)) {
            return false;
        }
        points->push_back(point);
    }

    return !points->empty();
}

ContourType contour_type_from_attribute(const char* typeAttribute)
{
    const std::string type = typeAttribute == 0 ? "" : typeAttribute;
    if (type == "Circle") {
        return ContourType::Circle;
    }
    if (type == "Ellipse") {
        return ContourType::Ellipse;
    }
    if (type == "SplinePolygon") {
        return ContourType::SplinePolygon;
    }
    if (type == "LevelSet" || type == "LevelSetResult") {
        return ContourType::LevelSetResult;
    }
    if (type == "Threshold" || type == "ThresholdResult") {
        return ContourType::ThresholdResult;
    }
    return ContourType::Manual;
}

bool read_bool_attribute_if_present(const tinyxml2::XMLElement* element, const char* name, bool* out)
{
    if (element == 0 || out == 0) {
        return false;
    }

    const tinyxml2::XMLError error = element->QueryBoolAttribute(name, out);
    return error == tinyxml2::XML_SUCCESS || error == tinyxml2::XML_NO_ATTRIBUTE;
}

bool read_contour_id(const tinyxml2::XMLElement* contourElement, std::size_t sequenceIndex, ContourId* out)
{
    if (contourElement == 0 || out == 0) {
        return false;
    }

    std::uint64_t rawId = 0;
    const tinyxml2::XMLError error = contourElement->QueryUnsigned64Attribute("id", &rawId);
    if (error == tinyxml2::XML_SUCCESS) {
        if (rawId == std::numeric_limits<std::uint64_t>::max()) {
            return false;
        }
        *out = ContourId(static_cast<NodeId::ValueType>(rawId + 1));
        return true;
    }
    if (error != tinyxml2::XML_NO_ATTRIBUTE) {
        return false;
    }

    *out = ContourId(static_cast<NodeId::ValueType>(sequenceIndex + 1));
    return true;
}

bool read_path_arc_length(const tinyxml2::XMLElement* pathPointElement,
                          std::size_t sequenceIndex,
                          double* pathArcLength)
{
    if (pathArcLength == 0) {
        return false;
    }

    if (pathPointElement == 0) {
        *pathArcLength = static_cast<double>(sequenceIndex);
        return true;
    }

    const char* arcLengthNames[] = {
        "arclength",
        "arc_length",
    };
    for (const char* name : arcLengthNames) {
        double value = 0.0;
        const tinyxml2::XMLError error = pathPointElement->QueryDoubleAttribute(name, &value);
        if (error == tinyxml2::XML_SUCCESS) {
            *pathArcLength = value;
            return true;
        }
        if (error != tinyxml2::XML_NO_ATTRIBUTE) {
            return false;
        }
    }

    double pathPointId = 0.0;
    const tinyxml2::XMLError error = pathPointElement->QueryDoubleAttribute("id", &pathPointId);
    if (error == tinyxml2::XML_SUCCESS) {
        *pathArcLength = pathPointId;
        return true;
    }
    if (error != tinyxml2::XML_NO_ATTRIBUTE) {
        return false;
    }

    *pathArcLength = static_cast<double>(sequenceIndex);
    return true;
}

bool read_source_path_node(const tinyxml2::XMLElement* contourGroupElement, XQContourGroup* group)
{
    if (contourGroupElement == 0 || group == 0) {
        return false;
    }

    std::uint64_t pathId = 0;
    const tinyxml2::XMLError error = contourGroupElement->QueryUnsigned64Attribute("path_id", &pathId);
    if (error == tinyxml2::XML_SUCCESS) {
        group->setSourcePathNode(NodeId(static_cast<NodeId::ValueType>(pathId)));
        return true;
    }

    return error == tinyxml2::XML_NO_ATTRIBUTE;
}

bool read_contour(const tinyxml2::XMLElement* contourElement, std::size_t sequenceIndex, XQContour* contour)
{
    if (contourElement == 0 || contour == 0) {
        return false;
    }

    XQContour parsed = {};
    if (!read_contour_id(contourElement, sequenceIndex, &parsed.contourId)) {
        return false;
    }

    bool closed = false;
    if (!read_bool_attribute_if_present(contourElement, "closed", &closed)) {
        return false;
    }
    parsed.closed = closed;

    parsed.type = contour_type_from_attribute(contourElement->Attribute("type"));

    if (!read_contour_points(contourElement, &parsed.points)) {
        return false;
    }

    const tinyxml2::XMLElement* pathPointElement = contourElement->FirstChildElement("path_point");
    if (!read_path_arc_length(pathPointElement, sequenceIndex, &parsed.pathArcLength)) {
        return false;
    }

    if (pathPointElement != 0) {
        // Some legacy files keep a path_point with non-normalizable frame vectors.
        if (!read_frame_from_path_point(pathPointElement, &parsed.frame)
            && !read_frame_from_points(parsed.points, &parsed.frame)) {
            return false;
        }
    } else if (!read_frame_from_points(parsed.points, &parsed.frame)) {
        return false;
    }

    *contour = parsed;
    return true;
}

} // namespace

CTGRContourReader::Status CTGRContourReader::read(const std::string& ctgrFilePath, CTGRReadResult* out)
{
    if (out == 0) {
        return Status::ParseError;
    }

    std::ifstream input(ctgrFilePath.c_str());
    if (!input.good()) {
        return Status::FileNotFound;
    }
    input.close();

    tinyxml2::XMLDocument document;
    if (document.LoadFile(ctgrFilePath.c_str()) != tinyxml2::XML_SUCCESS) {
        return Status::ParseError;
    }

    const tinyxml2::XMLElement* contourGroupElement = document.FirstChildElement("contourgroup");
    if (contourGroupElement == 0) {
        return Status::ParseError;
    }

    CTGRReadResult result = {};
    const char* groupName = contourGroupElement->Attribute("path_name");
    result.groupName = groupName == 0 ? basename_without_extension(ctgrFilePath) : groupName;
    result.sourceRelativePath = "Segmentations/" + basename_with_extension(ctgrFilePath);
    if (!read_source_path_node(contourGroupElement, &result.group)) {
        return Status::ParseError;
    }

    bool sawTimestep = false;
    std::size_t contourIndex = 0;
    for (const tinyxml2::XMLElement* timestepElement = contourGroupElement->FirstChildElement("timestep");
         timestepElement != 0;
         timestepElement = timestepElement->NextSiblingElement("timestep")) {
        sawTimestep = true;
        for (const tinyxml2::XMLElement* contourElement = timestepElement->FirstChildElement("contour");
             contourElement != 0;
             contourElement = contourElement->NextSiblingElement("contour")) {
            XQContour contour = {};
            if (!read_contour(contourElement, contourIndex, &contour)) {
                return Status::ParseError;
            }
            result.group.addContour(contour);
            ++contourIndex;
        }
    }

    if (!sawTimestep) {
        return Status::ParseError;
    }
    if (result.group.contours().empty()) {
        return Status::EmptyGroup;
    }

    *out = result;
    return Status::Ok;
}

} // namespace xq
