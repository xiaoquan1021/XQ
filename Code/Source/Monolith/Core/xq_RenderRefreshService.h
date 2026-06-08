#ifndef XQ_CORE_RENDERREFRESHSERVICE_H
#define XQ_CORE_RENDERREFRESHSERVICE_H

#include <mitkDataStorage.h>

namespace xq::core
{

class RenderRefreshService
{
public:
    virtual ~RenderRefreshService() = default;

    virtual void RefreshDataStorage(mitk::DataStorage::Pointer dataStorage) = 0;
};

} // namespace xq::core

#endif // XQ_CORE_RENDERREFRESHSERVICE_H
