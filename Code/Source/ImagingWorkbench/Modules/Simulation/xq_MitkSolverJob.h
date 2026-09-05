#pragma once

#include <xqModuleSimulationExports.h>
#include "xq_SolverJob.h"

#include <mitkBaseData.h>
#include <itkEventObject.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class XQMODULESIMULATION_EXPORT xq_MitkSolverJob : public mitk::BaseData
{
public:
    mitkClassMacro(xq_MitkSolverJob, mitk::BaseData)
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

    [[nodiscard]] xq_SolverJob* GetSimJob(unsigned int t = 0) const;
    void SetSimJob(std::unique_ptr<xq_SolverJob> job, unsigned int t = 0);

    void SetMeshName(std::string_view name);
    [[nodiscard]] const std::string& GetMeshName() const;

    void SetModelName(std::string_view name);
    [[nodiscard]] const std::string& GetModelName() const;

    [[nodiscard]] const std::string& GetStatus() const;
    void SetStatus(std::string_view status);

    [[nodiscard]] bool IsDataModified() const { return m_DataModified; }
    void SetDataModified(bool modified = true) { m_DataModified = modified; }

protected:
    xq_MitkSolverJob();
    xq_MitkSolverJob(const xq_MitkSolverJob& other);
    ~xq_MitkSolverJob() override = default;

    void ClearData() override;
    void InitializeEmpty() override;

    std::vector<std::unique_ptr<xq_SolverJob>> m_JobSet;
    bool m_CalculateBoundingBox = true;
    std::string m_MeshName;
    std::string m_ModelName;
    std::string m_Status;
    bool m_DataModified = false;
};

itkEventMacroDeclaration(xq_MitkSolverJobEvent, itk::AnyEvent);
