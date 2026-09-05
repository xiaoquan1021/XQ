#ifndef XQ_CORE_XQ_PROJECT_H
#define XQ_CORE_XQ_PROJECT_H

#include "core/XQScene.h"

namespace xq {

class XQProject {
public:
    XQScene& scene();
    const XQScene& scene() const;

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
    LifecycleState state_ = LifecycleState::Created;
};

} // namespace xq

#endif // XQ_CORE_XQ_PROJECT_H
