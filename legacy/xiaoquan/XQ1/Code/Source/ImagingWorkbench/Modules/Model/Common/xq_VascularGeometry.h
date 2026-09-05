#pragma once

#include <xqModelCommonExports.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>

struct XQMODELCOMMON_EXPORT FaceInfo
{
    int id = -1;
    std::string name;
    std::string type = "wall";
    bool visible = true;
    float opacity = 1.0f;
    std::array<float, 3> color = {1.0f, 1.0f, 1.0f};
    bool selected = false;
};

struct XQMODELCOMMON_EXPORT BlendRadiusEntry
{
    int faceId1 = -1;
    int faceId2 = -1;
    double radius = 0.0;
};

class XQMODELCOMMON_EXPORT xq_VascularGeometry
{
public:
    virtual ~xq_VascularGeometry();

    virtual std::unique_ptr<xq_VascularGeometry> Clone() const = 0;

    virtual vtkSmartPointer<vtkPolyData> GetWholeVtkPolyData() = 0;
    virtual vtkSmartPointer<vtkPolyData> GetFaceVtkPolyData(int faceId) = 0;

    [[nodiscard]] int GetFaceNumber() const;
    [[nodiscard]] const FaceInfo* GetFaceInfo(int id) const;
    void SetFaceInfo(int id, const FaceInfo& info);
    [[nodiscard]] const std::vector<FaceInfo>& GetAllFaceInfos() const;
    [[nodiscard]] std::string GetType() const;

    [[nodiscard]] std::vector<std::string> GetFaceNames() const;
    [[nodiscard]] int GetFaceIndex(int faceId) const;
    [[nodiscard]] int GetFaceIdByName(const std::string& name) const;
    void SelectFace(int faceId);
    void DeselectAllFaces();
    [[nodiscard]] bool IsFaceSelected(int faceId) const;
    [[nodiscard]] std::vector<int> GetWallFaceIDs() const;
    [[nodiscard]] std::vector<int> GetCapFaceIDs() const;
    void RemoveFace(int faceId);
    [[nodiscard]] double GetFaceArea(int faceId);

    void SetBlendRadii(const std::vector<BlendRadiusEntry>& radii);
    [[nodiscard]] const std::vector<BlendRadiusEntry>& GetBlendRadii() const;
    void AddBlendRadius(int faceId1, int faceId2, double radius);
    void ClearBlendRadii();

    // Persist face metadata in vtkPolyData field data so save/load preserves
    // face names, types, colors, and visibility across VTP round-trips.
    void EmbedFaceInfoToPolyData(vtkPolyData* pd);
    void RestoreFaceInfoFromPolyData(vtkPolyData* pd);

private:
    void SyncSelectionToCellData();

protected:
    xq_VascularGeometry();

    void setTypeTag(std::string_view tag);
    std::vector<FaceInfo>& facesMut();
    void clearFaces();

private:
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
