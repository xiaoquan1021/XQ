#include "xq_DataFolder.h"

xq_DataFolder::xq_DataFolder()
    : m_FolderName("")
    , m_FolderType("")
{
}

xq_DataFolder::xq_DataFolder(const xq_DataFolder& other)
    : mitk::BaseData(other)
    , m_FolderName(other.m_FolderName)
    , m_FolderType(other.m_FolderType)
{
}

xq_DataFolder::~xq_DataFolder()
{
}

void xq_DataFolder::SetFolderName(const std::string& name)
{
    m_FolderName = name;
}

std::string xq_DataFolder::GetFolderName() const
{
    return m_FolderName;
}

void xq_DataFolder::SetFolderType(const std::string& type)
{
    m_FolderType = type;
}

std::string xq_DataFolder::GetFolderType() const
{
    return m_FolderType;
}

void xq_DataFolder::UpdateOutputInformation()
{
}

void xq_DataFolder::SetRequestedRegionToLargestPossibleRegion()
{
}

bool xq_DataFolder::RequestedRegionIsOutsideOfTheBufferedRegion()
{
    return false;
}

bool xq_DataFolder::VerifyRequestedRegion()
{
    return true;
}

void xq_DataFolder::SetRequestedRegion(const itk::DataObject* /*data*/)
{
}
