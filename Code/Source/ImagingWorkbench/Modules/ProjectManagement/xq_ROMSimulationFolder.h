#ifndef XQ_ROMSIMULATIONFOLDER_H
#define XQ_ROMSIMULATIONFOLDER_H

#include "xq_DataFolder.h"

class XQPROJECTMANAGEMENT_EXPORT xq_ROMSimulationFolder : public xq_DataFolder
{
public:
    mitkClassMacro(xq_ROMSimulationFolder, xq_DataFolder)
    itkFactorylessNewMacro(Self)

protected:
    xq_ROMSimulationFolder() { SetFolderName("ROMSimulations"); SetFolderType("ROMSimulationFolder"); }
    ~xq_ROMSimulationFolder() override = default;
};

#endif
