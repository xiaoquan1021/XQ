#include "xq_PolyGeometry.h"

#include <vtkCellData.h>
#include <vtkCellArray.h>
#include <vtkCell.h>

xq_PolyGeometry::xq_PolyGeometry()
{
    setTypeTag("PolyData");
    m_WholePolyData = vtkSmartPointer<vtkPolyData>::New();
}

std::unique_ptr<xq_VascularGeometry> xq_PolyGeometry::Clone() const
{
    auto clone = std::make_unique<xq_PolyGeometry>();
    if (m_WholePolyData)
    {
        clone->m_WholePolyData = vtkSmartPointer<vtkPolyData>::New();
        clone->m_WholePolyData->DeepCopy(m_WholePolyData);
    }
    for (const auto& fi : GetAllFaceInfos())
        clone->SetFaceInfo(fi.id, fi);
    return clone;
}

void xq_PolyGeometry::SetWholeVtkPolyData(vtkSmartPointer<vtkPolyData> polyData)
{
    if (!polyData)
        return;

    m_WholePolyData = vtkSmartPointer<vtkPolyData>::New();
    m_WholePolyData->DeepCopy(polyData);
}

void xq_PolyGeometry::AssignFaceIds(vtkIntArray* faceIds)
{
    if (!faceIds || !m_WholePolyData)
        return;

    faceIds->SetName("FaceIds");
    m_WholePolyData->GetCellData()->AddArray(faceIds);
}

vtkSmartPointer<vtkPolyData> xq_PolyGeometry::GetWholeVtkPolyData()
{
    return m_WholePolyData;
}

vtkSmartPointer<vtkPolyData> xq_PolyGeometry::GetFaceVtkPolyData(int faceId)
{
    return ExtractFace(faceId);
}

vtkSmartPointer<vtkPolyData> xq_PolyGeometry::ExtractFace(int faceId)
{
    if (!m_WholePolyData || m_WholePolyData->GetNumberOfCells() == 0)
        return nullptr;

    auto* faceIds = vtkIntArray::SafeDownCast(
        m_WholePolyData->GetCellData()->GetArray("FaceIds"));
    if (!faceIds)
        return nullptr;

    auto result = vtkSmartPointer<vtkPolyData>::New();
    result->SetPoints(m_WholePolyData->GetPoints());

    auto cells = vtkSmartPointer<vtkCellArray>::New();
    for (vtkIdType i = 0; i < m_WholePolyData->GetNumberOfCells(); ++i)
    {
        if (faceIds->GetValue(i) == faceId)
        {
            cells->InsertNextCell(m_WholePolyData->GetCell(i));
        }
    }
    result->SetPolys(cells);
    return result;
}
