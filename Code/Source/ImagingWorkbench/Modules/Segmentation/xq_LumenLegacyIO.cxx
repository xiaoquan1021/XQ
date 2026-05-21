#include "xq_LumenLegacyIO.h"
#include "xq_ProfileGroup.h"
#include "xq_LumenProfile.h"
#include "xq_CircularProfile.h"
#include "xq_EllipticProfile.h"
#include "xq_PolygonalProfile.h"
#include "xq_XmlIOUtil.h"

#include <tinyxml2.h>

#include <functional>
#include <string>
#include <unordered_map>

// Factory lookup table for legacy contour types
static const std::unordered_map<std::string, std::function<xq_LumenProfile*()>> kLegacyContourFactory = {
    {"circle",  [] { return new xq_CircularProfile(); }},
    {"ellipse", [] { return new xq_EllipticProfile(); }},
    {"polygon", [] { return new xq_PolygonalProfile(); }},
};

static const std::unordered_map<size_t, std::string> kLegacyContourTypeNames = {
    {typeid(xq_CircularProfile).hash_code(),  "circle"},
    {typeid(xq_EllipticProfile).hash_code(),  "ellipse"},
    {typeid(xq_PolygonalProfile).hash_code(), "polygon"},
};

static xq_LumenProfile* CreateLegacyContour(const std::string& type)
{
    auto it = kLegacyContourFactory.find(type);
    return (it != kLegacyContourFactory.end()) ? it->second() : new xq_PolygonalProfile();
}

static std::string GetLegacyContourType(const xq_LumenProfile* contour)
{
    auto it = kLegacyContourTypeNames.find(typeid(*contour).hash_code());
    return (it != kLegacyContourTypeNames.end()) ? it->second : "polygon";
}

// ---------------------------------------------------------------------------
// IsLegacyFormat – check root element name
// ---------------------------------------------------------------------------

bool xq_LumenLegacyIO::IsLegacyFormat(std::string_view filePath)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(std::string(filePath).c_str()) != tinyxml2::XML_SUCCESS)
        return false;

    // Legacy files use "contour_group" as root; new format uses "xq_contour_group"
    return doc.FirstChildElement("contour_group") != nullptr;
}

// ---------------------------------------------------------------------------
// ReadContourGroupFile – parse legacy XML format
// ---------------------------------------------------------------------------

xq_ProfileGroup::Pointer xq_LumenLegacyIO::ReadContourGroupFile(std::string_view filePath)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(std::string(filePath).c_str()) != tinyxml2::XML_SUCCESS)
        return nullptr;

    auto* root = doc.FirstChildElement("contour_group");
    if (!root)
        return nullptr;

    xq_XmlIOUtil xmlUtil(doc);
    auto group = xq_ProfileGroup::New();

    int pathId = 0;
    root->QueryIntAttribute("path_id", &pathId);
    group->SetTrajectoryID(pathId);

    for (auto* contourElem = root->FirstChildElement("contour");
         contourElem;
         contourElem = contourElem->NextSiblingElement("contour"))
    {
        auto type = xq_XmlIOUtil::ReadStringAttribute(contourElem, "type", "polygon");

        int pathPosIndex = 0;
        contourElem->QueryIntAttribute("pos_index", &pathPosIndex);

        auto* contour = CreateLegacyContour(type);
        contour->SetPathPosIndex(pathPosIndex);

        std::vector<mitk::Point3D> controlPoints;
        for (auto* ptElem = contourElem->FirstChildElement("point");
             ptElem;
             ptElem = ptElem->NextSiblingElement("point"))
        {
            controlPoints.push_back(xmlUtil.ParsePoint3D(ptElem));
        }
        contour->SetAnchorPoints(controlPoints);

        group->AppendProfile(contour);
    }

    return group;
}

// ---------------------------------------------------------------------------
// WriteContourGroupFile – write legacy format for backward compatibility
// ---------------------------------------------------------------------------

bool xq_LumenLegacyIO::WriteContourGroupFile(
    const xq_ProfileGroup* group, std::string_view filePath)
{
    if (!group)
        return false;

    tinyxml2::XMLDocument doc;
    xq_XmlIOUtil xmlUtil(doc);

    doc.InsertFirstChild(doc.NewDeclaration());

    auto* root = doc.NewElement("contour_group");
    root->SetAttribute("path_id", group->GetTrajectoryID());
    doc.InsertEndChild(root);

    const auto pathIndices = group->GetProfilePathIndices();
    int ordinal = 0;
    for (int pathIdx : pathIndices)
    {
        auto* contour = group->GetProfileAtPathPos(pathIdx);
        if (!contour)
            continue;

        auto* contourElem = doc.NewElement("contour");
        contourElem->SetAttribute("id", ordinal++);
        contourElem->SetAttribute("type", GetLegacyContourType(contour).c_str());
        contourElem->SetAttribute("pos_index", contour->GetPathPosIndex());
        root->InsertEndChild(contourElem);

        const auto ctrlPts = contour->GetAnchorPoints();
        for (int j = 0; j < static_cast<int>(ctrlPts.size()); ++j)
        {
            auto* ptElem = xmlUtil.BuildVertexElement("point", j, ctrlPts[j]);
            contourElem->InsertEndChild(ptElem);
        }
    }

    return (doc.SaveFile(std::string(filePath).c_str()) == tinyxml2::XML_SUCCESS);
}
