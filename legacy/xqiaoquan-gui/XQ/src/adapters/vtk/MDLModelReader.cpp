#include "adapters/vtk/MDLModelReader.h"

#include <tinyxml2.h>

#include <vtkCellData.h>
#include <vtkCellType.h>
#include <vtkDataArray.h>
#include <vtkDataSetAttributes.h>
#include <vtkErrorCode.h>
#include <vtkIdList.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkTriangleFilter.h>
#include <vtkXMLPolyDataReader.h>

#include <fstream>
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

FaceKind face_kind_from_type(const char* typeAttribute)
{
    const std::string type = typeAttribute == 0 ? "" : typeAttribute;
    if (type == "wall") {
        return FaceKind::Wall;
    }
    if (type == "cap") {
        return FaceKind::Cap;
    }
    if (type == "inlet") {
        return FaceKind::Inlet;
    }
    if (type == "outlet") {
        return FaceKind::Outlet;
    }
    return FaceKind::Unknown;
}

const tinyxml2::XMLElement* first_model_element(const tinyxml2::XMLDocument& document)
{
    const tinyxml2::XMLElement* modelElement = document.FirstChildElement("model");
    if (modelElement == 0) {
        return 0;
    }

    const tinyxml2::XMLElement* timestepElement = modelElement->FirstChildElement("timestep");
    if (timestepElement == 0) {
        return 0;
    }

    return timestepElement->FirstChildElement("model_element");
}

bool read_face(const tinyxml2::XMLElement* faceElement, ModelFace* out)
{
    if (faceElement == 0 || out == 0) {
        return false;
    }

    ModelFace face = {};
    if (faceElement->QueryIntAttribute("id", &face.faceId) != tinyxml2::XML_SUCCESS) {
        return false;
    }

    const char* name = faceElement->Attribute("name");
    const char* type = faceElement->Attribute("type");
    if (name == 0 || type == 0) {
        return false;
    }

    face.name = name;
    face.kind = face_kind_from_type(type);

    int capId = 0;
    const tinyxml2::XMLError capIdError = faceElement->QueryIntAttribute("capid", &capId);
    if (capIdError == tinyxml2::XML_SUCCESS) {
        face.capId = capId;
    } else if (capIdError == tinyxml2::XML_NO_ATTRIBUTE) {
        if (face.kind == FaceKind::Cap) {
            face.capId = face.faceId;
        }
    } else {
        return false;
    }

    *out = face;
    return true;
}

bool read_faces(const tinyxml2::XMLDocument& document, XQSurfaceModel* model)
{
    if (model == 0) {
        return false;
    }

    const tinyxml2::XMLElement* modelElement = first_model_element(document);
    if (modelElement == 0) {
        return false;
    }

    const tinyxml2::XMLElement* facesElement = modelElement->FirstChildElement("faces");
    if (facesElement == 0) {
        return false;
    }

    bool sawFace = false;
    for (const tinyxml2::XMLElement* faceElement = facesElement->FirstChildElement("face");
         faceElement != 0;
         faceElement = faceElement->NextSiblingElement("face")) {
        ModelFace face = {};
        if (!read_face(faceElement, &face)) {
            return false;
        }
        model->addFace(face);
        sawFace = true;
    }

    return sawFace;
}

bool has_array(vtkDataSetAttributes* attributes, const char* name)
{
    return attributes != 0 && attributes->GetArray(name) != 0;
}

bool has_point_or_cell_array(vtkPolyData* polyData, const char* name)
{
    if (polyData == 0) {
        return false;
    }

    return has_array(polyData->GetPointData(), name)
        || has_array(polyData->GetCellData(), name);
}

PreservedVtpArrays read_preserved_arrays(vtkPolyData* polyData)
{
    PreservedVtpArrays arrays = {};
    arrays.hasGlobalNodeID = has_point_or_cell_array(polyData, "GlobalNodeID");
    arrays.hasGlobalElementID = has_point_or_cell_array(polyData, "GlobalElementID");
    arrays.hasModelFaceID = has_point_or_cell_array(polyData, "ModelFaceID");
    arrays.hasCapID = has_point_or_cell_array(polyData, "CapID");
    return arrays;
}

