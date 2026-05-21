#include "xq_LumenSegIO.h"
#include "xq_ProfileGroup.h"
#include "xq_LumenProfile.h"
#include "xq_CircularProfile.h"
#include "xq_EllipticProfile.h"
#include "xq_PolygonalProfile.h"
#include "xq_SplineProfile.h"
#include "xq_TensionProfile.h"
#include "xq_XmlIOUtil.h"

#include <mitkCustomMimeType.h>
#include <mitkIOMimeTypes.h>

#include <tinyxml2.h>

#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

static constexpr const char* kXqCtgrMimeName = "application/xq-contourgroup";
static constexpr const char* kXqCtgrExtension = "xqctgr";

// Factory lookup table: type string -> contour constructor
static const std::unordered_map<std::string, std::function<xq_LumenProfile*()>> kContourFactory = {
    {"circle",          [] { return new xq_CircularProfile(); }},
    {"ellipse",         [] { return new xq_EllipticProfile(); }},
    {"spline_polygon",  [] { return new xq_SplineProfile(); }},
    {"tension_polygon", [] { return new xq_TensionProfile(); }},
    {"polygon",         [] { return new xq_PolygonalProfile(); }},
};

// Reverse lookup table: typeid hash -> type string
static const std::unordered_map<size_t, std::string> kContourTypeNames = {
    {typeid(xq_CircularProfile).hash_code(),  "circle"},
    {typeid(xq_EllipticProfile).hash_code(),  "ellipse"},
    {typeid(xq_SplineProfile).hash_code(),    "spline_polygon"},
    {typeid(xq_TensionProfile).hash_code(),   "tension_polygon"},
    {typeid(xq_PolygonalProfile).hash_code(), "polygon"},
};

static xq_LumenProfile* CreateContourByType(const std::string& type)
{
    auto it = kContourFactory.find(type);
    return (it != kContourFactory.end()) ? it->second() : new xq_PolygonalProfile();
}

static std::string GetContourTypeString(const xq_LumenProfile* contour)
{
    auto it = kContourTypeNames.find(typeid(*contour).hash_code());
    return (it != kContourTypeNames.end()) ? it->second : "polygon";
}

