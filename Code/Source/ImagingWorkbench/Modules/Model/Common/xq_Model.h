#pragma once

#include <xqModelCommonExports.h>

#include "xq_VascularGeometry.h"

#include <mitkBaseData.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class XQMODELCOMMON_EXPORT xq_Model : public mitk::BaseData
{
public:
    mitkClassMacro(xq_Model, mitk::BaseData)
    itkFactorylessNewMacro(Self)
    itkCloneMacro(Self)

    // BaseData overrides
    void UpdateOutputInformation() override;
    void SetRequestedRegionToLargestPossibleRegion() override;
    bool RequestedRegionIsOutsideOfTheBufferedRegion() override;
    bool VerifyRequestedRegion() override;
    void SetRequestedRegion(const itk::DataObject* data) override;
    void Expand(unsigned int timeSteps) override;
    void ExecuteOperation(mitk::Operation* operation) override;

    // Model methods
    [[nodiscard]] xq_VascularGeometry* GetModelElement(unsigned int t = 0) const;
    void SetModelElement(std::unique_ptr<xq_VascularGeometry> element, unsigned int t = 0);
    void SetModelElement(xq_VascularGeometry* element, unsigned int t = 0);

    [[nodiscard]] std::string GetType() const;
    void SetType(std::string_view type);

    void SetProperty(std::string_view key, std::string_view value);
    [[nodiscard]] std::string GetProperty(const std::string& key) const;

protected:
    xq_Model();
    xq_Model(const xq_Model& other);
    ~xq_Model() override;

    [[nodiscard]] bool IsEmptyTimeStep(unsigned int t) const override;
    void ClearData() override;
    void InitializeEmpty() override;

private:
    std::vector<std::unique_ptr<xq_VascularGeometry>> m_ModelElements;
    std::string m_Type = "PolyData";
    std::map<std::string, std::string> m_Properties;
    bool m_DataModified = false;
    bool m_CalculateBoundingBox = true;
};
