#pragma once

#include <common/src/error.h>
#include <optional>
#include <string>
#include <vector>

namespace orion {

// Who asked for a notification. Extension notifications come from the
// WebExtensions runtime; Web (page Notification API) and Browser (Orion's own
// messages) are reserved for other core/host callers of the same presenter.
enum class NotificationSource {
    Extension,
    Web,
    Browser,
};

struct NotificationButtonRequest {
    std::string title;
    std::string icon_path; // see NotificationRequest::icon_path
};

struct NotificationItemRequest {
    std::string title;
    std::string message;
};

// A notification to display, fully resolved: extension-package resources have
// already been materialised into files, so hosts only deal with paths/URLs.
struct NotificationRequest {
    NotificationSource source = NotificationSource::Extension;
    std::string source_id;       // extension id for Extension; origin for Web
    std::string notification_id; // unique within (source, source_id); showing
                                 // the same id again replaces the notification
    std::string type;            // "basic" | "image" | "list" | "progress"
    std::string title;
    std::string message;
    std::string context_message;
    std::string icon_path;  // absolute local file path or http(s) URL; "" if none
    std::string image_path; // as icon_path; only for type "image"
    std::vector<NotificationButtonRequest> buttons;
    std::vector<NotificationItemRequest> items; // only for type "list"
    std::optional<int> progress;                // 0..100, only for type "progress"
    int priority = 0;                           // -2..2
    std::optional<double> event_time;           // ms since the Unix epoch
    bool require_interaction = false;
    bool silent = false;
};

// Host-implemented desktop notification presenter, injected with
// CoreApi::set_notification_provider (same pattern as PasswordStorage).
// Calls arrive on the core main thread.
class NotificationProvider {
public:
    virtual ~NotificationProvider() = default;

    // Display (or replace, for a known id) a notification.
    virtual Error show(const NotificationRequest& request) = 0;
    // Remove a notification from the screen / notification center.
    virtual Error clear(
        NotificationSource source, const std::string& source_id,
        const std::string& notification_id) = 0;
};

// User/platform interaction with a shown notification, reported by the host
// through CoreApi::notification_event() on the core main thread.
//
// Activation (Clicked / ButtonClicked) always dismisses the notification on
// the platforms we target, so the core synthesises the matching Closed event
// itself; hosts report Closed only for dismissals without activation that the
// platform lets them observe.
enum class NotificationEventType {
    Shown,
    Clicked,
    ButtonClicked,
    Closed,
};

struct NotificationEvent {
    NotificationEventType type = NotificationEventType::Shown;
    NotificationSource source = NotificationSource::Extension;
    std::string source_id;
    std::string notification_id;
    int button_index = -1; // ButtonClicked
    bool by_user = false;  // Closed
};

} // namespace orion
