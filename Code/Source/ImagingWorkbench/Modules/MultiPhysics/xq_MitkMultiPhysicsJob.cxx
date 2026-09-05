#include "xq_MitkMultiPhysicsJob.h"

xq_MitkMultiPhysicsJob::xq_MitkMultiPhysicsJob()
{
    m_JobSet.push_back(std::make_unique<xq_MultiPhysicsJob>());
}

xq_MitkMultiPhysicsJob::xq_MitkMultiPhysicsJob(const xq_MitkMultiPhysicsJob& other)
    : mitk::BaseData(other)
{
    for (const auto& j : other.m_JobSet)
        m_JobSet.push_back(std::make_unique<xq_MultiPhysicsJob>(*j));
    m_Status = other.m_Status;
}

void xq_MitkMultiPhysicsJob::Expand(unsigned int timeSteps)
{
    while (m_JobSet.size() < timeSteps)
        m_JobSet.push_back(std::make_unique<xq_MultiPhysicsJob>());
}

bool xq_MitkMultiPhysicsJob::IsEmptyTimeStep(unsigned int t) const
{
    return t >= m_JobSet.size() || !m_JobSet[t];
}

unsigned int xq_MitkMultiPhysicsJob::GetTimeSize() const
{
    return static_cast<unsigned int>(m_JobSet.size());
}

void xq_MitkMultiPhysicsJob::UpdateOutputInformation()
{
    if (m_CalculateBoundingBox)
    {
        m_CalculateBoundingBox = false;
    }
}

void xq_MitkMultiPhysicsJob::SetRequestedRegionToLargestPossibleRegion() {}
bool xq_MitkMultiPhysicsJob::RequestedRegionIsOutsideOfTheBufferedRegion() { return false; }
bool xq_MitkMultiPhysicsJob::VerifyRequestedRegion() { return true; }
void xq_MitkMultiPhysicsJob::SetRequestedRegion(const itk::DataObject*) {}

xq_MultiPhysicsJob* xq_MitkMultiPhysicsJob::GetJob(unsigned int t) const
{
    return (t < m_JobSet.size()) ? m_JobSet[t].get() : nullptr;
}

void xq_MitkMultiPhysicsJob::SetJob(std::unique_ptr<xq_MultiPhysicsJob> job, unsigned int t)
{
    if (t >= m_JobSet.size()) Expand(t + 1);
    m_JobSet[t] = std::move(job);
    m_DataModified = true;
}

const std::string& xq_MitkMultiPhysicsJob::GetStatus() const { return m_Status; }
void xq_MitkMultiPhysicsJob::SetStatus(std::string_view status) { m_Status = status; }

void xq_MitkMultiPhysicsJob::ClearData()
{
    m_JobSet.clear();
    m_JobSet.push_back(std::make_unique<xq_MultiPhysicsJob>());
}

void xq_MitkMultiPhysicsJob::InitializeEmpty()
{
    m_JobSet.clear();
    m_JobSet.push_back(std::make_unique<xq_MultiPhysicsJob>());
    m_Status.clear();
    m_CalculateBoundingBox = true;
}
