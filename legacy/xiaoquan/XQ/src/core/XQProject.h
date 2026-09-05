#ifndef XQ_CORE_XQ_PROJECT_H
#define XQ_CORE_XQ_PROJECT_H

#include "core/XQScene.h"
#include "core/asset/AssetRegistry.h"

namespace xq {

class XQProject {
public:
    XQScene& scene();
    const XQScene& scene() const;

    AssetRegistry& assetRegistry();
    const AssetRegistry& assetRegistry() const;

    enum class LifecycleState {
        Created,
        Open,
        Closed
    };

    enum class LifecycleResult {
        Ok,
        InvalidTransition
    };

    LifecycleState state() const;
    LifecycleResult open();
    LifecycleResult close();
    LifecycleResult reopen();

private:
    XQScene scene_;
    AssetRegistry assetRegistry_;
    LifecycleState state_ = LifecycleState::Created;
};

} // namespace xq

#endif // XQ_CORE_XQ_PROJECT_H
