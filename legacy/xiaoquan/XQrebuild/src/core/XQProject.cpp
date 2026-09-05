#include "core/XQProject.h"

namespace xq {

XQScene& XQProject::scene()
{
    return scene_;
}

const XQScene& XQProject::scene() const
{
    return scene_;
}

XQProject::LifecycleState XQProject::state() const
{
    return state_;
}

XQProject::LifecycleResult XQProject::open()
{
    if (state_ != LifecycleState::Created) {
        return LifecycleResult::InvalidTransition;
    }

    state_ = LifecycleState::Open;
    return LifecycleResult::Ok;
}

XQProject::LifecycleResult XQProject::close()
{
    if (state_ != LifecycleState::Open) {
        return LifecycleResult::InvalidTransition;
    }

    scene_.clear();
    state_ = LifecycleState::Closed;
    return LifecycleResult::Ok;
}

XQProject::LifecycleResult XQProject::reopen()
{
    if (state_ != LifecycleState::Closed) {
        return LifecycleResult::InvalidTransition;
    }

    state_ = LifecycleState::Open;
    return LifecycleResult::Ok;
}

} // namespace xq
