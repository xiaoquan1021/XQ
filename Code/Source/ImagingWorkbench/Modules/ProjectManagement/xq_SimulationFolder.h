#ifndef XQ_SIMULATIONFOLDER_H
#define XQ_SIMULATIONFOLDER_H

#include "xq_DataFolder.h"

class XQPROJECTMANAGEMENT_EXPORT xq_SimulationFolder : public xq_DataFolder
{
public:
    mitkClassMacro(xq_SimulationFolder, xq_DataFolder)
    itkFactorylessNewMacro(Self)

protected:
    xq_SimulationFolder() { SetFolderName("Simulations"); SetFolderType("SimulationFolder"); }
    ~xq_SimulationFolder() override = default;
};

#endif
