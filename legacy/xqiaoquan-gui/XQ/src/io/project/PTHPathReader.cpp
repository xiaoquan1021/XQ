#include "io/project/PTHPathReader.h"

#include <tinyxml2.h>

#include "core/GeometryTypes.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

namespace xq {
namespace {

struct OrderedControlPoint {
    int id;
    PathControlPoint point;
};

struct OrderedSamplePoint {
    int id;
    PathSamplePoint sample;
};

std::string basename_without_extension(const std::string& path)
{
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t begin = slash == std::string::npos ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    const std::size_t end = (dot == std::string::npos || dot < begin) ? path.size() : dot;
    return path.substr(begin, end - begin);
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

bool read_control_points(const tinyxml2::XMLElement* pathElement,
                         std::vector<PathControlPoint>* controlPoints)
{
    if (pathElement == 0 || controlPoints == 0) {
        return false;
    }

    const tinyxml2::XMLElement* controlPointsElement = pathElement->FirstChildElement("control_points");
    if (controlPointsElement == 0) {
        return false;
    }

    std::vector<OrderedControlPoint> ordered;
    for (const tinyxml2::XMLElement* pointElement = controlPointsElement->FirstChildElement("point");
         pointElement != 0;
         pointElement = pointElement->NextSiblingElement("point")) {
        OrderedControlPoint item = {};
        if (pointElement->QueryIntAttribute("id", &item.id) != tinyxml2::XML_SUCCESS) {
            return false;
        }
        if (!read_point3(pointElement, &item.point.position)) {
            return false;
        }
        ordered.push_back(item);
    }

    std::stable_sort(ordered.begin(),
                     ordered.end(),
                     [](const OrderedControlPoint& left, const OrderedControlPoint& right) {
                         return left.id < right.id;
                     });

    controlPoints->clear();
    controlPoints->reserve(ordered.size());
    for (const OrderedControlPoint& item : ordered) {
        controlPoints->push_back(item.point);
    }

    return true;
}

bool read_raw_sample_points(const tinyxml2::XMLElement* pathElement,
                            std::vector<PathSamplePoint>* rawSamplePoints)
{
    if (pathElement == 0 || rawSamplePoints == 0) {
        return false;
    }

    const tinyxml2::XMLElement* pathPointsElement = pathElement->FirstChildElement("path_points");
    if (pathPointsElement == 0) {
        return false;
    }

    std::vector<OrderedSamplePoint> ordered;
    for (const tinyxml2::XMLElement* pathPointElement = pathPointsElement->FirstChildElement("path_point");
         pathPointElement != 0;
         pathPointElement = pathPointElement->NextSiblingElement("path_point")) {
        OrderedSamplePoint item = {};
        if (pathPointElement->QueryIntAttribute("id", &item.id) != tinyxml2::XML_SUCCESS) {
            return false;
        }

        const tinyxml2::XMLElement* posElement = pathPointElement->FirstChildElement("pos");
        const tinyxml2::XMLElement* tangentElement = pathPointElement->FirstChildElement("tangent");
        const tinyxml2::XMLElement* rotationElement = pathPointElement->FirstChildElement("rotation");

        Vec3 tangent = {};
        Vec3 normal = {};
        if (!read_point3(posElement, &item.sample.position)
            || !read_point3(tangentElement, &tangent)
            || !read_point3(rotationElement, &normal)) {
            return false;
        }

        item.sample.tangent = normalized(tangent);
        item.sample.normal = normalized(normal);
        item.sample.normal = normalized(
            sub(item.sample.normal, scale(item.sample.tangent, dot(item.sample.normal, item.sample.tangent))));
        item.sample.binormal = normalized(cross(item.sample.tangent, item.sample.normal));
        ordered.push_back(item);
    }

    std::stable_sort(ordered.begin(),
                     ordered.end(),
                     [](const OrderedSamplePoint& left, const OrderedSamplePoint& right) {
                         return left.id < right.id;
                     });

    rawSamplePoints->clear();
    rawSamplePoints->reserve(ordered.size());

    double arcLength = 0.0;
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        if (i > 0) {
            arcLength += distance(ordered[i - 1].sample.position, ordered[i].sample.position);
        }
        ordered[i].sample.arcLength = arcLength;
        rawSamplePoints->push_back(ordered[i].sample);
    }

    return true;
}

double estimated_sample_spacing(const std::vector<PathSamplePoint>& rawSamplePoints)
{
    if (rawSamplePoints.size() < 2) {
        return 0.0;
    }

    const double totalLength = rawSamplePoints.back().arcLength;
    if (totalLength <= 0.0) {
        return 0.0;
    }

    return totalLength / static_cast<double>(rawSamplePoints.size() - 1);
}

const tinyxml2::XMLElement* first_path_element(const tinyxml2::XMLDocument& document)
{
    const tinyxml2::XMLElement* pathElement = document.FirstChildElement("path");
    if (pathElement == 0) {
        return 0;
    }

    const tinyxml2::XMLElement* timestepElement = pathElement->FirstChildElement("timestep");
    if (timestepElement == 0) {
        return 0;
    }

    return timestepElement->FirstChildElement("path_element");
}

} // namespace

PTHPathReader::Status PTHPathReader::read(const std::string& pthFilePath, PTHReadResult* out)
{
    if (out == 0) {
        return Status::ParseError;
    }

    std::ifstream input(pthFilePath.c_str());
    if (!input.good()) {
        return Status::FileNotFound;
    }
    input.close();

    tinyxml2::XMLDocument document;
    if (document.LoadFile(pthFilePath.c_str()) != tinyxml2::XML_SUCCESS) {
        return Status::ParseError;
    }

    const tinyxml2::XMLElement* pathElement = first_path_element(document);
    if (pathElement == 0) {
        return Status::ParseError;
    }

    std::vector<PathControlPoint> controlPoints;
    std::vector<PathSamplePoint> rawSamplePoints;
    if (!read_control_points(pathElement, &controlPoints)
        || !read_raw_sample_points(pathElement, &rawSamplePoints)) {
        return Status::ParseError;
    }

    if (controlPoints.empty() || rawSamplePoints.empty()) {
        return Status::EmptyPath;
    }

    PTHReadResult result = {};
    result.pathName = basename_without_extension(pthFilePath);
    result.sourceRelativePath = "Paths/" + result.pathName + ".pth";
    result.rawSamplePoints = rawSamplePoints;
    result.path.setControlPoints(controlPoints);
    result.path.setInterpolation(PathInterpolation::Polyline);

    const double sampleSpacing = estimated_sample_spacing(result.rawSamplePoints);
    if (result.path.resample(sampleSpacing) != XQPath::ResampleStatus::Ok) {
        return Status::EmptyPath;
    }

    *out = result;
    return Status::Ok;
}

} // namespace xq
