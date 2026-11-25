#pragma once

#include <memory>

#include <core/core_api.h>

namespace WebExtensions {

class WebExtension;
class WebExtensionsRuntimeImpl;

class ExtensionImpl : public v2::Extension {
public:
    std::string script_source_for_extension(WebExtension* extension);
    void set_profile_world(std::weak_ptr<orion::bridge::ProfileWorld> profile_world);

    void isAllowedIncognitoAccess(v2::Promise<bool> promise) override;
    void isAllowedFileSchemeAccess(v2::Promise<bool> promise) override;
    void setUpdateUrlData(v2::Promise<void> promise, const std::string& data) override;

private:
    std::weak_ptr<orion::bridge::ProfileWorld> m_profile_world;
};

} // namespace WebExtensions
