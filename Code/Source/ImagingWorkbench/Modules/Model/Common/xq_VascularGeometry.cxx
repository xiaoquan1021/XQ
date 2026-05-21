#include "xq_VascularGeometry.h"

#include <algorithm>
#include <cmath>
#include <set>

#include <vtkCell.h>
#include <vtkCellData.h>
#include <vtkFieldData.h>
#include <vtkIntArray.h>
#include <vtkStringArray.h>

struct xq_VascularGeometry::Impl
{
    std::vector<FaceInfo> faces;
    std::string type = "PolyData";
    std::vector<BlendRadiusEntry> blendRadii;
};

xq_VascularGeometry::xq_VascularGeometry()
    : m_pImpl(std::make_unique<Impl>())
{
}

xq_VascularGeometry::~xq_VascularGeometry() = default;

int xq_VascularGeometry::GetFaceNumber() const
{
    return static_cast<int>(m_pImpl->faces.size());
}

const FaceInfo* xq_VascularGeometry::GetFaceInfo(int id) const
{
    auto it = std::find_if(m_pImpl->faces.cbegin(), m_pImpl->faces.cend(),
        [id](const FaceInfo& f) { return f.id == id; });
    return it != m_pImpl->faces.cend() ? &(*it) : nullptr;
}

void xq_VascularGeometry::SetFaceInfo(int id, const FaceInfo& info)
{
    auto it = std::find_if(m_pImpl->faces.begin(), m_pImpl->faces.end(),
        [id](const FaceInfo& f) { return f.id == id; });
    if (it != m_pImpl->faces.end())
    {
        *it = info;
    }
    else
    {
        m_pImpl->faces.push_back(info);
    }
}

const std::vector<FaceInfo>& xq_VascularGeometry::GetAllFaceInfos() const
{
    return m_pImpl->faces;
}

std::string xq_VascularGeometry::GetType() const
{
    return m_pImpl->type;
}

void xq_VascularGeometry::setTypeTag(std::string_view tag)
{
    m_pImpl->type = std::string(tag);
}

std::vector<FaceInfo>& xq_VascularGeometry::facesMut()
{
    return m_pImpl->faces;
}

void xq_VascularGeometry::clearFaces()
{
    m_pImpl->faces.clear();
}

std::vector<std::string> xq_VascularGeometry::GetFaceNames() const
{
    std::vector<std::string> names;
    names.reserve(m_pImpl->faces.size());
    for (const auto& f : m_pImpl->faces)
    {
        names.push_back(f.name);
    }
    return names;
}

int xq_VascularGeometry::GetFaceIndex(int faceId) const
{
    const auto& faces = m_pImpl->faces;
    auto it = std::find_if(faces.cbegin(), faces.cend(),
        [faceId](const FaceInfo& f) { return f.id == faceId; });
    return it != faces.cend() ? static_cast<int>(std::distance(faces.cbegin(), it)) : -1;
}

int xq_VascularGeometry::GetFaceIdByName(const std::string& name) const
{
    auto it = std::find_if(m_pImpl->faces.cbegin(), m_pImpl->faces.cend(),
        [&name](const FaceInfo& f) { return f.name == name; });
    return it != m_pImpl->faces.cend() ? it->id : -1;
}

void xq_VascularGeometry::SelectFace(int faceId)
{
    auto it = std::find_if(m_pImpl->faces.begin(), m_pImpl->faces.end(),
        [faceId](const FaceInfo& f) { return f.id == faceId; });
    if (it != m_pImpl->faces.end())
    {
        it->selected = true;
    }
    SyncSelectionToCellData();
}

void xq_VascularGeometry::DeselectAllFaces()
{
    for (auto& f : m_pImpl->faces)
    {
        f.selected = false;
    }
    SyncSelectionToCellData();
}

bool xq_VascularGeometry::IsFaceSelected(int faceId) const
{
    auto it = std::find_if(m_pImpl->faces.cbegin(), m_pImpl->faces.cend(),
        [faceId](const FaceInfo& f) { return f.id == faceId; });
    return it != m_pImpl->faces.cend() && it->selected;
}

std::vector<int> xq_VascularGeometry::GetWallFaceIDs() const
{
    std::vector<int> ids;
    for (const auto& f : m_pImpl->faces)
    {
        if (f.type == "wall")
        {
            ids.push_back(f.id);
        }
    }
    return ids;
}

