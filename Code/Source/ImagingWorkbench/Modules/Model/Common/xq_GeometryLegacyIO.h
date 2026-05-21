#pragma once

#include <xqModelCommonExports.h>

#include "xq_Model.h"

#include <string>
#include <string_view>

namespace xq::legacy_io
{

[[nodiscard]] XQMODELCOMMON_EXPORT
xq_Model::Pointer readModel(const std::string& filename);

XQMODELCOMMON_EXPORT
void writeModel(const std::string& filename, const xq_Model* model);

} // namespace xq::legacy_io

// Backward-compatible alias
using xq_GeometryLegacyIO = struct xq_GeometryLegacyIO_Deprecated;
