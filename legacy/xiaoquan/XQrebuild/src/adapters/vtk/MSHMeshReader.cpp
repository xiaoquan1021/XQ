#include "adapters/vtk/MSHMeshReader.h"

#include <tinyxml2.h>

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDataSet.h>
#include <vtkDataSetAttributes.h>
#include <vtkErrorCode.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLUnstructuredGridReader.h>

#include <fstream>
#include <map>
#include <memory>
#include <string>

namespace xq {
namespace {

std::string basename_without_extension(const std::string& path)
{
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t begin = slash == std::string::npos ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    const std::size_t end = (dot == std::string::npos || dot < begin) ? path.size() : dot;
    return path.substr(begin, end - begin);
}

std::string path_without_extension(const std::string& path)
{
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t begin = slash == std::string::npos ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || dot < begin) {
        return path;
    }
    return path.substr(0, dot);
}

bool file_exists(const std::string& path)
{
    std::ifstream input(path.c_str(), std::ios::binary);
    return input.good();
}

bool has_array(vtkDataSetAttributes* attributes, const char* name)
{
    return attributes != 0 && attributes->GetArray(name) != 0;
}

bool has_point_or_cell_array(vtkDataSet* dataSet, const char* name)
{
    if (dataSet == 0) {
        return false;
    }

    return has_array(dataSet->GetPointData(), name)
        || has_array(dataSet->GetCellData(), name);
}

bool read_msh_metadata(const std::string& mshFilePath, std::string* meshName)
{
    if (meshName == 0) {
        return false;
    }

    tinyxml2::XMLDocument document;
    if (document.LoadFile(mshFilePath.c_str()) != tinyxml2::XML_SUCCESS) {
        return false;
    }

    const tinyxml2::XMLElement* meshElement = document.FirstChildElement("mitk_mesh");
    if (meshElement == 0) {
        return false;
    }

    const char* type = meshElement->Attribute("type");
    const char* modelName = meshElement->Attribute("model_name");
    if (type == 0 || std::string(type) != "TetGen" || modelName == 0 || modelName[0] == '\0') {
        return false;
    }

    *meshName = modelName;
    return true;
}

PreservedMeshArrays read_preserved_arrays(vtkUnstructuredGrid* volumeGrid, vtkPolyData* surfaceMesh)
{
    PreservedMeshArrays arrays = {};
    arrays.hasGlobalNodeID = has_point_or_cell_array(volumeGrid, "GlobalNodeID")
        || has_point_or_cell_array(surfaceMesh, "GlobalNodeID");
    arrays.hasGlobalElementID = has_point_or_cell_array(volumeGrid, "GlobalElementID")
        || has_point_or_cell_array(surfaceMesh, "GlobalElementID");
    arrays.hasModelFaceID = has_point_or_cell_array(volumeGrid, "ModelFaceID")
        || has_point_or_cell_array(surfaceMesh, "ModelFaceID");
    arrays.hasCapID = has_point_or_cell_array(volumeGrid, "CapID")
        || has_point_or_cell_array(surfaceMesh, "CapID");
    return arrays;
}

bool add_boundary_faces(vtkPolyData* polyData, XQMesh* mesh)
{
    if (polyData == 0 || mesh == 0) {
        return false;
    }

    vtkCellData* cellData = polyData->GetCellData();
    vtkDataArray* modelFaceIds = cellData == 0 ? 0 : cellData->GetArray("ModelFaceID");
    if (modelFaceIds == 0) {
        return true;
    }

    const vtkIdType cellCount = polyData->GetNumberOfCells();
    if (modelFaceIds->GetNumberOfTuples() < cellCount) {
        return false;
    }

    std::map<int, MeshBoundaryFace> facesById;
    for (vtkIdType cellId = 0; cellId < cellCount; ++cellId) {
        const int faceId = static_cast<int>(modelFaceIds->GetTuple1(cellId));
        std::map<int, MeshBoundaryFace>::iterator found = facesById.find(faceId);
        if (found == facesById.end()) {
            MeshBoundaryFace face = {};
            face.faceId = faceId;
            face.name = "";
            face.kind = FaceKind::Unknown;
            found = facesById.insert(std::make_pair(faceId, face)).first;
        }
        found->second.cellIds.push_back(static_cast<int>(cellId));
    }

    for (std::map<int, MeshBoundaryFace>::const_iterator it = facesById.begin();
         it != facesById.end();
         ++it) {
        mesh->addBoundaryFace(it->second);
    }

    return true;
}

} // namespace

