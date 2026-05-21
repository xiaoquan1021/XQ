#ifndef XQ_IMAGEFOLDER_H
#define XQ_IMAGEFOLDER_H

#include "xq_DataFolder.h"

class XQPROJECTMANAGEMENT_EXPORT xq_ImageFolder : public xq_DataFolder
{
public:
    mitkClassMacro(xq_ImageFolder, xq_DataFolder)
    itkFactorylessNewMacro(Self)

protected:
    xq_ImageFolder() { SetFolderName("Images"); SetFolderType("ImageFolder"); }
    ~xq_ImageFolder() override = default;
};

#endif
