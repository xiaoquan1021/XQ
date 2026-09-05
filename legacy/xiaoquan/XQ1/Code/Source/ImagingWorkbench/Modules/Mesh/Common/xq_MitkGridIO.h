#pragma once

#include <xqMeshCommonExports.h>

#include <mitkAbstractFileIO.h>

class XQMESHCOMMON_EXPORT xq_MitkGridIO : public mitk::AbstractFileIO
{
public:
    xq_MitkGridIO();

    using mitk::AbstractFileIO::Read;

protected:
    std::vector<mitk::BaseData::Pointer> DoRead() override;
    void Write() override;

private:
    [[nodiscard]] xq_MitkGridIO* IOClone() const override;
};
