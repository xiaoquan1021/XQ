#pragma once

#include <xqModuleROMSimulationExports.h>
#include "xq_ROMJob.h"

#include <mitkBaseData.h>

#include <memory>
#include <string>

// MITK BaseData wrapper for xq_ROMJob so it can reside in the DataStorage
// tree alongside other XQ pipeline nodes.
class XQMODULEROMSIMULATION_EXPORT xq_MitkROMJob : public mitk::BaseData
{
public:
    mitkClassMacro(xq_MitkROMJob, mitk::BaseData)
    itkFactorylessNewMacro(Self)
    itkCloneMacro(Self)

    void Expand(unsigned int timeSteps = 1) override;
    [[nodiscard]] bool IsEmptyTimeStep(unsigned int t) const override;
    [[nodiscard]] unsigned int GetTimeSize() const;

    void UpdateOutputInformation() override;
    void SetRequestedRegionToLargestPossibleRegion() override;
    [[nodiscard]] bool RequestedRegionIsOutsideOfTheBufferedRegion() override;
    [[nodiscard]] bool VerifyRequestedRegion() override;
    void SetRequestedRegion(const itk::DataObject* data) override;

    [[nodiscard]] xq_ROMJob* GetROMJob(unsigned int t = 0) const;
    void SetROMJob(std::unique_ptr<xq_ROMJob> job, unsigned int t = 0);

    [[nodiscard]] const std::string& GetStatus() const;
    void SetStatus(std::string_view status);

protected:
    xq_MitkROMJob();
    xq_MitkROMJob(const xq_MitkROMJob& other);
    ~xq_MitkROMJob() override = default;

    void ClearData() override;
    void InitializeEmpty() override;

    std::vector<std::unique_ptr<xq_ROMJob>> m_JobSet;
    std::string m_Status;
    bool m_CalculateBoundingBox = true;
    bool m_DataModified = false;
};
