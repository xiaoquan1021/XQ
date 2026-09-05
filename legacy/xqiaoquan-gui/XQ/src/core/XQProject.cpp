#include "core/XQProject.h"

#include <utility>

namespace xq {

XQProject& XQProject::operator=(const XQProject& other)
{
    if (this == &other) {
        return *this;
    }

    XQScene replacementScene = other.scene_;
    AssetRegistry replacementRegistry = other.assetRegistry_;
    scene_ = std::move(replacementScene);
    assetRegistry_ = std::move(replacementRegistry);
    state_ = other.state_;
    advanceLifecycleEpoch();
    return *this;
}

XQProject& XQProject::operator=(XQProject&& other)
{
    if (this == &other) {
        return *this;
    }

    scene_ = std::move(other.scene_);
    assetRegistry_ = std::move(other.assetRegistry_);
    state_ = other.state_;
    advanceLifecycleEpoch();
    return *this;
}

XQScene& XQProject::scene()
{
    return scene_;
}

const XQScene& XQProject::scene() const
{
    return scene_;
}

AssetRegistry& XQProject::assetRegistry()
{
    return assetRegistry_;
}

const AssetRegistry& XQProject::assetRegistry() const
{
    return assetRegistry_;
}

XQProject::LifecycleState XQProject::state() const
{
    return state_;
}

std::uint64_t XQProject::lifecycleEpoch() const
{
    return lifecycleEpoch_;
}

void XQProject::advanceLifecycleEpoch()
{
    ++lifecycleEpoch_;
}

XQProject::LifecycleResult XQProject::open()
{
    if (state_ != LifecycleState::Created) {
        return LifecycleResult::InvalidTransition;
    }

    state_ = LifecycleState::Open;
    advanceLifecycleEpoch();
    return LifecycleResult::Ok;
}

XQProject::LifecycleResult XQProject::close()
{
    if (state_ != LifecycleState::Open) {
        return LifecycleResult::InvalidTransition;
    }

    scene_.clear();
    assetRegistry_.clear();
    state_ = LifecycleState::Closed;
    advanceLifecycleEpoch();
    return LifecycleResult::Ok;
}

XQProject::LifecycleResult XQProject::reopen()
{
    if (state_ != LifecycleState::Closed) {
        return LifecycleResult::InvalidTransition;
    }

    state_ = LifecycleState::Open;
    advanceLifecycleEpoch();
    return LifecycleResult::Ok;
}

} // namespace xq
