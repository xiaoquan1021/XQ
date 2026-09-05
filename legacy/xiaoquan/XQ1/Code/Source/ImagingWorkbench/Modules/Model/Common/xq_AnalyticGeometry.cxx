#include "xq_AnalyticGeometry.h"

#include <vtkCylinderSource.h>
#include <vtkSphereSource.h>
#include <vtkCubeSource.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkIntArray.h>
#include <vtkCellData.h>
#include <vtkCellArray.h>
#include <vtkCell.h>
#include <vtkMath.h>

#include <cmath>

namespace
{

constexpr int kCylinderResolution = 32;
constexpr int kSphereResolution   = 32;
constexpr double kEpsilon         = 1e-10;

constexpr std::array<const char*, 6> kBoxFaceNames = {
    "face_x_pos", "face_x_neg", "face_y_pos",
    "face_y_neg", "face_z_pos", "face_z_neg"
};

vtkSmartPointer<vtkIntArray> makeFaceIdArray(vtkIdType numCells)
{
    auto ids = vtkSmartPointer<vtkIntArray>::New();
    ids->SetName("FaceIds");
    ids->SetNumberOfTuples(numCells);
    return ids;
}

} // anonymous namespace

xq_AnalyticGeometry::xq_AnalyticGeometry()
{
    setTypeTag("Analytic");
    m_PolyData = vtkSmartPointer<vtkPolyData>::New();
}

std::unique_ptr<xq_VascularGeometry> xq_AnalyticGeometry::Clone() const
{
    auto clone = std::make_unique<xq_AnalyticGeometry>();
    clone->m_ShapeType = m_ShapeType;
    clone->m_Params = m_Params;
    if (m_PolyData)
    {
        clone->m_PolyData = vtkSmartPointer<vtkPolyData>::New();
        clone->m_PolyData->DeepCopy(m_PolyData);
    }
    for (const auto& fi : GetAllFaceInfos())
        clone->SetFaceInfo(fi.id, fi);
    return clone;
}

void xq_AnalyticGeometry::CreateCylinder(double center[3], double axis[3],
                                          double radius, double length)
{
    m_ShapeType = Cylinder;
    m_Params[0] = radius;
    m_Params[1] = length;

    auto cylinderSource = vtkSmartPointer<vtkCylinderSource>::New();
    cylinderSource->SetRadius(radius);
    cylinderSource->SetHeight(length);
    cylinderSource->SetResolution(kCylinderResolution);
    cylinderSource->CappingOn();
    cylinderSource->Update();

    // Align the cylinder (default Y-axis) with the desired axis
    constexpr std::array<double, 3> defaultAxis = {0.0, 1.0, 0.0};
    const double norm = vtkMath::Normalize(axis);
    if (norm < kEpsilon)
    {
        axis[0] = 0.0; axis[1] = 1.0; axis[2] = 0.0;
    }

    double crossVec[3];
    vtkMath::Cross(defaultAxis.data(), axis, crossVec);
    const double sinAngle = vtkMath::Norm(crossVec);
    const double cosAngle = vtkMath::Dot(defaultAxis.data(), axis);
    const double angle = std::atan2(sinAngle, cosAngle) * 180.0 / vtkMath::Pi();

    auto transform = vtkSmartPointer<vtkTransform>::New();
    transform->Translate(center[0], center[1], center[2]);
    if (sinAngle > kEpsilon)
    {
        transform->RotateWXYZ(angle, crossVec[0], crossVec[1], crossVec[2]);
    }

    auto transformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
    transformFilter->SetInputConnection(cylinderSource->GetOutputPort());
    transformFilter->SetTransform(transform);
    transformFilter->Update();

    m_PolyData = vtkSmartPointer<vtkPolyData>::New();
    m_PolyData->DeepCopy(transformFilter->GetOutput());

    const vtkIdType numCells = m_PolyData->GetNumberOfCells();
    auto faceIds = makeFaceIdArray(numCells);
    const vtkIdType wallCells = numCells - 2;
    for (vtkIdType i = 0; i < numCells; ++i)
    {
        if (i < wallCells)
            faceIds->SetValue(i, 1);
        else if (i == wallCells)
            faceIds->SetValue(i, 2);
        else
            faceIds->SetValue(i, 3);
    }
    m_PolyData->GetCellData()->AddArray(faceIds);

    clearFaces();
    auto& faces = facesMut();
    faces.push_back({1, "wall", "wall", true, 1.0f, {1.0f, 1.0f, 1.0f}});
    faces.push_back({2, "cap1", "cap", true, 1.0f, {1.0f, 1.0f, 1.0f}});
    faces.push_back({3, "cap2", "cap", true, 1.0f, {1.0f, 1.0f, 1.0f}});
}

