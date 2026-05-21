#pragma once

#include <xqModuleCommonExports.h>

#include <mitkDataStorage.h>

namespace xq::rendering {

XQMODULECOMMON_EXPORT void RestoreOrthogonalSliceViews(
    const mitk::DataStorage::Pointer& dataStorage);

} // namespace xq::rendering
