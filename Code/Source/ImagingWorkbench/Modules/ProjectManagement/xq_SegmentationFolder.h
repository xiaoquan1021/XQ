#ifndef XQ_SEGMENTATIONFOLDER_H
#define XQ_SEGMENTATIONFOLDER_H

#include "xq_DataFolder.h"

class XQPROJECTMANAGEMENT_EXPORT xq_SegmentationFolder : public xq_DataFolder
{
public:
    mitkClassMacro(xq_SegmentationFolder, xq_DataFolder)
    itkFactorylessNewMacro(Self)

protected:
    xq_SegmentationFolder() { SetFolderName("Segmentations"); SetFolderType("SegmentationFolder"); }
    ~xq_SegmentationFolder() override = default;
};

#endif
