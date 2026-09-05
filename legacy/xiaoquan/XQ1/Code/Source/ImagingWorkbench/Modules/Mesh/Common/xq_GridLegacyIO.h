#pragma once

#include <xqMeshCommonExports.h>

#include "xq_MitkGrid.h"

#include <string>
#include <string_view>

namespace xq::grid
{

XQMESHCOMMON_EXPORT xq_MitkGrid::Pointer ReadMesh(std::string_view filename);
XQMESHCOMMON_EXPORT bool WriteMesh(std::string_view filename, const xq_MitkGrid* mitkMesh);

} // namespace xq::grid
