#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <core/core_api.h>
#include <core/notification_provider.h>

namespace WebExtensions {

class WebExtensionsRuntimeImpl;

// browser.notifications: keeps the per-extension notification records and
// hands display to the host's orion::NotificationProvider. Host interaction
// events arrive through CoreApi::notification_event() and are translated into
// the extension-scoped onShown / onClicked / onButtonClicked / onClosed
// listeners. Records live only as long as the runtime.
class NotificationsImpl : public v2::Notifications {
public:
    ~NotificationsImpl();
    // Bridge hook (runtime_impl.h Implements): binds the CoreApi whose
    // NotificationProvider displays and whose notification_event() reports.
    void set_profile_world(std::weak_ptr<orion::bridge::ProfileWorld> profile_world);

    void create(
        v2::Promise<std::string> promise,
        const std::string* notification_id,
        const v2::NotificationOptions& options) override;
    void update(
        v2::Promise<bool> promise,
        const std::string& notification_id,
        const v2::NotificationOptions& options) override;
    void clear(v2::Promise<bool> promise, const std::string& notification_id) override;
    void getAll(v2::Promise<rfl::Generic> promise) override;
    void getPermissionLevel(v2::Promise<std::string> promise) override;

private:
    struct Record {
        v2::NotificationOptionsStorage options;
    };
    typedef std::map<std::string, Record> Records;           // notification id -> record
    typedef std::map<std::string, Records> ExtensionRecords; // extension id -> records

    void on_platform_event(const orion::NotificationEvent& event);
    void show(
        const std::string& extension_id, const std::string& notification_id,
        const v2::NotificationOptionsStorage& options);
    orion::NotificationRequest make_request(
        const std::string& extension_id, const std::string& notification_id,
        const v2::NotificationOptionsStorage& options);
    // Absolute local file path (or http(s) URL) for a notification resource
    // URL; extension-package resources and data: URLs are written to the data
    // directory. Empty when the resource cannot be resolved.
    std::string resolve_resource(const std::string& extension_id, const std::string& url);
    static void merge_options(
        v2::NotificationOptionsStorage& into, const v2::NotificationOptionsStorage& from);

    orion::CoreApi* m_api = nullptr;  // owned by the profile world, outlives the runtime
    std::weak_ptr<orion::bridge::ProfileWorld> m_profile_world;
    orion::ObserverId m_event_observer{};
    uint64_t m_next_notification_id = 1;
    ExtensionRecords m_notifications;
};

} // namespace WebExtensions
