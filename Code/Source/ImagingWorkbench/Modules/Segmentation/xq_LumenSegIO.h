// XQ LumenSegIO: factory-pattern XML serialization for contour group persistence
#pragma once

#include <xqModuleSegmentationExports.h>
#include <mitkAbstractFileIO.h>

#include <string>
#include <string_view>

// Forward declarations to avoid pulling in heavy headers for callers of the static helpers
namespace tinyxml2 { class XMLElement; class XMLDocument; }
class xq_ProfileGroup;

class XQMODULESEGMENTATION_EXPORT xq_LumenSegIO : public mitk::AbstractFileIO
{
public:
    // Supported format version written and accepted by this IO class
    static constexpr const char* kFormatVersion = "1.0";

    xq_LumenSegIO();
    ~xq_LumenSegIO() override = default;

    std::vector<mitk::BaseData::Pointer> DoRead() override;
    void Write() override;

    ConfidenceLevel GetReaderConfidenceLevel() const override;
    ConfidenceLevel GetWriterConfidenceLevel() const override;

    // ---------------------------------------------------------------------------
    // Testable static helpers
    // ---------------------------------------------------------------------------

    // Returns true iff version is a format version this reader can handle.
    // Exposed for unit testing.
    static bool IsVersionSupported(std::string_view version);

    // Writes all contour elements from group (for timeStep 0) into contoursElem.
    // Returns the number of contours written.
    // Iterates by path-position index, not by ordinal — sparse groups are handled correctly.
    // Exposed for unit testing.
    static int WriteContourElements(const xq_ProfileGroup* group,
                                    tinyxml2::XMLElement* contoursElem,
                                    tinyxml2::XMLDocument& doc);

    // Converts s to int safely: returns defaultVal instead of throwing on
    // std::invalid_argument / std::out_of_range.
    // Exposed for unit testing.
    static int ParseIntSafe(const std::string& s, int defaultVal = 0);

protected:
    xq_LumenSegIO* IOClone() const override;

private:
    static bool endsWith(std::string_view str, std::string_view suffix);
};
