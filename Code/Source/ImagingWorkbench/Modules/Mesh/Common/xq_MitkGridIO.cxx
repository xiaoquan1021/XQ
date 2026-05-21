#include "xq_MitkGridIO.h"
#include "xq_MitkGrid.h"
#include "xq_Grid.h"
#include "xq_GridFactory.h"

#include <mitkCustomMimeType.h>
#include <mitkIOMimeTypes.h>

#include <vtkSmartPointer.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkXMLUnstructuredGridWriter.h>
#include <vtkUnstructuredGrid.h>
#include <vtkGeometryFilter.h>
#include <vtkPolyData.h>

#include <tinyxml2.h>

#include <string>

namespace
{

constexpr const char* kRootTag           = "GridConfiguration";
constexpr const char* kVersionAttr       = "formatVersion";
constexpr const char* kCurrentVersion    = "2.0";
constexpr const char* kSolverAttr        = "solver";
constexpr const char* kDefaultSolver     = "TetGen";
constexpr const char* kSizingTag         = "SizingConstraints";
constexpr const char* kRefinementTag     = "FaceRefinement";
constexpr const char* kVolumeRefTag      = "VolumeReference";

mitk::CustomMimeType CreateXqMeshMimeType()
{
    mitk::CustomMimeType mimeType("application/x-xq-mesh");
    mimeType.SetCategory("XQ Mesh Files");
    mimeType.SetComment("XQ Mesh Data");
    mimeType.AddExtension("xqmsh");
    return mimeType;
}

std::string ExtractDirectory(const std::string& filePath)
{
    auto pos = filePath.find_last_of("/\\");
    return (pos != std::string::npos) ? filePath.substr(0, pos + 1) : "";
}

std::string StripDirectory(const std::string& filePath)
{
    auto pos = filePath.find_last_of("/\\");
    return (pos != std::string::npos) ? filePath.substr(pos + 1) : filePath;
}

std::string ReplaceExtension(const std::string& filePath, const std::string& newExt)
{
    auto dotPos = filePath.find_last_of('.');
    return (dotPos != std::string::npos) ? filePath.substr(0, dotPos) + newExt : filePath + newExt;
}

MeshParams DeserializeParams(const tinyxml2::XMLElement* sizingElem)
{
    MeshParams params;
    if (!sizingElem)
        return params;

    auto readAttr = [&](const char* name, auto& target) {
        using T = std::decay_t<decltype(target)>;
        if constexpr (std::is_same_v<T, double>)
            sizingElem->QueryDoubleAttribute(name, &target);
        else if constexpr (std::is_same_v<T, bool>)
            sizingElem->QueryBoolAttribute(name, &target);
        else if constexpr (std::is_same_v<T, int>)
            sizingElem->QueryIntAttribute(name, &target);
    };

    readAttr("edgeLength",    params.globalEdgeSize);
    readAttr("maxEdgeLength",  params.globalMaxEdgeSize);
    readAttr("surfaceOnly",    params.surfaceMeshOnly);
    readAttr("smoothingPasses", params.optimizationPasses);

    for (auto* refElem = sizingElem->FirstChildElement(kRefinementTag);
         refElem; refElem = refElem->NextSiblingElement(kRefinementTag))
    {
        int faceId = 0;
        double sz = 0.0;
        refElem->QueryIntAttribute("face", &faceId);
        refElem->QueryDoubleAttribute("edgeLength", &sz);
        params.localEdgeSizes[faceId] = sz;
    }
    return params;
}

void SerializeParams(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* parent,
                     const MeshParams& params)
{
    auto* sizingElem = doc.NewElement(kSizingTag);
    sizingElem->SetAttribute("edgeLength",      params.globalEdgeSize);
    sizingElem->SetAttribute("maxEdgeLength",    params.globalMaxEdgeSize);
    sizingElem->SetAttribute("surfaceOnly",      params.surfaceMeshOnly);
    sizingElem->SetAttribute("smoothingPasses",  params.optimizationPasses);

    for (const auto& [faceId, sz] : params.localEdgeSizes)
    {
        auto* refElem = doc.NewElement(kRefinementTag);
        refElem->SetAttribute("face", faceId);
        refElem->SetAttribute("edgeLength", sz);
        sizingElem->InsertEndChild(refElem);
    }
    parent->InsertEndChild(sizingElem);
}

vtkSmartPointer<vtkUnstructuredGrid> LoadVtu(const std::string& path)
{
    auto reader = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
    reader->SetFileName(path.c_str());
    reader->Update();
    return reader->GetOutput();
}

vtkSmartPointer<vtkPolyData> ExtractSurface(vtkUnstructuredGrid* ugrid)
{
    auto geomFilter = vtkSmartPointer<vtkGeometryFilter>::New();
    geomFilter->SetInputData(ugrid);
    geomFilter->Update();
    return geomFilter->GetOutput();
}

} // anonymous namespace

