// XQ MitkSeg3DIO: composite XML+VTP serialization for 3D segmentation data
#pragma once

#include <xqModuleSegmentationExports.h>
#include <mitkAbstractFileIO.h>

class XQMODULESEGMENTATION_EXPORT xq_MitkSeg3DIO : public mitk::AbstractFileIO
{
public:
    xq_MitkSeg3DIO();
    ~xq_MitkSeg3DIO() override = default;

    std::vector<mitk::BaseData::Pointer> DoRead() override;
    void Write() override;

protected:
    xq_MitkSeg3DIO* IOClone() const override;
};
