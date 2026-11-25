#pragma once

#include <memory>

#include <core/core_api.h>

namespace WebExtensions {

class WebExtensionsRuntimeImpl;

class TabsV2Impl : public v2::Tabs {
public:
    void set_profile_world(std::weak_ptr<orion::bridge::ProfileWorld> profile_world);
    void getCurrent(v2::Promise<rfl::Generic> promise) override;
    void query(
        v2::Promise<std::vector<rfl::Generic>> promise,
        const v2::TabsQueryInfo& query_info) override;
private:
    std::weak_ptr<orion::bridge::ProfileWorld> m_profile_world;
};

} // namespace WebExtensions
