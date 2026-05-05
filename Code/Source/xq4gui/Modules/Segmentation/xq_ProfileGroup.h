// XQ ProfileGroup: RAII-managed contour collection with unique_ptr ownership semantics
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkBaseData.h>

#include <vtkSmartPointer.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class xq_LumenProfile;
class vtkPolyData;

class XQMODULESEGMENTATION_EXPORT xq_ProfileGroup : public mitk::BaseData
{
public:
    mitkClassMacro(xq_ProfileGroup, mitk::BaseData)
    itkFactorylessNewMacro(Self)
    itkCloneMacro(Self)

    // Contour management — takes ownership of raw pointer via unique_ptr
    void AppendProfile(xq_LumenProfile* contour, int pathPosIndex, unsigned int timeStep = 0);
    void RemoveProfile(int pathPosIndex, unsigned int timeStep = 0);
    void ReplaceProfile(xq_LumenProfile* contour, int pathPosIndex, unsigned int timeStep = 0);

    // Convenience overload — uses contour's own GetPathPosIndex()
    void AppendProfile(xq_LumenProfile* contour);

    // ---------------------------------------------------------------------------
    // Canonical query API — use these in all new code
    // ---------------------------------------------------------------------------
    int GetProfileCount(unsigned int timeStep = 0) const;
    bool HasProfile(int pathPosIndex, unsigned int timeStep = 0) const;
    std::vector<int> GetProfilePathIndices(unsigned int timeStep = 0) const;
    std::vector<int> GetUnresolvedProfilePathIndices(unsigned int timeStep = 0) const;
    std::vector<int> GetMissingProfilePathIndices(unsigned int timeStep = 0) const;
    bool IsReadyForLoft(unsigned int timeStep = 0) const;
    xq_LumenProfile* GetProfileAtPathPos(int posIndex, unsigned int timeStep = 0) const;

    // ---------------------------------------------------------------------------
    // Deprecated aliases — kept for caller compatibility, will be removed in a later phase
    // ---------------------------------------------------------------------------
    [[deprecated("Use GetProfileCount() instead")]]
    int CountProfiles(unsigned int timeStep = 0) const;
    [[deprecated("Use GetProfileAtPathPos() instead")]]
    xq_LumenProfile* FetchProfile(int pathPosIndex, unsigned int timeStep = 0) const;

    // Loft cache dirty-state API
    //
    // A timestep's loft cache is "dirty" when profile data has been mutated
    // (via Append/Replace/Remove/Clear/Initialize) since the last SetLoftedMesh
    // or ClearLoftCacheDirty call.  Callers must regenerate the mesh before
    // using GetLoftedMesh() when IsLoftCacheDirty() returns true.
    bool IsLoftCacheDirty(unsigned int timeStep = 0) const;
    void MarkLoftCacheDirty(unsigned int timeStep = 0);
    void ClearLoftCacheDirty(unsigned int timeStep = 0);

    // Lofting surface from contour stack
    vtkSmartPointer<vtkPolyData> GetLoftedMesh(unsigned int timeStep = 0) const;
    void SetLoftedMesh(vtkSmartPointer<vtkPolyData> surface, unsigned int timeStep = 0);

    // Path ID this group is associated with
    void SetTrajectoryID(int id);
    int GetTrajectoryID() const;

    // Property management
    void SetAttribute(std::string_view key, const std::string& value);
    std::string GetAttribute(std::string_view key) const;
    std::map<std::string, std::string> GetAttributes() const;

    // BaseData overrides
    void Expand(unsigned int timeSteps) override;
    void ClearData() override;
    void InitializeEmpty() override;
    void SetRequestedRegionToLargestPossibleRegion() override;
    bool RequestedRegionIsOutsideOfTheBufferedRegion() override;
    bool VerifyRequestedRegion() override;
    void SetRequestedRegion(const itk::DataObject* data) override;

    // Undo/redo support
    void ExecuteOperation(mitk::Operation* operation) override;

    void UpdateOutputInformation() override;

protected:
    xq_ProfileGroup();
    xq_ProfileGroup(const xq_ProfileGroup& other);
    ~xq_ProfileGroup() override = default;
    itk::LightObject::Pointer InternalClone() const override;

    // Map: timeStep -> (pathPosIndex -> owned contour)
    using ProfileMap = std::map<int, std::unique_ptr<xq_LumenProfile>>;
    std::map<unsigned int, ProfileMap> m_ProfileSets;
    std::map<unsigned int, vtkSmartPointer<vtkPolyData>> m_LoftedMeshes;
    std::map<unsigned int, bool> m_LoftCacheDirty; // per-timestep dirty flag for loft cache
    int m_TrajectoryID = -1;
    std::map<std::string, std::string> m_Attributes;
};
