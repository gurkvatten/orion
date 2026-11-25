#pragma once

#include <memory>

#include <core/core_api.h>

namespace WebExtensions {

class WebExtensionsRuntimeImpl;
class WebExtension;

class WindowsImpl : public v2::Windows {
public:
    static std::string script_source_for_extension(WebExtension* extension);
    void set_profile_world(std::weak_ptr<orion::bridge::ProfileWorld> profile_world);
    void getCurrent(
        v2::Promise<v2::WindowsWindow> promise,
        const v2::WindowsQueryOptions* query_options) override;
private:
    std::weak_ptr<orion::bridge::ProfileWorld> m_profile_world;
};

} // namespace WebExtensions
