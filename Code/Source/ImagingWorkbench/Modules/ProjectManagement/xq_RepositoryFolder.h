#ifndef XQ_REPOSITORYFOLDER_H
#define XQ_REPOSITORYFOLDER_H

#include "xq_DataFolder.h"

class XQPROJECTMANAGEMENT_EXPORT xq_RepositoryFolder : public xq_DataFolder
{
public:
    mitkClassMacro(xq_RepositoryFolder, xq_DataFolder)
    itkFactorylessNewMacro(Self)

protected:
    xq_RepositoryFolder() { SetFolderName("Repository"); SetFolderType("RepositoryFolder"); }
    ~xq_RepositoryFolder() override = default;
};

#endif
