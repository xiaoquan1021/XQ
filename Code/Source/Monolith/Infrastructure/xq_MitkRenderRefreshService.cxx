#include "xq_MitkRenderRefreshService.h"

#include <mitkRenderingManager.h>

namespace xq::infrastructure
{

void MitkRenderRefreshService::RefreshDataStorage(
    mitk::DataStorage::Pointer dataStorage)
{
    auto* renderingManager = mitk::RenderingManager::GetInstance();
    if (!renderingManager)
        return;

    if (dataStorage.IsNotNull())
        renderingManager->InitializeViewsByBoundingObjects(dataStorage);

    renderingManager->RequestUpdateAll();
}

} // namespace xq::infrastructure