xq_MitkGridIO::xq_MitkGridIO()
    : mitk::AbstractFileIO(xq_MitkGrid::GetStaticNameOfClass(),
                           CreateXqMeshMimeType(),
                           "XQ Mesh Data")
{
    this->RegisterService();
}

std::vector<mitk::BaseData::Pointer> xq_MitkGridIO::DoRead()
{
    const auto fileName = this->GetInputLocation();

    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(fileName.c_str()) != tinyxml2::XML_SUCCESS)
        mitkThrow() << "Failed to load XQ mesh file: " << fileName;

    auto* rootElem = doc.FirstChildElement(kRootTag);
    if (!rootElem)
        mitkThrow() << "Invalid XQ mesh file: missing " << kRootTag << " element";

    const char* solverAttr = rootElem->Attribute(kSolverAttr);
    const std::string solverType = solverAttr ? solverAttr : kDefaultSolver;

    auto params = DeserializeParams(rootElem->FirstChildElement(kSizingTag));

    auto mitkMesh = xq_MitkGrid::New();

    if (auto* volRefElem = rootElem->FirstChildElement(kVolumeRefTag))
    {
        const char* vtuFile = volRefElem->Attribute("href");
        if (vtuFile)
        {
            const auto vtuPath = ExtractDirectory(fileName) + vtuFile;
            auto volumeData = LoadVtu(vtuPath);

            if (volumeData && volumeData->GetNumberOfPoints() > 0)
            {
                if (auto mesh = xq_GridFactory::CreateMesh(solverType))
                {
                    mesh->SetMeshParams(params);
                    mesh->GetVolumeMesh()->DeepCopy(volumeData);
                    mesh->GetSurfaceMesh()->DeepCopy(ExtractSurface(volumeData));
                    mitkMesh->SetMesh(mesh.release(), 0);
                }
            }
        }
    }

    return {mitkMesh.GetPointer()};
}

void xq_MitkGridIO::Write()
{
    const auto* mitkMesh = dynamic_cast<const xq_MitkGrid*>(this->GetInput());
    if (!mitkMesh)
        mitkThrow() << "Invalid input for XQ mesh writer";

    const auto fileName = this->GetOutputLocation();
    auto* mesh = mitkMesh->GetMesh(0);
    if (!mesh)
        mitkThrow() << "No mesh data to write";

    tinyxml2::XMLDocument doc;
    auto* rootElem = doc.NewElement(kRootTag);
    rootElem->SetAttribute(kVersionAttr, kCurrentVersion);
    rootElem->SetAttribute(kSolverAttr, mesh->GetType().c_str());
    doc.InsertEndChild(rootElem);

    SerializeParams(doc, rootElem, mesh->GetMeshParams());

    const auto vtuFileName = ReplaceExtension(fileName, ".vtu");
    auto* volRefElem = doc.NewElement(kVolumeRefTag);
    volRefElem->SetAttribute("href", StripDirectory(vtuFileName).c_str());
    rootElem->InsertEndChild(volRefElem);

    auto writer = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
    writer->SetFileName(vtuFileName.c_str());
    writer->SetInputData(mesh->GetVolumeMesh());
    writer->Write();

    doc.SaveFile(fileName.c_str());
}

xq_MitkGridIO* xq_MitkGridIO::IOClone() const
{
    return new xq_MitkGridIO(*this);
}
