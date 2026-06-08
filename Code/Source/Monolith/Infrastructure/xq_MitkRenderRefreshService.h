#ifndef XQ_INFRASTRUCTURE_MITK_RENDER_REFRESH_SERVICE_H
#define XQ_INFRASTRUCTURE_MITK_RENDER_REFRESH_SERVICE_H

#include "Core/xq_RenderRefreshService.h"

namespace xq::infrastructure
{

class MitkRenderRefreshService : public xq::core::RenderRefreshService
{
public:
    void RefreshDataStorage(mitk::DataStorage::Pointer dataStorage) override;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_MITK_RENDER_REFRESH_SERVICE_H
