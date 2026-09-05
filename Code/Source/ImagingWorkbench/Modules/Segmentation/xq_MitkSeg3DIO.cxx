#include "xq_MitkSeg3DIO.h"
#include "xq_MitkSeg3D.h"
#include "xq_XmlIOUtil.h"

#include <mitkCustomMimeType.h>
#include <mitkIOMimeTypes.h>
#include <mitkStringProperty.h>

#include <tinyxml2.h>

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>

#include <string>
#include <string_view>
#include <unordered_map>

static constexpr const char* kXqSeg3dMimeName = "application/xq-seg3d";
static constexpr const char* kXqSeg3dExtension = "xqseg3d";

// Method string -> enum lookup table
static const std::unordered_map<std::string, xq_MitkSeg3D::Seg3DMethod> kMethodFromString = {
    {"region_growing",  xq_MitkSeg3D::Seg3DMethod::REGION_GROWING},
    {"level_set",       xq_MitkSeg3D::Seg3DMethod::LEVEL_SET},
    {"colliding_fronts", xq_MitkSeg3D::Seg3DMethod::COLLIDING_FRONTS},
    {"threshold",       xq_MitkSeg3D::Seg3DMethod::THRESHOLD},
};

// ---------------------------------------------------------------------------
// Constructor – registers MimeType and service
// ---------------------------------------------------------------------------

xq_MitkSeg3DIO::xq_MitkSeg3DIO()
    : mitk::AbstractFileIO(xq_MitkSeg3D::GetStaticNameOfClass(),
                           mitk::CustomMimeType(kXqSeg3dMimeName),
                           "XQ 3D Segmentation File")
{
    mitk::CustomMimeType mimeType(kXqSeg3dMimeName);
    mimeType.SetCategory("XQ Segmentation");
    mimeType.SetComment("XQ 3D Segmentation");
    mimeType.AddExtension(kXqSeg3dExtension);
    this->SetMimeType(mimeType);

    this->SetReaderDescription("XQ Seg3D Reader");
    this->SetWriterDescription("XQ Seg3D Writer");

    RegisterService();
}

// ---------------------------------------------------------------------------
// DoRead
// ---------------------------------------------------------------------------

std::vector<mitk::BaseData::Pointer> xq_MitkSeg3DIO::DoRead()
{
    std::vector<mitk::BaseData::Pointer> result;

    const auto filename = this->GetInputLocation();

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != tinyxml2::XML_SUCCESS)
    {
        mitkThrow() << "xq_MitkSeg3DIO: Failed to load file: " << filename;
    }

    auto* root = doc.FirstChildElement("xq_seg3d");
    if (!root)
    {
        mitkThrow() << "xq_MitkSeg3DIO: Missing <xq_seg3d> root element in "
                     << filename;
    }

    xq_XmlIOUtil xmlUtil(doc);
    auto seg3d = xq_MitkSeg3D::New();

    // Read method via lookup table
    const auto method = xq_XmlIOUtil::ReadStringAttribute(root, "method", "threshold");
    if (auto it = kMethodFromString.find(method); it != kMethodFromString.end())
        seg3d->SetMethod(it->second);
    else
        seg3d->SetMethod(xq_MitkSeg3D::Seg3DMethod::THRESHOLD);

    // Read thresholds
    if (auto* threshElem = root->FirstChildElement("thresholds"))
    {
        double lower = 0.0, upper = 0.0;
        threshElem->QueryDoubleAttribute("lower", &lower);
        threshElem->QueryDoubleAttribute("upper", &upper);
        seg3d->SetLowerThreshold(lower);
        seg3d->SetUpperThreshold(upper);
    }

    // Read seed points
    if (auto* seedsElem = root->FirstChildElement("seed_points"))
    {
        seg3d->ClearSeedPoints();
        for (auto* ptElem = seedsElem->FirstChildElement("point");
             ptElem;
             ptElem = ptElem->NextSiblingElement("point"))
        {
            seg3d->AddSeedPoint(xmlUtil.ParsePoint3D(ptElem));
        }
    }

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
                seg3d->SetProperty(key, mitk::StringProperty::New(value));
        }
    }

    // Read embedded VTP polydata
    if (auto* vtpElem = root->FirstChildElement("vtp_file"))
    {
        const auto vtpPath = xq_XmlIOUtil::ReadStringAttribute(vtpElem, "path");
        if (!vtpPath.empty())
        {
            // Resolve relative path against the input file directory
            const auto dir = filename.substr(0, filename.find_last_of("/\\") + 1);
            const auto fullVtpPath = dir + vtpPath;

            auto reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
            reader->SetFileName(fullVtpPath.c_str());
            reader->Update();

            if (auto* polyData = reader->GetOutput();
                polyData && polyData->GetNumberOfPoints() > 0)
            {
                seg3d->SetSurfaceMesh(polyData);
            }
        }
    }

    result.push_back(seg3d.GetPointer());
    return result;
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

