#include <core/webextensions/private/notifications.h>
#include <core/webextensions/runtime_impl.h>
#include <common/src/conversion_util.h>
#include <common/src/file_system.h>
#include <common/src/log.h>

#include <rfl/json.hpp>

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>

namespace WebExtensions {
namespace {

bool
starts_with(const std::string& s, const char* prefix)
{
    return s.rfind(prefix, 0) == 0;
}

// Path inside the extension package for a resource URL ("icon.png",
// "/images/icon.png", "moz-extension://<id>/icon.png"), or nullopt when the
// URL points elsewhere (http(s), data:, another extension).
std::optional<std::string>
package_path_for_url(const std::string& url, const std::string& extension_id)
{
    std::string path = url;
    for(const char* scheme : {"moz-extension://", "chrome-extension://"}) {
        if(starts_with(path, scheme)) {
            const std::string prefix = std::string(scheme) + extension_id + "/";
            if(!starts_with(path, prefix.c_str())) {
                return std::nullopt;
            }
            path = path.substr(prefix.size());
            break;
        }
    }
    if(path.find("://") != std::string::npos || starts_with(path, "data:")) {
        return std::nullopt;
    }
    while(!path.empty() && path.front() == '/') {
        path.erase(0, 1);
    }
    const size_t cut = path.find_first_of("?#");
    if(cut != std::string::npos) {
        path.erase(cut);
    }
    if(path.empty()) {
        return std::nullopt;
    }
    return path;
}

// Filesystem-safe cache file extension derived from a path or MIME subtype.
std::string
file_extension_for(const std::string& name)
{
    std::string ext = name;
    const size_t dot = ext.find_last_of('.');
    if(dot != std::string::npos) {
        ext = ext.substr(dot + 1);
    }
    if(ext == "jpeg") ext = "jpg";
    if(ext == "svg+xml") ext = "svg";
    if(ext.empty() || ext.size() > 5) {
        return "png";
    }
    for(const char c : ext) {
        if(!std::isalnum(static_cast<unsigned char>(c))) {
            return "png";
        }
    }
    return ext;
}

std::string
sanitize_component(const std::string& s)
{
    std::string out;
    for(const char c : s) {
        const bool ok = std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_';
        out += ok ? c : '_';
    }
    return out;
}

std::string
hex_hash(const std::string& s)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx",
        static_cast<unsigned long long>(std::hash<std::string>{}(s)));
    return buf;
}

// Writes data to path unless an identical-size file is already there.
bool
write_cache_file(const std::filesystem::path& path, const uint8_t* data, size_t length)
{
    std::error_code ec;
    if(std::filesystem::exists(path, ec) && std::filesystem::file_size(path, ec) == length) {
        return true;
    }
    if(orion::ensure_parent_directories(path)) {
        return false;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if(!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(length));
    return out.good();
}

} // namespace

NotificationsImpl::~NotificationsImpl()
{
    if(m_api && m_event_observer != orion::ObserverId{}) {
        m_api->notification_event().remove_observer(m_event_observer);
    }
}

void
NotificationsImpl::set_profile_world(std::weak_ptr<orion::bridge::ProfileWorld> profile_world)
{
    m_profile_world = profile_world;
    if(m_api && m_event_observer != orion::ObserverId{}) {
        m_api->notification_event().remove_observer(m_event_observer);
        m_event_observer = {};
    }
    const auto world = profile_world.lock();
    m_api = world ? &world->core_api() : nullptr;
    if(m_api) {
        m_event_observer = m_api->notification_event().add_observer(
            [this](orion::NotificationEvent event) { on_platform_event(event); });
    }
}

void
NotificationsImpl::create(
    v2::Promise<std::string> promise,
    const std::string* notification_id,
    const v2::NotificationOptions& options)
{
    std::string id = notification_id ? *notification_id : "";
    if(id.empty()) {
        id = "orion-notification-" + std::to_string(m_next_notification_id++);
    }
    const std::string& extension_id = promise.extension_id();
    Record& record = m_notifications[extension_id][id];
    record.options = v2::to_storage(options);
    show(extension_id, id, record.options);
    promise.resolve(id);
}