MSHMeshReader::Status MSHMeshReader::read(const std::string& mshFilePath, MSHReadResult* out)
{
    if (out == 0) {
        return Status::MshParseError;
    }

    if (!file_exists(mshFilePath)) {
        return Status::MshNotFound;
    }

    const std::string pathWithoutExtension = path_without_extension(mshFilePath);
    const std::string vtuFilePath = pathWithoutExtension + ".vtu";
    if (!file_exists(vtuFilePath)) {
        return Status::VtuNotFound;
    }

    const std::string vtpFilePath = pathWithoutExtension + ".vtp";
    if (!file_exists(vtpFilePath)) {
        return Status::VtpNotFound;
    }

    std::string meshName;
    if (!read_msh_metadata(mshFilePath, &meshName)) {
        return Status::MshParseError;
    }

    vtkSmartPointer<vtkXMLUnstructuredGridReader> volumeReader =
        vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
    volumeReader->SetFileName(vtuFilePath.c_str());
    volumeReader->Update();
    if (volumeReader->GetErrorCode() != vtkErrorCode::NoError) {
        return Status::VtuReadError;
    }

    vtkUnstructuredGrid* volumeGrid = volumeReader->GetOutput();
    if (volumeGrid == 0
        || volumeGrid->GetNumberOfPoints() <= 0
        || volumeGrid->GetNumberOfCells() <= 0) {
        return Status::VtuReadError;
    }

    vtkSmartPointer<vtkXMLPolyDataReader> surfaceReader =
        vtkSmartPointer<vtkXMLPolyDataReader>::New();
    surfaceReader->SetFileName(vtpFilePath.c_str());
    surfaceReader->Update();
    if (surfaceReader->GetErrorCode() != vtkErrorCode::NoError) {
        return Status::VtpReadError;
    }

    vtkPolyData* surfaceMesh = surfaceReader->GetOutput();
    if (surfaceMesh == 0
        || surfaceMesh->GetNumberOfPoints() <= 0
        || surfaceMesh->GetNumberOfCells() <= 0) {
        return Status::VtpReadError;
    }

    MSHReadResult result = {};
    result.meshName = meshName;
    result.sourceRelativePath = "Meshes/" + basename_without_extension(mshFilePath) + ".msh";

    VolumeMeshHandle volumeHandle = {};
    volumeHandle.setCounts(static_cast<std::size_t>(volumeGrid->GetNumberOfPoints()),
                           static_cast<std::size_t>(volumeGrid->GetNumberOfCells()));
    result.mesh.setVolumeGrid(std::make_shared<VolumeMeshHandle>(volumeHandle));

    SurfaceMeshHandle surfaceHandle = {};
    surfaceHandle.setCounts(static_cast<std::size_t>(surfaceMesh->GetNumberOfPoints()),
                            static_cast<std::size_t>(surfaceMesh->GetNumberOfCells()));
    result.mesh.setSurfaceMesh(std::make_shared<SurfaceMeshHandle>(surfaceHandle));

    result.mesh.setPreservedArrays(read_preserved_arrays(volumeGrid, surfaceMesh));

    MeshQualitySummary quality = {};
    quality.elementCount = static_cast<std::size_t>(volumeGrid->GetNumberOfCells());
    result.mesh.setQuality(quality);

    if (!add_boundary_faces(surfaceMesh, &result.mesh)) {
        return Status::VtpReadError;
    }

    *out = result;
    return Status::Ok;
}

} // namespace xq
