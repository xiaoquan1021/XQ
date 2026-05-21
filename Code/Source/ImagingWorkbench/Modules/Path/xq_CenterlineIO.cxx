#include "xq_CenterlineIO.h"
#include "xq_VesselCenterline.h"
#include "xq_CenterlineSegment.h"
#include "xq_XmlIOUtil.h"

#include <mitkCustomMimeType.h>
#include <mitkIOMimeTypes.h>

#include <tinyxml2.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string_view>

static constexpr std::string_view XQPTH_MIME_NAME = "application/vnd.xq.path";
static constexpr std::string_view XQPTH_EXTENSION = "xqpth";

// Functional iteration over XML sibling elements with the same tag name
static auto forEachXmlSibling(tinyxml2::XMLElement* parent, const char* tag)
{
    struct Range {
        tinyxml2::XMLElement* first;
        const char* tag;

        struct Iterator {
            tinyxml2::XMLElement* cur;
            const char* tag;
            auto& operator++() { cur = cur->NextSiblingElement(tag); return *this; }
            bool operator!=(const Iterator& o) const { return cur != o.cur; }
            tinyxml2::XMLElement* operator*() const { return cur; }
        };

        Iterator begin() const { return {first ? first->FirstChildElement(tag) : nullptr, tag}; }
        Iterator end()   const { return {nullptr, tag}; }
    };
    return Range{parent, tag};
}

// ---------------------------------------------------------------------------
// Constructor – registers MimeType and service
// ---------------------------------------------------------------------------

xq_CenterlineIO::xq_CenterlineIO()
    : mitk::AbstractFileIO(xq_VesselCenterline::GetStaticNameOfClass(),
                           mitk::CustomMimeType(std::string{XQPTH_MIME_NAME}),
                           "XQ Path File")
{
    mitk::CustomMimeType mimeType(std::string{XQPTH_MIME_NAME});
    mimeType.SetCategory("XQ Path");
    mimeType.SetComment("XQ Vascular Path");
    mimeType.AddExtension(std::string{XQPTH_EXTENSION});
    this->SetMimeType(mimeType);

    this->SetReaderDescription("XQ Path Reader");
    this->SetWriterDescription("XQ Path Writer");

    RegisterService();
}

// ---------------------------------------------------------------------------
// Reader
// ---------------------------------------------------------------------------

std::vector<mitk::BaseData::Pointer> xq_CenterlineIO::DoRead()
{
    const auto filename = this->GetInputLocation();

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS)
        mitkThrow() << "xq_CenterlineIO: Failed to load file: " << filename;

    auto* root = doc.FirstChildElement("xq_path");
    if (!root)
        mitkThrow() << "xq_CenterlineIO: Missing <xq_path> root element in " << filename;

    xq_XmlIOUtil xmlUtil(doc);
    auto path = xq_VesselCenterline::New();

    // Read properties via lambda
    auto readProperties = [&path](tinyxml2::XMLElement* propsElem) {
        for (auto* propElem : forEachXmlSibling(propsElem, "property"))
        {
            if (auto key = xq_XmlIOUtil::ReadStringAttribute(propElem, "key"); !key.empty())
                path->SetAttribute(key, xq_XmlIOUtil::ReadStringAttribute(propElem, "value"));
        }
    };

    if (auto* propsElem = root->FirstChildElement("properties"))
        readProperties(propsElem);

    // Read path element
    if (auto* pathElemNode = root->FirstChildElement("path_element"))
    {
        auto* elem = new xq_CenterlineSegment();

        if (int method = 0; pathElemNode->QueryIntAttribute("method", &method) == tinyxml2::XML_SUCCESS)
            elem->SetInterpolationMode(static_cast<xq_CenterlineSegment::InterpolationMode>(method));

        if (int calcNum = 100; pathElemNode->QueryIntAttribute("calculation_number", &calcNum) == tinyxml2::XML_SUCCESS)
            elem->SetSampleDensity(calcNum);

        if (double spacing = 0.5; pathElemNode->QueryDoubleAttribute("spacing", &spacing) == tinyxml2::XML_SUCCESS)
            elem->SetStepSize(spacing);

        std::vector<mitk::Point3D> controlPoints;
        if (auto* ctrlRoot = pathElemNode->FirstChildElement("control_points"))
        {
            for (auto* ptElem : forEachXmlSibling(ctrlRoot, "point"))
                controlPoints.push_back(xmlUtil.ParsePoint3D(ptElem));
        }

        elem->ReplaceAnchors(controlPoints, true);
        path->SetSegment(elem, 0);
    }

    return {path.GetPointer()};
}

mitk::AbstractFileIO::ConfidenceLevel xq_CenterlineIO::GetReaderConfidenceLevel() const
{
    if (mitk::AbstractFileIO::GetReaderConfidenceLevel() == Unsupported)
        return Unsupported;

    const auto ext = std::filesystem::path(this->GetInputLocation()).extension().string();
    return (ext == std::string{"."}  + std::string{XQPTH_EXTENSION}) ? Supported : Unsupported;
}

// ---------------------------------------------------------------------------
// Writer
// ---------------------------------------------------------------------------

void xq_CenterlineIO::Write()
{
    ValidateOutputLocation();

    const auto* path = dynamic_cast<const xq_VesselCenterline*>(this->GetInput());
    if (!path)
        mitkThrow() << "xq_CenterlineIO: Input is not an xq_VesselCenterline";

    tinyxml2::XMLDocument doc;
    xq_XmlIOUtil xmlUtil(doc);

    doc.InsertFirstChild(doc.NewDeclaration());

    auto* root = doc.NewElement("xq_path");
    root->SetAttribute("version", "1.0");
    doc.InsertEndChild(root);

    // Properties – structured bindings
    if (const auto props = path->GetAttributes(); !props.empty())
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

    // Path element
    if (auto* elem = path->GetSegment(0))
    {
        auto* pathElemNode = doc.NewElement("path_element");
        pathElemNode->SetAttribute("method", static_cast<int>(elem->GetInterpolationMode()));
        pathElemNode->SetAttribute("calculation_number", elem->GetSampleDensity());
        pathElemNode->SetAttribute("spacing", elem->GetStepSize());
        root->InsertEndChild(pathElemNode);

        const auto& ctrlPts = elem->GetAnchorPositions();
        auto* ctrlRoot = doc.NewElement("control_points");
        ctrlRoot->SetAttribute("count", static_cast<int>(ctrlPts.size()));
        pathElemNode->InsertEndChild(ctrlRoot);

        // Enumerate control points with index via std::for_each
        int idx = 0;
        std::for_each(ctrlPts.cbegin(), ctrlPts.cend(), [&](const mitk::Point3D& pt) {
            ctrlRoot->InsertEndChild(xmlUtil.BuildVertexElement("point", idx++, pt));
        });
    }

    if (const auto outputFile = this->GetOutputLocation();
        doc.SaveFile(outputFile.c_str()) != tinyxml2::XML_SUCCESS)
        mitkThrow() << "xq_CenterlineIO: Failed to save file: " << outputFile;
}

mitk::AbstractFileIO::ConfidenceLevel xq_CenterlineIO::GetWriterConfidenceLevel() const
{
    return dynamic_cast<const xq_VesselCenterline*>(this->GetInput()) ? Supported : Unsupported;
}

// ---------------------------------------------------------------------------

xq_CenterlineIO* xq_CenterlineIO::IOClone() const
{
    return new xq_CenterlineIO(*this);
}