// Builds the real triangle geometry the render path needs (upsertNode only
// mounts a SurfaceModel actor when hasTriangleGeometry()). The .vtp may carry
// strips/polys, so triangulate first; every point copies across and every
// output triangle keeps its ModelFaceID (the filter passes cell data), falling
// back to face id 1 when the array is absent. Returns false when triangulation
// yields no triangles (caller maps that to VtpReadError -- no empty geometry).
bool build_triangle_geometry(vtkPolyData* polyData, XQTriangleSurfaceGeometryHandle* out)
{
    vtkNew<vtkTriangleFilter> triangulate;
    triangulate->SetInputData(polyData);
    triangulate->Update();
    vtkPolyData* tri = triangulate->GetOutput();
    if (tri == 0 || tri->GetNumberOfPoints() <= 0 || tri->GetNumberOfCells() <= 0) {
        return false;
    }

    vtkPoints* points = tri->GetPoints();
    if (points == 0) {
        return false;
    }
    const vtkIdType pointCount = points->GetNumberOfPoints();
    for (vtkIdType p = 0; p < pointCount; ++p) {
        double xyz[3] = {0.0, 0.0, 0.0};
        points->GetPoint(p, xyz);
        Point3 point = {xyz[0], xyz[1], xyz[2]};
        out->addPoint(point);
    }

    // ModelFaceID is the per-cell model/boundary face tag; pass it through so the
    // renderer's faceId colouring matches SimVascular. Absent -> uniform id 1.
    vtkDataArray* faceIds = tri->GetCellData() != 0
        ? tri->GetCellData()->GetArray("ModelFaceID")
        : 0;

    vtkNew<vtkIdList> cellPoints;
    const vtkIdType cellCount = tri->GetNumberOfCells();
    for (vtkIdType c = 0; c < cellCount; ++c) {
        if (tri->GetCellType(c) != VTK_TRIANGLE) {
            continue;
        }
        tri->GetCellPoints(c, cellPoints);
        if (cellPoints->GetNumberOfIds() != 3) {
            continue;
        }
        int faceId = 1;
        if (faceIds != 0) {
            faceId = static_cast<int>(faceIds->GetComponent(c, 0));
        }
        out->addTriangle(static_cast<int>(cellPoints->GetId(0)),
                         static_cast<int>(cellPoints->GetId(1)),
                         static_cast<int>(cellPoints->GetId(2)),
                         faceId);
    }

    return out->triangleCount() > 0;
}

} // namespace

MDLModelReader::Status MDLModelReader::read(const std::string& mdlFilePath, MDLReadResult* out)
{
    if (out == 0) {
        return Status::MdlParseError;
    }

    if (!file_exists(mdlFilePath)) {
        return Status::MdlNotFound;
    }

    const std::string modelName = basename_without_extension(mdlFilePath);
    const std::string vtpFilePath = path_without_extension(mdlFilePath) + ".vtp";
    if (!file_exists(vtpFilePath)) {
        return Status::VtpNotFound;
    }

    tinyxml2::XMLDocument document;
    if (document.LoadFile(mdlFilePath.c_str()) != tinyxml2::XML_SUCCESS) {
        return Status::MdlParseError;
    }

    MDLReadResult result = {};
    result.modelName = modelName;
    result.sourceRelativePath = "Models/" + modelName + ".mdl";
    if (!read_faces(document, &result.model)) {
        return Status::MdlParseError;
    }

    vtkSmartPointer<vtkXMLPolyDataReader> reader =
        vtkSmartPointer<vtkXMLPolyDataReader>::New();
    reader->SetFileName(vtpFilePath.c_str());
    reader->Update();
    if (reader->GetErrorCode() != vtkErrorCode::NoError) {
        return Status::VtpReadError;
    }

    vtkPolyData* polyData = reader->GetOutput();
    if (polyData == 0 || polyData->GetNumberOfPoints() <= 0 || polyData->GetNumberOfCells() <= 0) {
        return Status::VtpReadError;
    }

    SurfaceGeometryHandle geometry = {};
    geometry.setCounts(static_cast<std::size_t>(polyData->GetNumberOfPoints()),
                       static_cast<std::size_t>(polyData->GetNumberOfCells()));
    result.model.setGeometry(std::make_shared<SurfaceGeometryHandle>(geometry));
    result.model.setPreservedArrays(read_preserved_arrays(polyData));
    result.model.setSource(ModelSource::Loaded);

    // Materialize the real triangle geometry from the .vtp so the render path can
    // mount an actor (it only does so for hasTriangleGeometry()); the counts-only
    // handle above stays for the existing metadata contract. No triangles ->
    // treat as an unreadable surface rather than a silent empty model.
    XQTriangleSurfaceGeometryHandle triangles;
    if (!build_triangle_geometry(polyData, &triangles)) {
        return Status::VtpReadError;
    }
    result.model.setTriangleGeometry(
        std::make_shared<XQTriangleSurfaceGeometryHandle>(std::move(triangles)));

    *out = result;
    return Status::Ok;
}

} // namespace xq
