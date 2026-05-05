#include "xq_ProfileGroup.h"
#include "xq_LumenProfile.h"
#include "xq_ProfileOp.h"

#include <mitkOperation.h>
#include <mitkProportionalTimeGeometry.h>

#include <vtkPolyData.h>

#include <algorithm>
#include <iterator>

xq_ProfileGroup::xq_ProfileGroup()
{
    // XQ fix: MITK's VtkPropRenderer needs a valid TimeGeometry to compute
    // the node's bounding box; without it the node is silently culled from
    // rendering. mitk::PointSet calls Superclass::InitializeTimeGeometry(1)
    // for exactly this reason in its ctor — do the same here so
    // contour-group nodes participate in the renderer walk.
    Superclass::InitializeTimeGeometry(1);
}

xq_ProfileGroup::xq_ProfileGroup(const xq_ProfileGroup& other)
    : mitk::BaseData(other)
    , m_TrajectoryID(other.m_TrajectoryID)
    , m_Attributes(other.m_Attributes)
    , m_LoftCacheDirty(other.m_LoftCacheDirty)
{
    for (const auto& [timeStep, contourMap] : other.m_ProfileSets) {
        for (const auto& [posIdx, contourPtr] : contourMap) {
            if (contourPtr) {
                m_ProfileSets[timeStep][posIdx] = contourPtr->Duplicate();
            }
        }
    }

    for (const auto& [timeStep, surface] : other.m_LoftedMeshes) {
        if (surface && !other.IsLoftCacheDirty(timeStep)) {
            auto copy = vtkSmartPointer<vtkPolyData>::New();
            copy->DeepCopy(surface);
            m_LoftedMeshes[timeStep] = copy;
        }
    }
}

itk::LightObject::Pointer xq_ProfileGroup::InternalClone() const
{
    Pointer smartPtr = new Self(*this);
    smartPtr->UnRegister();
    return smartPtr.GetPointer();
}

void xq_ProfileGroup::AppendProfile(xq_LumenProfile* contour, int pathPosIndex, unsigned int timeStep)
{
    if (!contour) {
        return;
    }
    // Keep the contour's own path-pos index in sync with its key in the map.
    // The map key is the canonical position; contour->GetPathPosIndex() must agree.
    contour->SetPathPosIndex(pathPosIndex);
    m_ProfileSets[timeStep][pathPosIndex] = std::unique_ptr<xq_LumenProfile>(contour);
    MarkLoftCacheDirty(timeStep);
    Modified();
}

void xq_ProfileGroup::RemoveProfile(int pathPosIndex, unsigned int timeStep)
{
    auto tsIt = m_ProfileSets.find(timeStep);
    if (tsIt == m_ProfileSets.end()) {
        return;
    }

    if (auto cIt = tsIt->second.find(pathPosIndex); cIt != tsIt->second.end()) {
        tsIt->second.erase(cIt);
        MarkLoftCacheDirty(timeStep);
        Modified();
    }
}

void xq_ProfileGroup::ReplaceProfile(xq_LumenProfile* contour, int pathPosIndex, unsigned int timeStep)
{
    if (!contour) {
        return;
    }
    contour->SetPathPosIndex(pathPosIndex);
    m_ProfileSets[timeStep][pathPosIndex] = std::unique_ptr<xq_LumenProfile>(contour);
    MarkLoftCacheDirty(timeStep);
    Modified();
}

xq_LumenProfile* xq_ProfileGroup::FetchProfile(int pathPosIndex, unsigned int timeStep) const
{
    auto tsIt = m_ProfileSets.find(timeStep);
    if (tsIt == m_ProfileSets.end()) {
        return nullptr;
    }
    auto cIt = tsIt->second.find(pathPosIndex);
    if (cIt == tsIt->second.end()) {
        return nullptr;
    }
    return cIt->second.get();
}

int xq_ProfileGroup::CountProfiles(unsigned int timeStep) const
{
    auto tsIt = m_ProfileSets.find(timeStep);
    if (tsIt == m_ProfileSets.end()) {
        return 0;
    }
    return static_cast<int>(tsIt->second.size());
}

