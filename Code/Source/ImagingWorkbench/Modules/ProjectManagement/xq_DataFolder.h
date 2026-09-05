#ifndef XQ_DATAFOLDER_H
#define XQ_DATAFOLDER_H

#include <xqProjectManagementExports.h>

#include <mitkBaseData.h>

#include <string>

class XQPROJECTMANAGEMENT_EXPORT xq_DataFolder : public mitk::BaseData
{
public:
    mitkClassMacro(xq_DataFolder, mitk::BaseData)
    itkFactorylessNewMacro(Self)

    void SetFolderName(const std::string& name);
    std::string GetFolderName() const;

    void SetFolderType(const std::string& type);
    std::string GetFolderType() const;

    void UpdateOutputInformation() override;
    void SetRequestedRegionToLargestPossibleRegion() override;
    bool RequestedRegionIsOutsideOfTheBufferedRegion() override;
    bool VerifyRequestedRegion() override;
    void SetRequestedRegion(const itk::DataObject* data) override;

protected:
    xq_DataFolder();
    xq_DataFolder(const xq_DataFolder& other);
    ~xq_DataFolder() override;

    std::string m_FolderName;
    std::string m_FolderType;
};

#endif
