// XQ LumenLegacyIO: backward-compatible reader using lookup-table dispatch
#pragma once

#include <xqModuleSegmentationExports.h>
#include "xq_ProfileGroup.h"

#include <string>
#include <string_view>

class XQMODULESEGMENTATION_EXPORT xq_LumenLegacyIO
{
public:
    xq_LumenLegacyIO() = default;
    ~xq_LumenLegacyIO() = default;

    static xq_ProfileGroup::Pointer ReadContourGroupFile(std::string_view filePath);
    static bool WriteContourGroupFile(const xq_ProfileGroup* group, std::string_view filePath);
    static bool IsLegacyFormat(std::string_view filePath);
};