void
NotificationsImpl::update(
    v2::Promise<bool> promise,
    const std::string& notification_id,
    const v2::NotificationOptions& options)
{
    const std::string& extension_id = promise.extension_id();
    auto extension_it = m_notifications.find(extension_id);
    if(extension_it == m_notifications.end()) {
        promise.resolve(false);
        return;
    }
    auto record_it = extension_it->second.find(notification_id);
    if(record_it == extension_it->second.end()) {
        promise.resolve(false);
        return;
    }
    // Chrome semantics: only the supplied fields change.
    merge_options(record_it->second.options, v2::to_storage(options));
    show(extension_id, notification_id, record_it->second.options);
    promise.resolve(true);
}

void
NotificationsImpl::clear(v2::Promise<bool> promise, const std::string& notification_id)
{
    const std::string extension_id = promise.extension_id();
    auto extension_it = m_notifications.find(extension_id);
    if(extension_it == m_notifications.end() || extension_it->second.erase(notification_id) == 0) {
        promise.resolve(false);
        return;
    }
    if(m_api) {
        const orion::Error err = m_api->notification_provider().clear(
            orion::NotificationSource::Extension, extension_id, notification_id);
        if(err) {
            orion::log_warning("notifications: clear(%s) failed: %s",
                notification_id.c_str(), err.message().c_str());
        }
    }
    promise.resolve(true);
    onClosed.notify_extension(extension_id, notification_id, false);
}

void
NotificationsImpl::getAll(v2::Promise<rfl::Generic> promise)
{
    // Chrome/Firefox: an object whose keys are the live notification ids
    // (values are `true`), not the stored options.
    rfl::Generic::Object result;
    const auto extension_it = m_notifications.find(promise.extension_id());
    if(extension_it != m_notifications.end()) {
        for(const auto& [id, record] : extension_it->second) {
            result.insert(std::make_pair(id, rfl::Generic(true)));
        }
    }
    promise.resolve(rfl::Generic(std::move(result)));
}

void
NotificationsImpl::getPermissionLevel(v2::Promise<std::string> promise)
{
    promise.resolve("granted");
}

void
NotificationsImpl::on_platform_event(const orion::NotificationEvent& event)
{
    if(event.source != orion::NotificationSource::Extension) {
        return;
    }
    auto extension_it = m_notifications.find(event.source_id);
    if(extension_it == m_notifications.end()) {
        return;
    }
    if(extension_it->second.find(event.notification_id) == extension_it->second.end()) {
        return; // already cleared, or another runtime's extension
    }
    // Copies: the record (and possibly the extension map entry) is erased below.
    const std::string extension_id = event.source_id;
    const std::string notification_id = event.notification_id;

    switch(event.type) {
        case orion::NotificationEventType::Shown:
            onShown.notify_extension(extension_id, notification_id);
            return;
        case orion::NotificationEventType::Clicked:
            onClicked.notify_extension(extension_id, notification_id);
            break;
        case orion::NotificationEventType::ButtonClicked:
            onButtonClicked.notify_extension(extension_id, notification_id, event.button_index);
            break;
        case orion::NotificationEventType::Closed:
            break;
    }
    // Activation dismisses the notification (see notification_provider.h).
    const bool by_user = event.type == orion::NotificationEventType::Closed ? event.by_user : true;
    extension_it->second.erase(notification_id);
    if(extension_it->second.empty()) {
        m_notifications.erase(extension_it);
    }
    onClosed.notify_extension(extension_id, notification_id, by_user);
}

void
NotificationsImpl::show(
    const std::string& extension_id, const std::string& notification_id,
    const v2::NotificationOptionsStorage& options)
{
    if(!m_api) {
        return;
    }
    const orion::NotificationRequest request = make_request(extension_id, notification_id, options);
    const orion::Error err = m_api->notification_provider().show(request);
    if(err) {
        orion::log_warning("notifications: show(%s) failed: %s",
            notification_id.c_str(), err.message().c_str());
    }
}

