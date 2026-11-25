#include <algorithm>
#include <set>

#include <core/webextensions/extension.h>
#include <core/webextensions/private/permissions.h>
#include <core/webextensions/repository.h>
#include <core/webextensions/runtime_impl.h>
#include <rfl/json.hpp>

namespace WebExtensions {
namespace {

struct DataCollectionPermissions {
    std::optional<std::vector<std::string>> required;
};

struct FirefoxSpecificSettings {
    std::optional<DataCollectionPermissions> data_collection_permissions;
};

struct BrowserSpecificSettings {
    std::optional<FirefoxSpecificSettings> gecko;
};

struct PermissionManifest {
    std::optional<std::vector<std::string>> host_permissions;
    std::optional<BrowserSpecificSettings> browser_specific_settings;
};

bool
is_origin_permission(const std::string& permission)
{
    return permission == "<all_urls>" || permission.find("://") != std::string::npos;
}

std::vector<std::string>
sorted_values(const std::set<std::string>& values)
{
    return {values.begin(), values.end()};
}

} // namespace

void
PermissionsImpl::set_profile_world(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world)
{
    m_profile_world = profile_world;
}

void
PermissionsImpl::getAll(v2::Promise<v2::PermissionSet> promise)
{
    if(!extension_for(promise.extension_id())) {
        promise.reject("No extension with id " + promise.extension_id());
        return;
    }
    const v2::PermissionSetStorage permissions = active_permissions_for(promise.extension_id());
    const v2::PermissionSet permissions_view = v2::from_storage(permissions);
    promise.resolve(permissions_view);
}

void
PermissionsImpl::contains(
    v2::Promise<bool> promise, const v2::PermissionSet& permissions)
{
    if(!extension_for(promise.extension_id())) {
        promise.reject("No extension with id " + promise.extension_id());
        return;
    }
    promise.resolve(contains_all(active_permissions_for(promise.extension_id()), permissions));
}

void
PermissionsImpl::request(
    v2::Promise<bool> promise, const v2::PermissionSet& permissions)
{
    if(!extension_for(promise.extension_id())) {
        promise.reject("No extension with id " + promise.extension_id());
        return;
    }

    // The shim never grants new privileges without a browser permission prompt.
    // A request for permissions which are already active succeeds without one.
    promise.resolve(contains_all(active_permissions_for(promise.extension_id()), permissions));
}

void
PermissionsImpl::remove(
    v2::Promise<bool> promise, const v2::PermissionSet&)
{
    if(!extension_for(promise.extension_id())) {
        promise.reject("No extension with id " + promise.extension_id());
        return;
    }

    // Required manifest permissions cannot be removed, and this shim does not
    // yet persist optional grants.
    promise.resolve(false);
}

void
PermissionsImpl::addHostAccessRequest(
    v2::Promise<void> promise, const v2::HostAccessRequest&)
{
    promise.reject("permissions.addHostAccessRequest is not implemented");
}

void
PermissionsImpl::removeHostAccessRequest(
    v2::Promise<void> promise, const v2::HostAccessRequest&)
{
    promise.reject("permissions.removeHostAccessRequest is not implemented");
}

std::shared_ptr<WebExtension>
PermissionsImpl::extension_for(const std::string& extension_id) const
{
    auto profile_world = m_profile_world.lock();
    if(!profile_world) return nullptr;
    auto* runtime = profile_world->runtime();
    if(!runtime) return nullptr;
    for(const auto& weak_extension : runtime->get_extensions().value_or(WebExtensionList())) {
        auto extension = weak_extension.lock();
        if(extension && extension->extension_id().value == extension_id) {
            return extension;
        }
    }
    return nullptr;
}

v2::PermissionSetStorage
PermissionsImpl::active_permissions_for(const std::string& extension_id) const
{
    std::set<std::string> named_permissions;
    std::set<std::string> origins;
    std::set<std::string> data_collection;
    auto extension = extension_for(extension_id);
    if(!extension) {
        return {};
    }

    auto add_permission = [&](const std::string& permission) {
        if(is_origin_permission(permission)) {
            origins.insert(permission);
        } else {
            named_permissions.insert(permission);
        }
    };

    // WebExtension::permissions() is the effective named permission set. It
    // includes the declared debug permission only when debug mode grants it.
    for(const auto& permission : extension->permissions()) {
        add_permission(permission);
    }

    const auto manifest_data = extension->resources().data_for_path("manifest.json");
    if(manifest_data) {
        const std::string manifest_json(
            reinterpret_cast<const char*>(manifest_data->data), manifest_data->datalen);
        auto manifest = rfl::json::read<PermissionManifest>(manifest_json);
        if(manifest) {
            for(const auto& origin : manifest->host_permissions.value_or(std::vector<std::string>())) {
                origins.insert(origin);
            }
            if(manifest->browser_specific_settings
                && manifest->browser_specific_settings->gecko
                && manifest->browser_specific_settings->gecko->data_collection_permissions) {
                const auto& declared =
                    manifest->browser_specific_settings->gecko->data_collection_permissions->required;
                const auto declared_values = declared.value_or(std::vector<std::string>());
                data_collection.insert(declared_values.begin(), declared_values.end());
            }
        }
    }

    for(const auto& content_script : extension->content_scripts()) {
        origins.insert(content_script.matches.begin(), content_script.matches.end());
    }

    return {
        .permissions = sorted_values(named_permissions),
        .origins = sorted_values(origins),
        .data_collection = sorted_values(data_collection),
    };
}

bool
PermissionsImpl::contains_all(
    const v2::PermissionSetStorage& active, const v2::PermissionSet& requested)
{
    const auto active_named = active.permissions.value_or(std::vector<std::string>());
    const auto active_origins = active.origins.value_or(std::vector<std::string>());
    const auto active_data_collection =
        active.data_collection.value_or(std::vector<std::string>());
    const std::set<std::string> named_permissions(active_named.begin(), active_named.end());
    const std::set<std::string> origins(active_origins.begin(), active_origins.end());
    const std::set<std::string> data_collection(
        active_data_collection.begin(), active_data_collection.end());

    const auto all_present = [](const std::vector<std::string>* requested_values,
                                const std::set<std::string>& active_values) {
        return !requested_values || std::all_of(
            requested_values->begin(), requested_values->end(),
            [&](const std::string& value) { return active_values.contains(value); });
    };
    const auto all_origins_present = [&](const std::vector<std::string>* requested_values) {
        if(!requested_values) {
            return true;
        }
        return std::all_of(
            requested_values->begin(), requested_values->end(),
            [&](const std::string& value) {
                // Match-pattern permissions cover narrower requested patterns;
                // they are not limited to exact string equality.
                if(origins.contains(value) || origins.contains("<all_urls>")) {
                    return true;
                }
                return origins.contains("*://*/*")
                    && (value.starts_with("http://")
                        || value.starts_with("https://")
                        || value.starts_with("*://"));
            });
    };
    return all_present(requested.permissions, named_permissions)
        && all_origins_present(requested.origins)
        && all_present(requested.data_collection, data_collection);
}

} // namespace WebExtensions