std::vector<int> xq_VascularGeometry::GetCapFaceIDs() const
{
    std::vector<int> ids;
    for (const auto& f : m_pImpl->faces)
    {
        if (f.type == "cap" || f.type == "inlet" || f.type == "outlet")
        {
            ids.push_back(f.id);
        }
    }
    return ids;
}

void xq_VascularGeometry::RemoveFace(int faceId)
{
    auto& faces = m_pImpl->faces;
    faces.erase(
        std::remove_if(faces.begin(), faces.end(),
            [faceId](const FaceInfo& f) { return f.id == faceId; }),
        faces.end());

    // Update the underlying vtkPolyData: mark removed cells with faceId = -1
    auto polyData = GetWholeVtkPolyData();
    if (polyData)
    {
        vtkCellData* cellData = polyData->GetCellData();
        if (cellData)
        {
            vtkDataArray* faceIds = cellData->GetArray("FaceIds");
            if (faceIds)
            {
                for (vtkIdType i = 0; i < faceIds->GetNumberOfTuples(); ++i)
                {
                    if (static_cast<int>(faceIds->GetTuple1(i)) == faceId)
                    {
                        faceIds->SetTuple1(i, -1);
                    }
                }
                polyData->Modified();
            }
        }
    }
}

double xq_VascularGeometry::GetFaceArea(int faceId)
{
    auto polyData = GetFaceVtkPolyData(faceId);
    if (!polyData || polyData->GetNumberOfCells() == 0)
    {
        return 0.0;
    }

    double totalArea = 0.0;
    for (vtkIdType i = 0; i < polyData->GetNumberOfCells(); ++i)
    {
        vtkCell* cell = polyData->GetCell(i);
        if (cell && cell->GetNumberOfPoints() == 3)
        {
            double p0[3], p1[3], p2[3];
            polyData->GetPoint(cell->GetPointId(0), p0);
            polyData->GetPoint(cell->GetPointId(1), p1);
            polyData->GetPoint(cell->GetPointId(2), p2);

            double v1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
            double v2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};

            double cross[3] = {
                v1[1] * v2[2] - v1[2] * v2[1],
                v1[2] * v2[0] - v1[0] * v2[2],
                v1[0] * v2[1] - v1[1] * v2[0]
            };

            totalArea += 0.5 * std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
        }
    }
    return totalArea;
}

void xq_VascularGeometry::SetBlendRadii(const std::vector<BlendRadiusEntry>& radii)
{
    m_pImpl->blendRadii = radii;
}

const std::vector<BlendRadiusEntry>& xq_VascularGeometry::GetBlendRadii() const
{
    return m_pImpl->blendRadii;
}

void xq_VascularGeometry::AddBlendRadius(int faceId1, int faceId2, double radius)
{
    m_pImpl->blendRadii.push_back({faceId1, faceId2, radius});
}

void xq_VascularGeometry::ClearBlendRadii()
{
    m_pImpl->blendRadii.clear();
}

void xq_VascularGeometry::SyncSelectionToCellData()
{
    auto polyData = GetWholeVtkPolyData();
    if (!polyData)
        return;

    vtkCellData* cellData = polyData->GetCellData();
    if (!cellData)
        return;

    vtkDataArray* faceIdsArray = cellData->GetArray("FaceIds");
    if (!faceIdsArray)
        return;

    vtkIdType numCells = faceIdsArray->GetNumberOfTuples();

    vtkSmartPointer<vtkIntArray> selArray =
        vtkIntArray::SafeDownCast(cellData->GetArray("SelectedFaces"));
    if (!selArray)
    {
        selArray = vtkSmartPointer<vtkIntArray>::New();
        selArray->SetName("SelectedFaces");
        selArray->SetNumberOfComponents(1);
        selArray->SetNumberOfTuples(numCells);
        cellData->AddArray(selArray);
    }
    else if (selArray->GetNumberOfTuples() != numCells)
    {
        selArray->SetNumberOfTuples(numCells);
    }

    // Build a quick lookup of selected face IDs
    std::set<int> selectedIds;
    for (const auto& f : m_pImpl->faces)
    {
        if (f.selected)
            selectedIds.insert(f.id);
    }

    for (vtkIdType i = 0; i < numCells; ++i)
    {
        int fid = static_cast<int>(faceIdsArray->GetTuple1(i));
        selArray->SetValue(i, selectedIds.count(fid) ? 1 : 0);
    }

    polyData->Modified();
}