bool xq_ProfileGroup::HasProfile(int pathPosIndex, unsigned int timeStep) const
{
    auto tsIt = m_ProfileSets.find(timeStep);
    if (tsIt == m_ProfileSets.end()) {
        return false;
    }
    return tsIt->second.count(pathPosIndex) > 0;
}

std::vector<int> xq_ProfileGroup::GetProfilePathIndices(unsigned int timeStep) const
{
    auto tsIt = m_ProfileSets.find(timeStep);
    if (tsIt == m_ProfileSets.end()) {
        return {};
    }

    std::vector<int> indices;
    indices.reserve(tsIt->second.size());
    std::transform(tsIt->second.begin(), tsIt->second.end(),
                   std::back_inserter(indices),
                   [](const auto& pair) { return pair.first; });
    // std::map is ordered, so indices are already sorted
    return indices;
}

std::vector<int> xq_ProfileGroup::GetUnresolvedProfilePathIndices(unsigned int timeStep) const
{
    auto tsIt = m_ProfileSets.find(timeStep);
    if (tsIt == m_ProfileSets.end()) {
        return {};
    }

    const bool requiresPathPlacement = !GetAttribute("path_name").empty();
    std::vector<int> unresolvedIndices;
    unresolvedIndices.reserve(tsIt->second.size());
    for (const auto& [pathPosIndex, profile] : tsIt->second) {
        if (!profile) {
            unresolvedIndices.push_back(pathPosIndex);
            continue;
        }

        if (profile->GetPathPosIndex() != pathPosIndex || profile->GetPathPosIndex() < 0) {
            unresolvedIndices.push_back(pathPosIndex);
            continue;
        }

        if (requiresPathPlacement && profile->GetSlicePlane().IsNull()) {
            unresolvedIndices.push_back(pathPosIndex);
        }
    }

    return unresolvedIndices;
}

std::vector<int> xq_ProfileGroup::GetMissingProfilePathIndices(unsigned int timeStep) const
{
    if (GetAttribute("path_name").empty()) {
        return {};
    }

    const auto indices = GetProfilePathIndices(timeStep);
    if (indices.size() < 2) {
        return {};
    }

    std::vector<int> missingIndices;
    for (size_t i = 1; i < indices.size(); ++i) {
        for (int missing = indices[i - 1] + 1; missing < indices[i]; ++missing) {
            missingIndices.push_back(missing);
        }
    }

    return missingIndices;
}

bool xq_ProfileGroup::IsReadyForLoft(unsigned int timeStep) const
{
    return GetProfileCount(timeStep) >= 2 &&
           GetUnresolvedProfilePathIndices(timeStep).empty() &&
           GetMissingProfilePathIndices(timeStep).empty();
}

xq_LumenProfile* xq_ProfileGroup::GetProfileAtPathPos(int posIndex, unsigned int timeStep) const
{
    return FetchProfile(posIndex, timeStep);
}

vtkSmartPointer<vtkPolyData> xq_ProfileGroup::GetLoftedMesh(unsigned int timeStep) const
{
    // Refuse to return stale data: if the cache is dirty the caller must
    // regenerate the mesh first via SetLoftedMesh().
    if (IsLoftCacheDirty(timeStep)) {
        return nullptr;
    }
    auto it = m_LoftedMeshes.find(timeStep);
    if (it == m_LoftedMeshes.end()) {
        return nullptr;
    }
    return it->second;
}

void xq_ProfileGroup::SetLoftedMesh(vtkSmartPointer<vtkPolyData> surface, unsigned int timeStep)
{
    m_LoftedMeshes[timeStep] = surface;
    ClearLoftCacheDirty(timeStep);
    Modified();
}

void xq_ProfileGroup::SetTrajectoryID(int id)
{
    m_TrajectoryID = id;
}

int xq_ProfileGroup::GetTrajectoryID() const
{
    return m_TrajectoryID;
}

void xq_ProfileGroup::SetRequestedRegionToLargestPossibleRegion()
{
}

