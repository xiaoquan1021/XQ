#ifndef XQ_MODELFOLDER_H
#define XQ_MODELFOLDER_H

#include "xq_DataFolder.h"

class XQPROJECTMANAGEMENT_EXPORT xq_ModelFolder : public xq_DataFolder
{
public:
    mitkClassMacro(xq_ModelFolder, xq_DataFolder)
    itkFactorylessNewMacro(Self)

protected:
    xq_ModelFolder() { SetFolderName("Models"); SetFolderType("ModelFolder"); }
    ~xq_ModelFolder() override = default;
};

#endif