void xq_MitkSeg3DIO::Write()
{
    ValidateOutputLocation();

    const auto* seg3d = dynamic_cast<const xq_MitkSeg3D*>(this->GetInput());
    if (!seg3d)
        mitkThrow() << "xq_MitkSeg3DIO: Input is not an xq_MitkSeg3D";

    const auto outputFile = this->GetOutputLocation();

    tinyxml2::XMLDocument doc;
    xq_XmlIOUtil xmlUtil(doc);

    doc.InsertFirstChild(doc.NewDeclaration());

    auto* root = doc.NewElement("xq_seg3d");
    root->SetAttribute("version", "1.0");
    root->SetAttribute("method", std::string(seg3d->GetMethodString()).c_str());
    doc.InsertEndChild(root);

    // Write thresholds
    auto* threshElem = doc.NewElement("thresholds");
    threshElem->SetAttribute("lower", seg3d->GetLowerThreshold());
    threshElem->SetAttribute("upper", seg3d->GetUpperThreshold());
    root->InsertEndChild(threshElem);

    // Write seed points
    const auto seedPoints = seg3d->GetSeedPoints();
    if (!seedPoints.empty())
    {
        auto* seedsElem = doc.NewElement("seed_points");
        seedsElem->SetAttribute("count", static_cast<int>(seedPoints.size()));
        root->InsertEndChild(seedsElem);

        for (int i = 0; i < static_cast<int>(seedPoints.size()); ++i)
        {
            auto* ptElem = xmlUtil.BuildVertexElement("point", i, seedPoints[i]);
            seedsElem->InsertEndChild(ptElem);
        }
    }

    // Write VTP polydata as companion file
    auto polyData = seg3d->GetSurfaceMesh();
    if (polyData && polyData->GetNumberOfPoints() > 0)
    {
        auto baseName = outputFile;
        if (const auto dotPos = baseName.find_last_of('.'); dotPos != std::string::npos)
            baseName = baseName.substr(0, dotPos);
        const auto vtpFullPath = baseName + ".vtp";
        const auto vtpRelName = vtpFullPath.substr(vtpFullPath.find_last_of("/\\") + 1);

        auto writer = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
        writer->SetFileName(vtpFullPath.c_str());
        writer->SetInputData(polyData);
        writer->Write();

        auto* vtpElem = doc.NewElement("vtp_file");
        vtpElem->SetAttribute("path", vtpRelName.c_str());
        root->InsertEndChild(vtpElem);
    }

    if (doc.SaveFile(outputFile.c_str()) != tinyxml2::XML_SUCCESS)
        mitkThrow() << "xq_MitkSeg3DIO: Failed to save file: " << outputFile;
}

// ---------------------------------------------------------------------------
// IOClone
// ---------------------------------------------------------------------------

xq_MitkSeg3DIO* xq_MitkSeg3DIO::IOClone() const
{
    return new xq_MitkSeg3DIO(*this);
}
