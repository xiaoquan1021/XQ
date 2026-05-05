#pragma once

#include <xqMeshCommonExports.h>

#include <mitkBaseData.h>

#include <memory>
#include <vector>

class xq_Grid;
class xq_VascularGeometry;

class XQMESHCOMMON_EXPORT xq_MitkGrid : public mitk::BaseData
{
public:
    mitkClassMacro(xq_MitkGrid, mitk::BaseData);
    itkFactorylessNewMacro(Self);
    itkCloneMacro(Self);

    void UpdateOutputInformation() override;
    void SetRequestedRegionToLargestPossibleRegion() override;
    bool RequestedRegionIsOutsideOfTheBufferedRegion() override;
    bool VerifyRequestedRegion() override;
    void SetRequestedRegion(const itk::DataObject* data) override;

    void Expand(unsigned int timeSteps) override;
    void ExecuteOperation(mitk::Operation* operation) override;
    [[nodiscard]] bool IsEmptyTimeStep(unsigned int t) const override;

    [[nodiscard]] xq_Grid* GetMesh(unsigned int t = 0) const;
    void SetMesh(xq_Grid* mesh, unsigned int t = 0);

    [[nodiscard]] xq_VascularGeometry* GetModelElement() const;

    bool ApplySetMesh(xq_Grid* mesh, unsigned int t);
    bool ApplyGenerateMesh(unsigned int t);
    bool ApplyAdaptMesh(unsigned int t);

protected:
    xq_MitkGrid();
    xq_MitkGrid(const xq_MitkGrid& other);
    ~xq_MitkGrid() override;

    void ClearData() override;
    void InitializeEmpty() override;

private:
    std::vector<std::unique_ptr<xq_Grid>> m_MeshList;
};
