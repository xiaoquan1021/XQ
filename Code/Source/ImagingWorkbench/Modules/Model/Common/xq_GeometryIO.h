#pragma once

#include <xqModelCommonExports.h>

#include <mitkAbstractFileIO.h>

class XQMODELCOMMON_EXPORT xq_GeometryIO : public mitk::AbstractFileIO
{
public:
    xq_GeometryIO();

    std::vector<mitk::BaseData::Pointer> DoRead() override;
    void Write() override;

protected:
    xq_GeometryIO* IOClone() const override;
};