void xq_VascularGeometry::EmbedFaceInfoToPolyData(vtkPolyData* pd)
{
    if (!pd)
        return;

    vtkFieldData* fd = pd->GetFieldData();
    if (!fd)
        return;

    const auto& faces = m_pImpl->faces;
    const vtkIdType n = static_cast<vtkIdType>(faces.size());

    auto faceIdArr = vtkSmartPointer<vtkIntArray>::New();
    faceIdArr->SetName("XQ_FaceIds");
    faceIdArr->SetNumberOfComponents(1);
    faceIdArr->SetNumberOfTuples(n);

    auto nameArr = vtkSmartPointer<vtkStringArray>::New();
    nameArr->SetName("XQ_FaceNames");
    nameArr->SetNumberOfTuples(n);

    auto typeArr = vtkSmartPointer<vtkStringArray>::New();
    typeArr->SetName("XQ_FaceTypes");
    typeArr->SetNumberOfTuples(n);

    auto visibleArr = vtkSmartPointer<vtkIntArray>::New();
    visibleArr->SetName("XQ_FaceVisible");
    visibleArr->SetNumberOfComponents(1);
    visibleArr->SetNumberOfTuples(n);

    auto colorArr = vtkSmartPointer<vtkIntArray>::New();
    colorArr->SetName("XQ_FaceColor");
    colorArr->SetNumberOfComponents(3);
    colorArr->SetNumberOfTuples(n);

    for (vtkIdType i = 0; i < n; ++i)
    {
        const auto& f = faces[static_cast<size_t>(i)];
        faceIdArr->SetValue(i, f.id);
        nameArr->SetValue(i, f.name);
        typeArr->SetValue(i, f.type);
        visibleArr->SetValue(i, f.visible ? 1 : 0);
        colorArr->SetComponent(i, 0, static_cast<int>(f.color[0] * 255));
        colorArr->SetComponent(i, 1, static_cast<int>(f.color[1] * 255));
        colorArr->SetComponent(i, 2, static_cast<int>(f.color[2] * 255));
    }

    fd->AddArray(faceIdArr);
    fd->AddArray(nameArr);
    fd->AddArray(typeArr);
    fd->AddArray(visibleArr);
    fd->AddArray(colorArr);
    pd->Modified();
}

void xq_VascularGeometry::RestoreFaceInfoFromPolyData(vtkPolyData* pd)
{
    if (!pd)
        return;

    vtkFieldData* fd = pd->GetFieldData();
    if (!fd)
        return;

    auto* faceIdArr = vtkIntArray::SafeDownCast(fd->GetArray("XQ_FaceIds"));
    auto* nameArr = vtkStringArray::SafeDownCast(fd->GetArray("XQ_FaceNames"));
    auto* typeArr = vtkStringArray::SafeDownCast(fd->GetArray("XQ_FaceTypes"));
    auto* visibleArr = vtkIntArray::SafeDownCast(fd->GetArray("XQ_FaceVisible"));
    auto* colorArr = vtkIntArray::SafeDownCast(fd->GetArray("XQ_FaceColor"));

    if (!faceIdArr || !typeArr)
        return; // No embedded face metadata — keep existing faces

    m_pImpl->faces.clear();
    const vtkIdType n = faceIdArr->GetNumberOfTuples();
    for (vtkIdType i = 0; i < n; ++i)
    {
        FaceInfo fi;
        fi.id = faceIdArr->GetValue(i);
        if (nameArr && i < nameArr->GetNumberOfTuples())
            fi.name = nameArr->GetValue(i);
        if (typeArr && i < typeArr->GetNumberOfTuples())
            fi.type = typeArr->GetValue(i);
        else
            fi.type = "wall";
        if (visibleArr && i < visibleArr->GetNumberOfTuples())
            fi.visible = visibleArr->GetValue(i) != 0;
        if (colorArr && i < colorArr->GetNumberOfTuples())
        {
            fi.color[0] = static_cast<float>(colorArr->GetComponent(i, 0)) / 255.0f;
            fi.color[1] = static_cast<float>(colorArr->GetComponent(i, 1)) / 255.0f;
            fi.color[2] = static_cast<float>(colorArr->GetComponent(i, 2)) / 255.0f;
        }
        m_pImpl->faces.push_back(fi);
    }
}
