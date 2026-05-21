#ifndef XQ_PATHFOLDER_H
#define XQ_PATHFOLDER_H

#include "xq_DataFolder.h"

class XQPROJECTMANAGEMENT_EXPORT xq_PathFolder : public xq_DataFolder
{
public:
    mitkClassMacro(xq_PathFolder, xq_DataFolder)
    itkFactorylessNewMacro(Self)

protected:
    xq_PathFolder() { SetFolderName("Paths"); SetFolderType("PathFolder"); }
    ~xq_PathFolder() override = default;
};

#endif
