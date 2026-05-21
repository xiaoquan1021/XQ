#include "xq_ContourGroupIO.h"
#include "xq_ContourGroup.h"
#include "xq_XmlIOUtil.h"

#include <mitkCustomMimeType.h>
#include <mitkIOMimeTypes.h>

#include <tinyxml2.h>

#include <string>
#include <string_view>

static constexpr const char* kXqCgMimeName = "application/xq-contourgroup-cg";
static constexpr const char* kXqCgExtension = "xqcg";

bool xq_ContourGroupIO::endsWith(std::string_view str, std::string_view suffix)
{
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ---------------------------------------------------------------------------
// Constructor – registers MimeType and service
// ---------------------------------------------------------------------------

xq_ContourGroupIO::xq_ContourGroupIO()
    : mitk::AbstractFileIO(xq_ContourGroup::GetStaticNameOfClass(),
                           mitk::CustomMimeType(kXqCgMimeName),
                           "XQ Contour Group CG File")
{
    mitk::CustomMimeType mimeType(kXqCgMimeName);
    mimeType.SetCategory("XQ Segmentation");
    mimeType.SetComment("XQ Contour Group");
    mimeType.AddExtension(kXqCgExtension);
    this->SetMimeType(mimeType);

    this->SetReaderDescription("XQ Contour Group CG Reader");
    this->SetWriterDescription("XQ Contour Group CG Writer");

    RegisterService();
}

// ---------------------------------------------------------------------------
// DoRead
// ---------------------------------------------------------------------------

std::vector<mitk::BaseData::Pointer> xq_ContourGroupIO::DoRead()
{
    const auto filename = this->GetInputLocation();

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS)
        mitkThrow() << "xq_ContourGroupIO: Failed to load file: " << filename;

    auto* root = doc.FirstChildElement("xq_contour_group_cg");
    if (!root)
        mitkThrow() << "xq_ContourGroupIO: Missing <xq_contour_group_cg> root element in "
                     << filename;

    xq_XmlIOUtil xmlUtil(doc);
    auto cg = xq_ContourGroup::New();

    // Read path name
    auto pathName = xq_XmlIOUtil::ReadStringAttribute(root, "path_name");
    if (!pathName.empty())
        cg->SetPathName(pathName);

    // Read contour slices
    if (auto* contoursElem = root->FirstChildElement("contours"))
    {
        for (auto* contourElem = contoursElem->FirstChildElement("contour");
             contourElem;
             contourElem = contourElem->NextSiblingElement("contour"))
        {
            ContourSlice slice;

            double slicePos = 0.0;
            contourElem->QueryDoubleAttribute("slice_position", &slicePos);
            slice.slicePosition = slicePos;

            int closed = 1;
            contourElem->QueryIntAttribute("is_closed", &closed);
            slice.isClosed = (closed != 0);

            slice.method = xq_XmlIOUtil::ReadStringAttribute(contourElem, "method", "manual");

            // Read points
            if (auto* pointsElem = contourElem->FirstChildElement("points"))
            {
                for (auto* ptElem = pointsElem->FirstChildElement("point");
                     ptElem;
                     ptElem = ptElem->NextSiblingElement("point"))
                {
                    slice.points.push_back(xmlUtil.ParsePoint3D(ptElem));
                }
            }

            cg->AddContour(slice);
        }
    }

    std::vector<mitk::BaseData::Pointer> result;
    result.push_back(cg.GetPointer());
    return result;
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

void xq_ContourGroupIO::Write()
{
    ValidateOutputLocation();

    const auto* cg = dynamic_cast<const xq_ContourGroup*>(this->GetInput());
    if (!cg)
        mitkThrow() << "xq_ContourGroupIO: Input is not an xq_ContourGroup";

    tinyxml2::XMLDocument doc;
    xq_XmlIOUtil xmlUtil(doc);

    doc.InsertFirstChild(doc.NewDeclaration());

    auto* root = doc.NewElement("xq_contour_group_cg");
    root->SetAttribute("version", "1.0");
    xq_XmlIOUtil::WriteStringAttribute(root, "path_name", cg->GetPathName());
    doc.InsertEndChild(root);

    // Write contour slices
    auto* contoursElem = doc.NewElement("contours");
    const int contourCount = cg->GetContourCount();
    contoursElem->SetAttribute("count", contourCount);
    root->InsertEndChild(contoursElem);

    for (int i = 0; i < contourCount; ++i)
    {
        const auto* slice = cg->GetContour(i);
        if (!slice)
            continue;

        auto* contourElem = doc.NewElement("contour");
        contourElem->SetAttribute("id", i);
        contourElem->SetAttribute("slice_position", slice->slicePosition);
        contourElem->SetAttribute("is_closed", slice->isClosed ? 1 : 0);
        contourElem->SetAttribute("method", slice->method.c_str());
        contoursElem->InsertEndChild(contourElem);

        // Write points
        auto* pointsElem = doc.NewElement("points");
        pointsElem->SetAttribute("count", static_cast<int>(slice->points.size()));
        contourElem->InsertEndChild(pointsElem);

        for (int j = 0; j < static_cast<int>(slice->points.size()); ++j)
        {
            auto* ptElem = xmlUtil.BuildVertexElement("point", j, slice->points[j]);
            pointsElem->InsertEndChild(ptElem);
        }
    }

    const auto outputFile = this->GetOutputLocation();
    if (doc.SaveFile(outputFile.c_str()) != tinyxml2::XML_SUCCESS)
        mitkThrow() << "xq_ContourGroupIO: Failed to save file: " << outputFile;
}

// ---------------------------------------------------------------------------
// IOClone
// ---------------------------------------------------------------------------

xq_ContourGroupIO* xq_ContourGroupIO::IOClone() const
{
    return new xq_ContourGroupIO(*this);
}

xq_ContourGroupIO::ConfidenceLevel xq_ContourGroupIO::GetReaderConfidenceLevel() const
{
    return endsWith(this->GetInputLocation(), ".xqcg") ? Supported : Unsupported;
}

xq_ContourGroupIO::ConfidenceLevel xq_ContourGroupIO::GetWriterConfidenceLevel() const
{
    return endsWith(this->GetOutputLocation(), ".xqcg") ? Supported : Unsupported;
}
