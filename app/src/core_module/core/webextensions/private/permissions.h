#pragma once

#include <memory>

#include <core/core_api.h>

namespace WebExtensions {

class WebExtension;
class WebExtensionsRuntimeImpl;

class PermissionsImpl : public v2::Permissions {
public:
    void set_profile_world(std::weak_ptr<orion::bridge::ProfileWorld> profile_world);

    void getAll(v2::Promise<v2::PermissionSet> promise) override;
    void contains(v2::Promise<bool> promise, const v2::PermissionSet& permissions) override;
    void request(v2::Promise<bool> promise, const v2::PermissionSet& permissions) override;
    void remove(v2::Promise<bool> promise, const v2::PermissionSet& permissions) override;
    void addHostAccessRequest(
        v2::Promise<void> promise, const v2::HostAccessRequest& request) override;
    void removeHostAccessRequest(
        v2::Promise<void> promise, const v2::HostAccessRequest& request) override;

private:
    std::shared_ptr<WebExtension> extension_for(const std::string& extension_id) const;
    v2::PermissionSetStorage active_permissions_for(const std::string& extension_id) const;
    static bool contains_all(
        const v2::PermissionSetStorage& active, const v2::PermissionSet& requested);

    std::weak_ptr<orion::bridge::ProfileWorld> m_profile_world;
};

} // namespace WebExtensions
