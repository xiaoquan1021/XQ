#include "xq_MitkSolverJob.h"
#include <mitkProportionalTimeGeometry.h>

itkEventMacroDefinition(xq_MitkSolverJobEvent, itk::AnyEvent);

xq_MitkSolverJob::xq_MitkSolverJob()
    : mitk::BaseData()
{
    Expand(1);
    // XQ fix: even for metadata-only nodes, a valid TimeGeometry is required
    // to keep MITK's rendering pipeline from silently skipping the node
    // (bounding-box evaluation uses TimeGeometry).
    Superclass::InitializeTimeGeometry(1);
}

xq_MitkSolverJob::xq_MitkSolverJob(const xq_MitkSolverJob& other)
    : mitk::BaseData(other)
    , m_MeshName(other.m_MeshName)
    , m_ModelName(other.m_ModelName)
    , m_Status(other.m_Status)
    , m_DataModified(other.m_DataModified)
{
    m_JobSet.reserve(other.m_JobSet.size());
    for (const auto& job : other.m_JobSet)
    {
        if (job)
            m_JobSet.push_back(job->Clone());
        else
            m_JobSet.push_back(nullptr);
    }
}

void xq_MitkSolverJob::ClearData()
{
    m_JobSet.clear();
}

void xq_MitkSolverJob::InitializeEmpty()
{
    ClearData();
    Expand(1);
}

void xq_MitkSolverJob::Expand(unsigned int timeSteps)
{
    const auto oldSize = static_cast<unsigned int>(m_JobSet.size());
    if (timeSteps > oldSize)
    {
        m_JobSet.resize(timeSteps);
        Superclass::Expand(timeSteps);
    }
}

bool xq_MitkSolverJob::IsEmptyTimeStep(unsigned int t) const
{
    return (t >= m_JobSet.size()) || !m_JobSet[t];
}

unsigned int xq_MitkSolverJob::GetTimeSize() const
{
    return static_cast<unsigned int>(m_JobSet.size());
}

void xq_MitkSolverJob::UpdateOutputInformation()
{
    if (this->GetSource())
        this->GetSource()->UpdateOutputInformation();

    if (m_CalculateBoundingBox)
    {
        auto timeGeometry = GetTimeGeometry();
        if (timeGeometry != nullptr)
        {
            if (auto* propTimeGeom = dynamic_cast<mitk::ProportionalTimeGeometry*>(timeGeometry))
                propTimeGeom->Initialize(static_cast<mitk::TimeStepType>(m_JobSet.size()));
        }
        m_CalculateBoundingBox = false;
    }

    GetTimeGeometry()->Update();
}

void xq_MitkSolverJob::SetRequestedRegionToLargestPossibleRegion() {}

bool xq_MitkSolverJob::RequestedRegionIsOutsideOfTheBufferedRegion() { return false; }

bool xq_MitkSolverJob::VerifyRequestedRegion() { return true; }

void xq_MitkSolverJob::SetRequestedRegion(const itk::DataObject* /*data*/) {}

xq_SolverJob* xq_MitkSolverJob::GetSimJob(unsigned int t) const
{
    return (t < m_JobSet.size()) ? m_JobSet[t].get() : nullptr;
}

void xq_MitkSolverJob::SetSimJob(std::unique_ptr<xq_SolverJob> job, unsigned int t)
{
    if (t >= m_JobSet.size())
        Expand(t + 1);

    m_JobSet[t] = std::move(job);
    m_CalculateBoundingBox = true;
    m_DataModified = true;
    this->Modified();
}

void xq_MitkSolverJob::SetMeshName(std::string_view name) { m_MeshName = name; }
const std::string& xq_MitkSolverJob::GetMeshName() const { return m_MeshName; }

void xq_MitkSolverJob::SetModelName(std::string_view name) { m_ModelName = name; }
const std::string& xq_MitkSolverJob::GetModelName() const { return m_ModelName; }

const std::string& xq_MitkSolverJob::GetStatus() const { return m_Status; }
void xq_MitkSolverJob::SetStatus(std::string_view status) { m_Status = status; }
