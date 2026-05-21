#pragma once

#include <xqModulePathExports.h>

#include <mitkAbstractFileIO.h>

class XQMODULEPATH_EXPORT xq_CenterlineIO : public mitk::AbstractFileIO
{
public:
    xq_CenterlineIO();

    [[nodiscard]] std::vector<mitk::BaseData::Pointer> DoRead() override;
    [[nodiscard]] ConfidenceLevel GetReaderConfidenceLevel() const override;

    void Write() override;
    [[nodiscard]] ConfidenceLevel GetWriterConfidenceLevel() const override;

private:
    xq_CenterlineIO* IOClone() const override;
};