void xq_AnalyticGeometry::CreateSphere(double center[3], double radius)
{
    m_ShapeType = Sphere;
    m_Params[0] = radius;

    auto sphereSource = vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetCenter(center[0], center[1], center[2]);
    sphereSource->SetRadius(radius);
    sphereSource->SetThetaResolution(kSphereResolution);
    sphereSource->SetPhiResolution(kSphereResolution);
    sphereSource->Update();

    m_PolyData = vtkSmartPointer<vtkPolyData>::New();
    m_PolyData->DeepCopy(sphereSource->GetOutput());

    const vtkIdType numCells = m_PolyData->GetNumberOfCells();
    auto faceIds = makeFaceIdArray(numCells);
    for (vtkIdType i = 0; i < numCells; ++i)
    {
        faceIds->SetValue(i, 1);
    }
    m_PolyData->GetCellData()->AddArray(faceIds);

    clearFaces();
    facesMut().push_back({1, "sphere_surface", "wall", true, 1.0f, {1.0f, 1.0f, 1.0f}});
}

void xq_AnalyticGeometry::CreateBox(double center[3], double dims[3])
{
    m_ShapeType = Box;
    m_Params[0] = dims[0];
    m_Params[1] = dims[1];
    m_Params[2] = dims[2];

    auto cubeSource = vtkSmartPointer<vtkCubeSource>::New();
    cubeSource->SetCenter(center[0], center[1], center[2]);
    cubeSource->SetXLength(dims[0]);
    cubeSource->SetYLength(dims[1]);
    cubeSource->SetZLength(dims[2]);
    cubeSource->Update();

    m_PolyData = vtkSmartPointer<vtkPolyData>::New();
    m_PolyData->DeepCopy(cubeSource->GetOutput());

    const vtkIdType numCells = m_PolyData->GetNumberOfCells();
    auto faceIds = makeFaceIdArray(numCells);
    for (vtkIdType i = 0; i < numCells; ++i)
    {
        const int faceIndex = std::min(static_cast<int>(i / 2) + 1, 6);
        faceIds->SetValue(i, faceIndex);
    }
    m_PolyData->GetCellData()->AddArray(faceIds);

    clearFaces();
    auto& faces = facesMut();
    for (int i = 0; i < 6; ++i)
    {
        faces.push_back({i + 1, kBoxFaceNames[i], "wall", true, 1.0f, {1.0f, 1.0f, 1.0f}});
    }
}

vtkSmartPointer<vtkPolyData> xq_AnalyticGeometry::GetWholeVtkPolyData()
{
    return m_PolyData;
}

vtkSmartPointer<vtkPolyData> xq_AnalyticGeometry::GetFaceVtkPolyData(int faceId)
{
    if (!m_PolyData || m_PolyData->GetNumberOfCells() == 0)
        return nullptr;

    auto* faceIds = vtkIntArray::SafeDownCast(
        m_PolyData->GetCellData()->GetArray("FaceIds"));
    if (!faceIds)
        return nullptr;

    auto result = vtkSmartPointer<vtkPolyData>::New();
    result->SetPoints(m_PolyData->GetPoints());

    auto cells = vtkSmartPointer<vtkCellArray>::New();
    for (vtkIdType i = 0; i < m_PolyData->GetNumberOfCells(); ++i)
    {
        if (faceIds->GetValue(i) == faceId)
        {
            cells->InsertNextCell(m_PolyData->GetCell(i));
        }
    }
    result->SetPolys(cells);
    return result;
}
