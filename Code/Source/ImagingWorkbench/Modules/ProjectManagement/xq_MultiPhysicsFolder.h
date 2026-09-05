#ifndef XQ_MULTIPHYSICSFOLDER_H
#define XQ_MULTIPHYSICSFOLDER_H

#include "xq_DataFolder.h"

class XQPROJECTMANAGEMENT_EXPORT xq_MultiPhysicsFolder : public xq_DataFolder
{
public:
    mitkClassMacro(xq_MultiPhysicsFolder, xq_DataFolder)
    itkFactorylessNewMacro(Self)

protected:
    xq_MultiPhysicsFolder() { SetFolderName("MultiPhysics"); SetFolderType("MultiPhysicsFolder"); }
    ~xq_MultiPhysicsFolder() override = default;
};

#endif
