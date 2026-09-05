#include "xq_MitkROMJob.h"

xq_MitkROMJob::xq_MitkROMJob()
{
    m_JobSet.push_back(std::make_unique<xq_ROMJob>());
}

xq_MitkROMJob::xq_MitkROMJob(const xq_MitkROMJob& other)
    : mitk::BaseData(other)
{
    for (const auto& j : other.m_JobSet)
        m_JobSet.push_back(std::make_unique<xq_ROMJob>(*j));
    m_Status = other.m_Status;
}

void xq_MitkROMJob::Expand(unsigned int timeSteps)
{
    while (m_JobSet.size() < timeSteps)
        m_JobSet.push_back(std::make_unique<xq_ROMJob>());
}

bool xq_MitkROMJob::IsEmptyTimeStep(unsigned int t) const
{
    return t >= m_JobSet.size() || !m_JobSet[t];
}

unsigned int xq_MitkROMJob::GetTimeSize() const
{
    return static_cast<unsigned int>(m_JobSet.size());
}

void xq_MitkROMJob::UpdateOutputInformation()
{
    if (m_CalculateBoundingBox)
    {
        m_CalculateBoundingBox = false;
    }
}

void xq_MitkROMJob::SetRequestedRegionToLargestPossibleRegion() {}
bool xq_MitkROMJob::RequestedRegionIsOutsideOfTheBufferedRegion() { return false; }
bool xq_MitkROMJob::VerifyRequestedRegion() { return true; }
void xq_MitkROMJob::SetRequestedRegion(const itk::DataObject*) {}

xq_ROMJob* xq_MitkROMJob::GetROMJob(unsigned int t) const
{
    return (t < m_JobSet.size()) ? m_JobSet[t].get() : nullptr;
}

void xq_MitkROMJob::SetROMJob(std::unique_ptr<xq_ROMJob> job, unsigned int t)
{
    if (t >= m_JobSet.size()) Expand(t + 1);
    m_JobSet[t] = std::move(job);
    m_DataModified = true;
}

const std::string& xq_MitkROMJob::GetStatus() const { return m_Status; }
void xq_MitkROMJob::SetStatus(std::string_view status) { m_Status = status; }

void xq_MitkROMJob::ClearData()
{
    m_JobSet.clear();
    m_JobSet.push_back(std::make_unique<xq_ROMJob>());
}

void xq_MitkROMJob::InitializeEmpty()
{
    m_JobSet.clear();
    m_JobSet.push_back(std::make_unique<xq_ROMJob>());
    m_Status.clear();
    m_CalculateBoundingBox = true;
}
