#include "xq_ContourGroup.h"

xq_ContourGroup::xq_ContourGroup()
{
    Superclass::InitializeTimeGeometry(1);
}

xq_ContourGroup::xq_ContourGroup(const xq_ContourGroup& other)
    : mitk::BaseData(other)
    , m_Contours(other.m_Contours)
    , m_PathName(other.m_PathName)
{
}

xq_ContourGroup::~xq_ContourGroup() = default;

void xq_ContourGroup::UpdateOutputInformation() {}
void xq_ContourGroup::SetRequestedRegionToLargestPossibleRegion() {}
bool xq_ContourGroup::RequestedRegionIsOutsideOfTheBufferedRegion() { return false; }
bool xq_ContourGroup::VerifyRequestedRegion() { return true; }
void xq_ContourGroup::SetRequestedRegion(const itk::DataObject*) {}

void xq_ContourGroup::AddContour(const ContourSlice& contour)
{
    m_Contours.push_back(contour);
    this->Modified();
}

void xq_ContourGroup::RemoveContour(int index)
{
    if (index >= 0 && index < static_cast<int>(m_Contours.size()))
    {
        m_Contours.erase(m_Contours.begin() + index);
        this->Modified();
    }
}

void xq_ContourGroup::SetContour(int index, const ContourSlice& contour)
{
    if (index >= 0 && index < static_cast<int>(m_Contours.size()))
    {
        m_Contours[index] = contour;
        this->Modified();
    }
}

const ContourSlice* xq_ContourGroup::GetContour(int index) const
{
    if (index >= 0 && index < static_cast<int>(m_Contours.size()))
        return &m_Contours[index];
    return nullptr;
}

int xq_ContourGroup::GetContourCount() const
{
    return static_cast<int>(m_Contours.size());
}

void xq_ContourGroup::ClearContours()
{
    m_Contours.clear();
    this->Modified();
}

void xq_ContourGroup::SetPathName(const std::string& name) { m_PathName = name; }
std::string xq_ContourGroup::GetPathName() const { return m_PathName; }

bool xq_ContourGroup::IsEmptyTimeStep(unsigned int) const { return m_Contours.empty(); }
void xq_ContourGroup::ClearData() { m_Contours.clear(); }
void xq_ContourGroup::InitializeEmpty() { m_Contours.clear(); }