bool xq_ProfileGroup::RequestedRegionIsOutsideOfTheBufferedRegion()
{
    return false;
}

bool xq_ProfileGroup::VerifyRequestedRegion()
{
    return true;
}

void xq_ProfileGroup::SetRequestedRegion(const itk::DataObject* /*data*/)
{
}

void xq_ProfileGroup::ExecuteOperation(mitk::Operation* operation)
{
    if (!operation) {
        return;
    }

    auto* contourOp = dynamic_cast<xq_ProfileOp*>(operation);
    if (!contourOp) {
        return;
    }

    switch (operation->GetOperationType())
    {
    case OpINSERTPROFILE:
        AppendProfile(contourOp->GetProfile(), contourOp->GetPathPosIndex(), contourOp->GetTemporalIndex());
        break;
    case OpREMOVEPROFILE:
        RemoveProfile(contourOp->GetPathPosIndex(), contourOp->GetTemporalIndex());
        break;
    case OpSETPROFILE:
        ReplaceProfile(contourOp->GetProfile(), contourOp->GetPathPosIndex(), contourOp->GetTemporalIndex());
        break;
    }
}

void xq_ProfileGroup::UpdateOutputInformation()
{
    if (GetSource()) {
        GetSource()->UpdateOutputInformation();
    }

    if (GetTimeGeometry()) {
        GetTimeGeometry()->Update();
    }
}

void xq_ProfileGroup::AppendProfile(xq_LumenProfile* contour)
{
    if (!contour)
        return;

    AppendProfile(contour, contour->GetPathPosIndex(), 0);
}

int xq_ProfileGroup::GetProfileCount(unsigned int timeStep) const
{
    auto it = m_ProfileSets.find(timeStep);
    if (it == m_ProfileSets.end()) return 0;
    return static_cast<int>(it->second.size());
}

void xq_ProfileGroup::SetAttribute(std::string_view key, const std::string& value)
{
    m_Attributes[std::string(key)] = value;
}

std::string xq_ProfileGroup::GetAttribute(std::string_view key) const
{
    // Returns empty string for missing keys for API compatibility
    auto it = m_Attributes.find(std::string(key));
    return (it != m_Attributes.end()) ? it->second : "";
}

std::map<std::string, std::string> xq_ProfileGroup::GetAttributes() const
{
    return m_Attributes;
}

void xq_ProfileGroup::Expand(unsigned int timeSteps)
{
    for (unsigned int t = 0; t < timeSteps; ++t) {
        if (m_ProfileSets.find(t) == m_ProfileSets.end()) {
            m_ProfileSets[t] = ProfileMap{};
        }
    }
    Superclass::Expand(timeSteps);
}

void xq_ProfileGroup::ClearData()
{
    // Mark all tracked timesteps dirty before clearing: any surviving loft mesh
    // would be stale relative to the (now empty) profile set.
    for (auto& [ts, _] : m_ProfileSets) {
        MarkLoftCacheDirty(ts);
    }
    m_ProfileSets.clear();
    m_LoftedMeshes.clear();
    m_Attributes.clear();
}

void xq_ProfileGroup::InitializeEmpty()
{
    ClearData();
    m_ProfileSets[0] = ProfileMap{};
    MarkLoftCacheDirty(0);
    Superclass::InitializeEmpty();
}

// ---------------------------------------------------------------------------
// Loft cache dirty-state implementation
// ---------------------------------------------------------------------------

bool xq_ProfileGroup::IsLoftCacheDirty(unsigned int timeStep) const
{
    auto it = m_LoftCacheDirty.find(timeStep);
    // Absent entry means no mutation has been recorded for this timestep → not dirty.
    return (it != m_LoftCacheDirty.end()) && it->second;
}

void xq_ProfileGroup::MarkLoftCacheDirty(unsigned int timeStep)
{
    m_LoftCacheDirty[timeStep] = true;
    m_LoftedMeshes.erase(timeStep);
}

void xq_ProfileGroup::ClearLoftCacheDirty(unsigned int timeStep)
{
    m_LoftCacheDirty[timeStep] = false;
}
