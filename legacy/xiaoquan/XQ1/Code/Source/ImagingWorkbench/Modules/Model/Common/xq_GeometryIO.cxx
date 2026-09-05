#include "xq_GeometryIO.h"
#include "xq_Model.h"
#include "xq_PolyGeometry.h"

#include <mitkCustomMimeType.h>
#include <mitkIOMimeTypes.h>

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkIntArray.h>
#include <vtkCellData.h>

#include <tinyxml2.h>

#include <memory>
#include <string>

xq_GeometryIO::xq_GeometryIO()
    : mitk::AbstractFileIO(xq_Model::GetStaticNameOfClass())
{
    mitk::CustomMimeType mimeType("application/x-xq-model");
    mimeType.SetCategory("XQ Model");
    mimeType.SetComment("XQ Model Data");
    mimeType.AddExtension("xqmdl");

    this->SetMimeType(mimeType);
    this->SetReaderDescription("XQ Model Reader");
    this->SetWriterDescription("XQ Model Writer");

    AbstractFileWriter::SetRanking(10);
    AbstractFileReader::SetRanking(10);

    this->RegisterService();
}

xq_GeometryIO* xq_GeometryIO::IOClone() const
{
    return new xq_GeometryIO(*this);
}

std::vector<mitk::BaseData::Pointer> xq_GeometryIO::DoRead()
{
    std::vector<mitk::BaseData::Pointer> result;
    const auto fileName = this->GetInputLocation();

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(fileName.c_str()) != tinyxml2::XML_SUCCESS)
    {
        mitkThrow() << "Failed to load XQ model file: " << fileName;
    }

    auto* root = doc.RootElement();
    if (!root)
    {
        mitkThrow() << "Invalid XQ model file: no root element";
    }

    auto model = xq_Model::New();

    if (const auto* typeAttr = root->Attribute("type"))
    {
        model->SetType(typeAttr);
    }

    // Read properties
    if (auto* propsElem = root->FirstChildElement("Properties"))
    {
        for (auto* prop = propsElem->FirstChildElement("Property");
             prop != nullptr;
             prop = prop->NextSiblingElement("Property"))
        {
            const auto* key = prop->Attribute("key");
            const auto* value = prop->Attribute("value");
            if (key && value)
            {
                model->SetProperty(key, value);
            }
        }
    }

    // Read face info and geometry
    auto element = std::make_unique<xq_PolyGeometry>();

    if (auto* facesElem = root->FirstChildElement("Faces"))
    {
        for (auto* faceElem = facesElem->FirstChildElement("Face");
             faceElem != nullptr;
             faceElem = faceElem->NextSiblingElement("Face"))
        {
            FaceInfo fi;
            faceElem->QueryIntAttribute("id", &fi.id);
            if (const auto* name = faceElem->Attribute("name"))
                fi.name = name;
            if (const auto* type = faceElem->Attribute("type"))
                fi.type = type;
            faceElem->QueryBoolAttribute("visible", &fi.visible);
            faceElem->QueryFloatAttribute("opacity", &fi.opacity);
            faceElem->QueryFloatAttribute("r", &fi.color[0]);
            faceElem->QueryFloatAttribute("g", &fi.color[1]);
            faceElem->QueryFloatAttribute("b", &fi.color[2]);
            element->SetFaceInfo(fi.id, fi);
        }
    }

    if (auto* geomElem = root->FirstChildElement("Geometry"))
    {
        if (const auto* vtpFile = geomElem->Attribute("file"))
        {
            const auto dir = fileName.substr(0, fileName.find_last_of("/\\") + 1);
            const auto vtpPath = dir + vtpFile;

            auto reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
            reader->SetFileName(vtpPath.c_str());
            reader->Update();

            if (reader->GetOutput())
            {
                element->SetWholeVtkPolyData(reader->GetOutput());
            }
        }
    }

    model->SetModelElement(std::move(element), 0);
    result.push_back(model.GetPointer());
    return result;
}

void xq_GeometryIO::Write()
{
    const auto* model = dynamic_cast<const xq_Model*>(this->GetInput());
    if (!model)
    {
        mitkThrow() << "Cannot write: invalid model data";
    }

    const auto fileName = this->GetOutputLocation();

    tinyxml2::XMLDocument doc;
    auto* root = doc.NewElement("XQModel");
    doc.InsertEndChild(root);

    root->SetAttribute("type", model->GetType().c_str());
    root->SetAttribute("version", "1.0");

    auto* propsElem = doc.NewElement("Properties");
    root->InsertEndChild(propsElem);

    auto* element = model->GetModelElement(0);
    if (element)
    {
        auto* facesElem = doc.NewElement("Faces");
        root->InsertEndChild(facesElem);

        for (const auto& face : element->GetAllFaceInfos())
        {
            auto* faceElem = doc.NewElement("Face");
            faceElem->SetAttribute("id", face.id);
            faceElem->SetAttribute("name", face.name.c_str());
            faceElem->SetAttribute("type", face.type.c_str());
            faceElem->SetAttribute("visible", face.visible);
            faceElem->SetAttribute("opacity", face.opacity);
            faceElem->SetAttribute("r", face.color[0]);
            faceElem->SetAttribute("g", face.color[1]);
            faceElem->SetAttribute("b", face.color[2]);
            facesElem->InsertEndChild(faceElem);
        }

        auto polyData = element->GetWholeVtkPolyData();
        if (polyData && polyData->GetNumberOfPoints() > 0)
        {
            const auto vtpFileName = fileName + ".vtp";
            const auto vtpBaseName = vtpFileName.substr(vtpFileName.find_last_of("/\\") + 1);

            auto writer = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
            writer->SetFileName(vtpFileName.c_str());
            writer->SetInputData(polyData);
            writer->Write();

            auto* geomElem = doc.NewElement("Geometry");
            geomElem->SetAttribute("file", vtpBaseName.c_str());
            root->InsertEndChild(geomElem);
        }
    }

    doc.SaveFile(fileName.c_str());
}