bool xq_LumenSegIO::endsWith(std::string_view str, std::string_view suffix)
{
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ---------------------------------------------------------------------------
// IsVersionSupported
// ---------------------------------------------------------------------------
bool xq_LumenSegIO::IsVersionSupported(std::string_view version)
{
    return version == kFormatVersion;
}

// ---------------------------------------------------------------------------
// ParseIntSafe
// Converts s to int; returns defaultVal rather than throwing on malformed input.
// ---------------------------------------------------------------------------
int xq_LumenSegIO::ParseIntSafe(const std::string& s, int defaultVal)
{
    if (s.empty())
        return defaultVal;
    try {
        return std::stoi(s);
    } catch (const std::invalid_argument&) {
        return defaultVal;
    } catch (const std::out_of_range&) {
        return defaultVal;
    }
}

// ---------------------------------------------------------------------------
// WriteContourElements
// Iterates by path-position key, not by ordinal, so sparse groups are handled
// correctly.  A group with profiles at {0, 5, 10} produces 3 contour elements
// regardless of whether the ordinal range 0..2 contains valid positions.
// ---------------------------------------------------------------------------
int xq_LumenSegIO::WriteContourElements(const xq_ProfileGroup* group,
                                         tinyxml2::XMLElement* contoursElem,
                                         tinyxml2::XMLDocument& doc)
{
    if (!group || !contoursElem)
        return 0;

    xq_XmlIOUtil xmlUtil(doc);
    int written = 0;

    for (int pathIdx : group->GetProfilePathIndices())
    {
        auto* contour = group->GetProfileAtPathPos(pathIdx);
        if (!contour)
            continue;

        auto* contourElem = doc.NewElement("contour");
        contourElem->SetAttribute("id", written);
        contourElem->SetAttribute("type", GetContourTypeString(contour).c_str());
        contourElem->SetAttribute("path_pos_index", contour->GetPathPosIndex());

        const auto method = contour->GetMethod();
        if (!method.empty())
            contourElem->SetAttribute("method", method.c_str());

        contoursElem->InsertEndChild(contourElem);

        const auto ctrlPts = contour->GetAnchorPoints();
        auto* ctrlPtsElem = doc.NewElement("control_points");
        ctrlPtsElem->SetAttribute("count", static_cast<int>(ctrlPts.size()));
        contourElem->InsertEndChild(ctrlPtsElem);

        for (int j = 0; j < static_cast<int>(ctrlPts.size()); ++j)
        {
            auto* ptElem = xmlUtil.BuildVertexElement("point", j, ctrlPts[j]);
            ctrlPtsElem->InsertEndChild(ptElem);
        }

        ++written;
    }
    return written;
}

// ---------------------------------------------------------------------------
// Constructor – registers MimeType and service
// ---------------------------------------------------------------------------

xq_LumenSegIO::xq_LumenSegIO()
    : mitk::AbstractFileIO(xq_ProfileGroup::GetStaticNameOfClass(),
                           mitk::CustomMimeType(kXqCtgrMimeName),
                           "XQ Contour Group File")
{
    mitk::CustomMimeType mimeType(kXqCtgrMimeName);
    mimeType.SetCategory("XQ Segmentation");
    mimeType.SetComment("XQ Contour Group");
    mimeType.AddExtension(kXqCtgrExtension);
    this->SetMimeType(mimeType);

    this->SetReaderDescription("XQ Contour Group Reader");
    this->SetWriterDescription("XQ Contour Group Writer");

    RegisterService();
}

// ---------------------------------------------------------------------------
// DoRead
// ---------------------------------------------------------------------------

std::vector<mitk::BaseData::Pointer> xq_LumenSegIO::DoRead()
{
    std::vector<mitk::BaseData::Pointer> result;

    const auto filename = this->GetInputLocation();

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS)
    {
        mitkThrow() << "xq_LumenSegIO: Failed to load file: " << filename;
    }

    auto* root = doc.FirstChildElement("xq_contour_group");
    if (!root)
    {
        mitkThrow() << "xq_LumenSegIO: Missing <xq_contour_group> root element in "
                     << filename;
    }

    // Validate format version before processing
    const auto version = xq_XmlIOUtil::ReadStringAttribute(root, "version", "");
    if (!IsVersionSupported(version))
    {
        mitkThrow() << "xq_LumenSegIO: Unsupported file format version '" << version
                    << "' in " << filename
                    << " (supported: " << kFormatVersion << ")";
    }

    xq_XmlIOUtil xmlUtil(doc);
    auto group = xq_ProfileGroup::New();

    // Read path ID
    const auto pathId = xq_XmlIOUtil::ReadStringAttribute(root, "path_id");
    if (!pathId.empty())
        group->SetTrajectoryID(ParseIntSafe(pathId));

    // Read properties
    if (auto* propsElem = root->FirstChildElement("properties"))
    {
        for (auto* propElem = propsElem->FirstChildElement("property");
             propElem;
             propElem = propElem->NextSiblingElement("property"))
        {
            auto key   = xq_XmlIOUtil::ReadStringAttribute(propElem, "key");
            auto value = xq_XmlIOUtil::ReadStringAttribute(propElem, "value");
            if (!key.empty())
                group->SetAttribute(key, value);
        }
    }

    // Read contour entries
    if (auto* contoursElem = root->FirstChildElement("contours"))
    {
        for (auto* contourElem = contoursElem->FirstChildElement("contour");
             contourElem;
             contourElem = contourElem->NextSiblingElement("contour"))
        {
            auto type = xq_XmlIOUtil::ReadStringAttribute(contourElem, "type", "polygon");

            int pathPosIndex = 0;
            contourElem->QueryIntAttribute("path_pos_index", &pathPosIndex);

            auto* contour = CreateContourByType(type);
            contour->SetPathPosIndex(pathPosIndex);

            // Read method if present
            auto method = xq_XmlIOUtil::ReadStringAttribute(contourElem, "method");
            if (!method.empty())
                contour->SetMethod(method);

            // Read control points
            if (auto* ctrlPtsElem = contourElem->FirstChildElement("control_points"))
            {
                std::vector<mitk::Point3D> controlPoints;
                for (auto* ptElem = ctrlPtsElem->FirstChildElement("point");
                     ptElem;
                     ptElem = ptElem->NextSiblingElement("point"))
                {
                    controlPoints.push_back(xmlUtil.ParsePoint3D(ptElem));
                }
                contour->SetAnchorPoints(controlPoints);
            }

            group->AppendProfile(contour);
        }
    }

    result.push_back(group.GetPointer());
    return result;
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

void xq_LumenSegIO::Write()
{
    ValidateOutputLocation();

    const auto* group = dynamic_cast<const xq_ProfileGroup*>(this->GetInput());
    if (!group)
        mitkThrow() << "xq_LumenSegIO: Input is not an xq_ProfileGroup";

    tinyxml2::XMLDocument doc;
    xq_XmlIOUtil xmlUtil(doc);

    doc.InsertFirstChild(doc.NewDeclaration());

    auto* root = doc.NewElement("xq_contour_group");
    root->SetAttribute("version", "1.0");
    root->SetAttribute("path_id", group->GetTrajectoryID());
    doc.InsertEndChild(root);

    // Write properties
    const auto props = group->GetAttributes();
    if (!props.empty())
    {
        auto* propsElem = doc.NewElement("properties");
        root->InsertEndChild(propsElem);

        for (const auto& [key, value] : props)
        {
            auto* propElem = doc.NewElement("property");
            propElem->SetAttribute("key", key.c_str());
            propElem->SetAttribute("value", value.c_str());
            propsElem->InsertEndChild(propElem);
        }
    }

    // Write contours
    auto* contoursElem = doc.NewElement("contours");
    const int written = WriteContourElements(group, contoursElem, doc);
    contoursElem->SetAttribute("count", written);
    root->InsertEndChild(contoursElem);

    const auto outputFile = this->GetOutputLocation();
    if (doc.SaveFile(outputFile.c_str()) != tinyxml2::XML_SUCCESS)
        mitkThrow() << "xq_LumenSegIO: Failed to save file: " << outputFile;
}

// ---------------------------------------------------------------------------
// IOClone
// ---------------------------------------------------------------------------

xq_LumenSegIO* xq_LumenSegIO::IOClone() const
{
    return new xq_LumenSegIO(*this);
}

xq_LumenSegIO::ConfidenceLevel xq_LumenSegIO::GetReaderConfidenceLevel() const
{
    return endsWith(this->GetInputLocation(), ".xqctgr") ? Supported : Unsupported;
}

xq_LumenSegIO::ConfidenceLevel xq_LumenSegIO::GetWriterConfidenceLevel() const
{
    return endsWith(this->GetOutputLocation(), ".xqctgr") ? Supported : Unsupported;
}
