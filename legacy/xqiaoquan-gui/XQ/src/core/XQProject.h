#ifndef XQ_CORE_XQ_PROJECT_H
#define XQ_CORE_XQ_PROJECT_H

#include "core/XQScene.h"
#include "core/asset/AssetRegistry.h"

#include <cstdint>

namespace xq {

class XQProject {
public:
    XQProject() = default;
    XQProject(const XQProject&) = default;
    XQProject(XQProject&&) = default;
    XQProject& operator=(const XQProject& other);
    XQProject& operator=(XQProject&& other);

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
    std::uint64_t lifecycleEpoch() const;
    LifecycleResult open();
    LifecycleResult close();
    LifecycleResult reopen();

private:
    void advanceLifecycleEpoch();

    XQScene scene_;
    AssetRegistry assetRegistry_;
    LifecycleState state_ = LifecycleState::Created;
    // Runtime-only identity for one open/close lifetime. It is deliberately not
    // serialized: prepared background work uses it to reject results captured
    // before a successful lifecycle transition on this same XQProject object.
    std::uint64_t lifecycleEpoch_ = 0;
};

} // namespace xq

#endif // XQ_CORE_XQ_PROJECT_H
