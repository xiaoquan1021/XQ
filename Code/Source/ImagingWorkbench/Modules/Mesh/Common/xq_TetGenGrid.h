#pragma once

#include <xqMeshCommonExports.h>

#include "xq_Grid.h"

#include <string>
#include <string_view>

class XQMESHCOMMON_EXPORT TetGenMeshHandle
{
public:
    TetGenMeshHandle() = default;
    ~TetGenMeshHandle() = default;
    TetGenMeshHandle(TetGenMeshHandle&&) noexcept = default;
    TetGenMeshHandle& operator=(TetGenMeshHandle&&) noexcept = default;
    TetGenMeshHandle(const TetGenMeshHandle&) = delete;
    TetGenMeshHandle& operator=(const TetGenMeshHandle&) = delete;

    void reset(vtkSmartPointer<vtkUnstructuredGrid> grid) { m_Grid = std::move(grid); }
    [[nodiscard]] vtkUnstructuredGrid* get() const { return m_Grid; }
    [[nodiscard]] explicit operator bool() const { return m_Grid != nullptr && m_Grid->GetNumberOfCells() > 0; }

private:
    vtkSmartPointer<vtkUnstructuredGrid> m_Grid;
};

class XQMESHCOMMON_EXPORT xq_TetGenGrid : public xq_Grid
{
public:
    xq_TetGenGrid();
    ~xq_TetGenGrid() override = default;

    bool GenerateMesh() override;
    bool AdaptMesh(std::string_view errorMetricArrayName) override;

    void SetQualityRatio(double ratio);
    [[nodiscard]] double GetQualityRatio() const;

    void SetVolumeConstraint(double constraint);
    [[nodiscard]] double GetVolumeConstraint() const;

    void SetCoarsenMesh(bool coarsen);
    [[nodiscard]] bool GetCoarsenMesh() const;

    void SetBoundaryLayerMesh(bool enable);
    [[nodiscard]] bool GetBoundaryLayerMesh() const;

    void SetBoundaryLayerCount(int count);
    [[nodiscard]] int GetBoundaryLayerCount() const;

    void SetBoundaryLayerGrowthRate(double rate);
    [[nodiscard]] double GetBoundaryLayerGrowthRate() const;

    // XQ fix: first-layer height was silently dropped by the pipeline — now
    // it is carried through to the TetGen invocation so -bR <h> is emitted.
    void SetBoundaryLayerFirstHeight(double h);
    [[nodiscard]] double GetBoundaryLayerFirstHeight() const;

private:
    double m_QualityRatio = 1.4;
    double m_VolumeConstraint = 0.0;
    bool m_CoarsenMesh = false;
    bool m_BoundaryLayerMesh = false;
    int m_BoundaryLayerCount = 2;
    double m_BoundaryLayerGrowthRate = 0.7;
    double m_BoundaryLayerFirstHeight = 0.1;
};
