#include "xq_RenderWindowStateUtils.h"

#include <mitkRenderingManager.h>

namespace xq::rendering {

void RestoreOrthogonalSliceViews(const mitk::DataStorage::Pointer& dataStorage)
{
    if (dataStorage.IsNull())
        return;

    auto* renderingManager = mitk::RenderingManager::GetInstance();
    if (!renderingManager)
        return;

    renderingManager->InitializeViewsByBoundingObjects(dataStorage);
    renderingManager->RequestUpdateAll();
}

} // namespace xq::rendering