orion::NotificationRequest
NotificationsImpl::make_request(
    const std::string& extension_id, const std::string& notification_id,
    const v2::NotificationOptionsStorage& options)
{
    orion::NotificationRequest request;
    request.source = orion::NotificationSource::Extension;
    request.source_id = extension_id;
    request.notification_id = notification_id;
    request.type = options.type.value_or("basic");
    request.title = options.title.value_or("");
    request.message = options.message.value_or("");
    request.context_message = options.contextMessage.value_or("");
    request.icon_path = resolve_resource(extension_id, options.iconUrl.value_or(""));
    if(request.type == "image") {
        request.image_path = resolve_resource(extension_id, options.imageUrl.value_or(""));
    }
    if(options.buttons) {
        for(const v2::NotificationButtonStorage& button : *options.buttons) {
            request.buttons.push_back({
                button.title,
                resolve_resource(extension_id, button.iconUrl.value_or(""))});
        }
    }
    if(options.items) {
        for(const v2::NotificationItemStorage& item : *options.items) {
            request.items.push_back({item.title, item.message});
        }
    }
    request.progress = options.progress;
    request.priority = options.priority.value_or(0);
    request.event_time = options.eventTime;
    request.require_interaction = options.requireInteraction.value_or(false);
    request.silent = options.silent.value_or(false);
    return request;
}

std::string
NotificationsImpl::resolve_resource(const std::string& extension_id, const std::string& url)
{
    if(url.empty() || !m_api) {
        return "";
    }
    if(starts_with(url, "http://") || starts_with(url, "https://")) {
        return url; // the host decides whether remote images are supported
    }

    const std::string cache_dir = orion::get_data_directory_path(
        m_api->app_id().c_str(), {"notifications", sanitize_component(extension_id)});

    if(starts_with(url, "data:")) {
        // data:<mime>;base64,<payload>
        const size_t comma = url.find(',');
        const std::string header = comma == std::string::npos ? "" : url.substr(5, comma - 5);
        if(comma == std::string::npos || header.find(";base64") == std::string::npos) {
            orion::log_warning("notifications: unsupported data: URL for %s", extension_id.c_str());
            return "";
        }
        const std::string bytes = orion::base64_decode(url.substr(comma + 1));
        const std::string mime = header.substr(0, header.find(';'));
        const std::filesystem::path file = std::filesystem::path(cache_dir)
            / (hex_hash(url) + "." + file_extension_for(mime.substr(mime.find('/') + 1)));
        if(!write_cache_file(file, reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size())) {
            orion::log_warning("notifications: cannot write %s", file.string().c_str());
            return "";
        }
        return file.string();
    }

    const std::optional<std::string> package_path = package_path_for_url(url, extension_id);
    if(!package_path) {
        orion::log_warning("notifications: cannot resolve resource '%s' for %s",
            url.c_str(), extension_id.c_str());
        return "";
    }
    const auto world = m_profile_world.lock();
    auto* runtime = world ? static_cast<WebExtensionsRuntimeImpl*>(world->runtime()) : nullptr;
    const std::shared_ptr<WebExtension> extension =
        runtime ? runtime->extension_for_id(orion::ExtensionId(extension_id)) : nullptr;
    if(!extension) {
        return "";
    }
    const std::optional<WebExtensionResourceData> data =
        extension->resources().data_for_path(*package_path);
    if(!data || !data->data) {
        orion::log_warning("notifications: %s has no resource '%s'",
            extension_id.c_str(), package_path->c_str());
        return "";
    }
    const std::filesystem::path file = std::filesystem::path(cache_dir)
        / (hex_hash(*package_path) + "." + file_extension_for(*package_path));
    if(!write_cache_file(file, data->data, data->datalen)) {
        orion::log_warning("notifications: cannot write %s", file.string().c_str());
        return "";
    }
    return file.string();
}

void
NotificationsImpl::merge_options(
    v2::NotificationOptionsStorage& into, const v2::NotificationOptionsStorage& from)
{
    if(from.type) into.type = from.type;
    if(from.iconUrl) into.iconUrl = from.iconUrl;
    if(from.appIconMaskUrl) into.appIconMaskUrl = from.appIconMaskUrl;
    if(from.title) into.title = from.title;
    if(from.message) into.message = from.message;
    if(from.contextMessage) into.contextMessage = from.contextMessage;
    if(from.priority) into.priority = from.priority;
    if(from.eventTime) into.eventTime = from.eventTime;
    if(from.buttons) into.buttons = from.buttons;
    if(from.imageUrl) into.imageUrl = from.imageUrl;
    if(from.items) into.items = from.items;
    if(from.progress) into.progress = from.progress;
    if(from.isClickable) into.isClickable = from.isClickable;
    if(from.requireInteraction) into.requireInteraction = from.requireInteraction;
    if(from.silent) into.silent = from.silent;
}

} // namespace WebExtensions
