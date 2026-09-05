// XQ ContourGroupIO: XML serialization for xq_ContourGroup persistence (.xqcg format)
#pragma once

#include <xqModuleSegmentationExports.h>
#include <mitkAbstractFileIO.h>

#include <string_view>

class XQMODULESEGMENTATION_EXPORT xq_ContourGroupIO : public mitk::AbstractFileIO
{
public:
    xq_ContourGroupIO();
    ~xq_ContourGroupIO() override = default;

    std::vector<mitk::BaseData::Pointer> DoRead() override;
    void Write() override;

    ConfidenceLevel GetReaderConfidenceLevel() const override;
    ConfidenceLevel GetWriterConfidenceLevel() const override;

protected:
    xq_ContourGroupIO* IOClone() const override;

private:
    static bool endsWith(std::string_view str, std::string_view suffix);
};
