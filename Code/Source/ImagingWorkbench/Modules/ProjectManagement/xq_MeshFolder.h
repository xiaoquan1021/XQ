#ifndef XQ_GRIDFOLDER_H
#define XQ_GRIDFOLDER_H

#include "xq_DataFolder.h"

class XQPROJECTMANAGEMENT_EXPORT xq_GridFolder : public xq_DataFolder
{
public:
    mitkClassMacro(xq_GridFolder, xq_DataFolder)
    itkFactorylessNewMacro(Self)

protected:
    xq_GridFolder() { SetFolderName("Meshes"); SetFolderType("MeshFolder"); }
    ~xq_GridFolder() override = default;
};

#endif
