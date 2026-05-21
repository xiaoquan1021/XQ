#pragma once

#include <xqMeshCommonExports.h>

#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPolyData.h>

#include <map>
#include <string>
#include <string_view>

class xq_VascularGeometry;

struct XQMESHCOMMON_EXPORT MeshParams
{
    double globalEdgeSize = 1.0;
    double globalMaxEdgeSize = 0.0;
    std::map<int, double> localEdgeSizes;
    bool surfaceMeshOnly = false;
    int optimizationPasses = 3;

    MeshParams() = default;
    ~MeshParams() = default;
    MeshParams(const MeshParams&) = default;
    MeshParams& operator=(const MeshParams&) = default;
    MeshParams(MeshParams&&) noexcept = default;
    MeshParams& operator=(MeshParams&&) noexcept = default;
};

class XQMESHCOMMON_EXPORT GridConfigBuilder
{
public:
    GridConfigBuilder& withGlobalEdgeSize(double size);
    GridConfigBuilder& withMaxEdgeSize(double size);
    GridConfigBuilder& withLocalEdgeSize(int faceId, double size);
    GridConfigBuilder& surfaceOnly(bool flag = true);
    GridConfigBuilder& withOptimizationPasses(int passes);
    [[nodiscard]] MeshParams build() const;

private:
    MeshParams m_Params;
};

class XQMESHCOMMON_EXPORT xq_Grid
{
public:
    virtual ~xq_Grid() = default;

    virtual bool GenerateMesh() = 0;
    virtual bool AdaptMesh(std::string_view errorMetricArrayName) = 0;

    void SetModelElement(xq_VascularGeometry* elem);
    [[nodiscard]] xq_VascularGeometry* GetModelElement() const;

    void SetMeshParams(const MeshParams& params);
    [[nodiscard]] const MeshParams& GetMeshParams() const;

    [[nodiscard]] vtkUnstructuredGrid* GetVolumeMesh() const;
    [[nodiscard]] vtkPolyData* GetSurfaceMesh() const;
    void SetVolumeMesh(vtkSmartPointer<vtkUnstructuredGrid> mesh);
    void SetSurfaceMesh(vtkSmartPointer<vtkPolyData> mesh);

    [[nodiscard]] std::string GetType() const;

    [[nodiscard]] int GetNumberOfNodes() const;
    [[nodiscard]] int GetNumberOfElements() const;

protected:
    xq_Grid();

    MeshParams m_Params;
    xq_VascularGeometry* m_ModelElement = nullptr;
    vtkSmartPointer<vtkUnstructuredGrid> m_VolumeMesh;
    vtkSmartPointer<vtkPolyData> m_SurfaceMesh;
    std::string m_Type;
};
