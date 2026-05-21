#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkBaseData.h>
#include <mitkPoint.h>

#include <vector>
#include <string>

struct ContourSlice
{
    double slicePosition = 0.0;  // position along path
    std::vector<mitk::Point3D> points;
    bool isClosed = true;
    std::string method = "manual";  // manual, threshold, regiongrow
};

class XQMODULESEGMENTATION_EXPORT xq_ContourGroup : public mitk::BaseData
{
public:
    mitkClassMacro(xq_ContourGroup, mitk::BaseData)
    itkFactorylessNewMacro(Self)
    itkCloneMacro(Self)

    void UpdateOutputInformation() override;
    void SetRequestedRegionToLargestPossibleRegion() override;
    bool RequestedRegionIsOutsideOfTheBufferedRegion() override;
    bool VerifyRequestedRegion() override;
    void SetRequestedRegion(const itk::DataObject* data) override;

    // Contour management
    void AddContour(const ContourSlice& contour);
    void RemoveContour(int index);
    void SetContour(int index, const ContourSlice& contour);
    [[nodiscard]] const ContourSlice* GetContour(int index) const;
    [[nodiscard]] int GetContourCount() const;
    void ClearContours();

    // Path reference
    void SetPathName(const std::string& name);
    [[nodiscard]] std::string GetPathName() const;

protected:
    xq_ContourGroup();
    xq_ContourGroup(const xq_ContourGroup& other);
    ~xq_ContourGroup() override;

    [[nodiscard]] bool IsEmptyTimeStep(unsigned int t) const override;
    void ClearData() override;
    void InitializeEmpty() override;

private:
    std::vector<ContourSlice> m_Contours;
    std::string m_PathName;
};
