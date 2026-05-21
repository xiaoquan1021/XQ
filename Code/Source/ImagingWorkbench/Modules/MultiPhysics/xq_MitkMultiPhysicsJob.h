#pragma once

#include <xqModuleMultiPhysicsExports.h>
#include "xq_MultiPhysicsJob.h"

#include <mitkBaseData.h>

#include <memory>
#include <string>

// MITK BaseData wrapper for xq_MultiPhysicsJob so it can live in the
// DataStorage tree alongside other XQ pipeline nodes.
class XQMODULEMULTIPHYSICS_EXPORT xq_MitkMultiPhysicsJob : public mitk::BaseData
{
public:
    mitkClassMacro(xq_MitkMultiPhysicsJob, mitk::BaseData)
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

    [[nodiscard]] xq_MultiPhysicsJob* GetJob(unsigned int t = 0) const;
    void SetJob(std::unique_ptr<xq_MultiPhysicsJob> job, unsigned int t = 0);

    [[nodiscard]] const std::string& GetStatus() const;
    void SetStatus(std::string_view status);

protected:
    xq_MitkMultiPhysicsJob();
    xq_MitkMultiPhysicsJob(const xq_MitkMultiPhysicsJob& other);
    ~xq_MitkMultiPhysicsJob() override = default;

    void ClearData() override;
    void InitializeEmpty() override;

    std::vector<std::unique_ptr<xq_MultiPhysicsJob>> m_JobSet;
    std::string m_Status;
    bool m_CalculateBoundingBox = true;
    bool m_DataModified = false;
};
