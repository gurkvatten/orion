#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>

#include "app.h"
#include "privacy_popover.h"
#include "util/ref_util.h"
#include "util/uri_util.h"
#include "util/text_util.h"
#include "window/main_window/main_window.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

struct _PrivacyPopover
{
    GtkPopover parent;

#ifdef ENABLE_ADW_17_API
    GtkImage* website_icon;
    GtkLabel* website_label;
    AdwToggleGroup* content_toggle_group;
    AdwToggle* requests_toggle;
    AdwToggle* trackers_toggle;
    GtkListBox* content_list_box;

    std::vector<std::string> tracker_domains;
    std::string tracker_status_message;
    guint64 tracker_summary_generation;
    bool tracker_summary_loading;
    GBinding* website_icon_binding;
    GWeakRef current_tab_content;
    gulong resources_changed_handler;
    gulong loading_changed_handler;
    guint tracker_refresh_timeout_id;
#endif
};

G_DEFINE_TYPE(PrivacyPopover, privacy_popover, GTK_TYPE_POPOVER)

#ifdef ENABLE_ADW_17_API

static constexpr guint TRACKER_REFRESH_DELAY_MS = 250;

static void
cancel_scheduled_tracker_refresh(PrivacyPopover* self)
{
    if (!self->tracker_refresh_timeout_id)
        return;

    g_source_remove(self->tracker_refresh_timeout_id);
    self->tracker_refresh_timeout_id = 0;
}

static gboolean
refresh_tracker_report_cb(gpointer user_data)
{
    orion::log_verbose("refresh_tracker_report_cb");
    PrivacyPopover* self = PRIVACY_POPOVER(user_data);
    self->tracker_refresh_timeout_id = 0;

    if (!gtk_widget_get_visible(GTK_WIDGET(self)))
        return G_SOURCE_REMOVE;

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(self));
    if (root && MAIN_IS_WINDOW(root))
        privacy_popover_refresh(self, MAIN_WINDOW(root));

    return G_SOURCE_REMOVE;
}

static void
schedule_tracker_refresh(PrivacyPopover* self)
{
    if (self->tracker_refresh_timeout_id
        || !gtk_widget_get_visible(GTK_WIDGET(self)))
        return;

    self->tracker_refresh_timeout_id = g_timeout_add(
        TRACKER_REFRESH_DELAY_MS, refresh_tracker_report_cb, self);
}

static void
on_current_tab_resources_changed(OrionWebviewTabContent*, PrivacyPopover* self)
{
    schedule_tracker_refresh(self);
}

static void
on_current_tab_loading_changed(
    OrionWebviewTabContent* content, GParamSpec*, PrivacyPopover* self)
{
    if (!orion_webview_tab_content_get_is_loading(content))
        schedule_tracker_refresh(self);
}

static void
set_current_tab_content(PrivacyPopover* self, OrionWebviewTabContent* content)
{
    auto* previous = static_cast<OrionWebviewTabContent*>(
        g_weak_ref_get(&self->current_tab_content));
    if (previous == content) {
        if (previous)
            g_object_unref(previous);
        return;
    }

    if (previous) {
        if (self->resources_changed_handler)
            g_signal_handler_disconnect(previous, self->resources_changed_handler);
        if (self->loading_changed_handler)
            g_signal_handler_disconnect(previous, self->loading_changed_handler);
        g_object_unref(previous);
    }

    self->resources_changed_handler = 0;
    self->loading_changed_handler = 0;
    g_weak_ref_set(&self->current_tab_content, content);

    if (!content)
        return;

    self->resources_changed_handler = g_signal_connect(
        content,
        "current-page-resources-changed",
        G_CALLBACK(on_current_tab_resources_changed),
        self);
    self->loading_changed_handler = g_signal_connect(
        content,
        "notify::is-loading",
        G_CALLBACK(on_current_tab_loading_changed),
        self);
}

static void
unbind_website_icon(PrivacyPopover* self)
{
    if (!self->website_icon_binding)
        return;

    g_binding_unbind(self->website_icon_binding);
    g_clear_object(&self->website_icon_binding);
}

static gboolean
transform_tab_icon(
    GBinding*, const GValue* source_value, GValue* target_value, gpointer)
{
    GIcon* icon = G_ICON(g_value_get_object(source_value));
    if (icon) {
        g_value_set_object(target_value, icon);
    } else {
        g_value_take_object(target_value, g_themed_icon_new("globe-alt2-symbolic"));
    }
    return TRUE;
}

static void
bind_website_icon(PrivacyPopover* self, OrionWebviewTabContent* content)
{
    unbind_website_icon(self);

    if (!content) {
        gtk_image_set_from_icon_name(self->website_icon, "globe-alt2-symbolic");
        return;
    }

    GBinding* binding = g_object_bind_property_full(
        content,
        "icon",
        self->website_icon,
        "gicon",
        G_BINDING_SYNC_CREATE,
        transform_tab_icon,
        nullptr,
        nullptr,
        nullptr);
    self->website_icon_binding = G_BINDING(g_object_ref(binding));
}

static void
clear_content_list(PrivacyPopover* self)
{
    while (GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(self->content_list_box)))
        gtk_list_box_remove(self->content_list_box, child);
}

static void
append_content_list_label(PrivacyPopover* self, const char* text)
{
    GtkWidget* label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_margin_top(label, 6);
    gtk_widget_set_margin_bottom(label, 6);
    gtk_widget_set_margin_start(label, 10);
    gtk_widget_set_margin_end(label, 10);
    gtk_list_box_append(self->content_list_box, label);
}

static void
render_active_report(PrivacyPopover* self)
{
    clear_content_list(self);

    if (adw_toggle_group_get_active(self->content_toggle_group) != 1) {
        append_content_list_label(self, _("Blocked request details are not available yet."));
        return;
    }

    if (self->tracker_summary_loading) {
        append_content_list_label(self, _("Loading tracker information…"));
        return;
    }

    if (self->tracker_domains.empty()) {
        append_content_list_label(
            self,
            self->tracker_status_message.empty()
                ? _("No trackers found for this website.")
                : self->tracker_status_message.c_str());
        return;
    }

    for (const std::string& domain : self->tracker_domains)
        append_content_list_label(self, domain.c_str());
}

static void
update_tracker_toggle_label(PrivacyPopover* self)
{
    const int count = static_cast<int>(self->tracker_domains.size());
    std::string text = orion::text::translatef("Blocked Trackers: %1$d", count);
    adw_toggle_set_label(self->trackers_toggle, text.c_str());
}

static void
on_content_toggle_group_active_changed(
    AdwToggleGroup*, GParamSpec*, PrivacyPopover* self)
{
    render_active_report(self);
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

struct TrackerSummaryRequest {
    orion::gtk::WeakRefWrapper popover;
    guint64 generation;
    std::string first_party_domain;
    std::unordered_set<std::string> resource_domains;
};

static void
get_itp_summary_ready_cb(GObject* source_object, GAsyncResult* result, gpointer user_data)
{
    std::unique_ptr<TrackerSummaryRequest> request(
        static_cast<TrackerSummaryRequest*>(user_data));

    GError* error = nullptr;
    GList* summary = webkit_network_session_get_itp_summary_finish(
        WEBKIT_NETWORK_SESSION(source_object), result, &error);

    auto popover_ref = request->popover.lock();
    if (!popover_ref) {
        if (error)
            g_error_free(error);
        g_list_free_full(
            summary,
            reinterpret_cast<GDestroyNotify>(webkit_itp_third_party_unref));
        return;
    }

    PrivacyPopover* self = PRIVACY_POPOVER(popover_ref.get());
    if (request->generation != self->tracker_summary_generation) {
        if (error)
            g_error_free(error);
        g_list_free_full(
            summary,
            reinterpret_cast<GDestroyNotify>(webkit_itp_third_party_unref));
        return;
    }

    self->tracker_summary_loading = false;
    self->tracker_domains.clear();
    self->tracker_status_message.clear();

    if (error) {
        orion::log_warning("Failed to get ITP summary: %s", error->message);
        self->tracker_status_message = _("Tracker information could not be loaded.");
        g_error_free(error);
    } else {
        // last_update_time is session history. Use it only for presentation order,
        // never as proof that a tracker appeared in this particular navigation.
        std::vector<std::pair<std::string, gint64>> matching_domains;
        std::set<std::string> seen_domains;

        for (GList* item = summary; item; item = item->next) {
            auto* third_party = static_cast<WebKitITPThirdParty*>(item->data);
            const char* third_party_domain = webkit_itp_third_party_get_domain(third_party);
            if (!third_party_domain || !*third_party_domain)
                continue;

            std::string domain = third_party_domain;
            std::ranges::transform(domain, domain.begin(), [](unsigned char ch) {
                return static_cast<char>(g_ascii_tolower(ch));
            });
            if (!request->resource_domains.contains(domain))
                continue;

            for (GList* fp_item = webkit_itp_third_party_get_first_parties(third_party);
                 fp_item;
                 fp_item = fp_item->next) {
                auto* first_party = static_cast<WebKitITPFirstParty*>(fp_item->data);
                const char* first_party_domain = webkit_itp_first_party_get_domain(first_party);
                if (!first_party_domain
                    || g_ascii_strcasecmp(first_party_domain, request->first_party_domain.c_str()))
                    continue;

                // A granted Storage Access API exception means this third party is
                // currently allowed to use its website data under the first party.
                if (webkit_itp_first_party_get_website_data_access_allowed(first_party))
                    continue;

                if (seen_domains.insert(domain).second) {
                    GDateTime* last_updated =
                        webkit_itp_first_party_get_last_update_time(first_party);
                    matching_domains.emplace_back(
                        std::move(domain),
                        last_updated ? g_date_time_to_unix(last_updated) : -1);
                }
                break;
            }
        }

        std::ranges::sort(matching_domains, [](const auto& lhs, const auto& rhs) {
            if (lhs.second != rhs.second)
                return lhs.second > rhs.second;
            return lhs.first < rhs.first;
        });
        self->tracker_domains.reserve(matching_domains.size());
        for (auto& [domain, last_updated] : matching_domains)
            self->tracker_domains.push_back(std::move(domain));
    }

    g_list_free_full(
        summary,
        reinterpret_cast<GDestroyNotify>(webkit_itp_third_party_unref));

    update_tracker_toggle_label(self);
    render_active_report(self);
}

#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#endif // ENABLE_ADW_17_API

GtkPopover*
privacy_popover_new()
{
    PrivacyPopover* self = PRIVACY_POPOVER(g_object_new(PRIVACY_POPOVER_TYPE, NULL));
    
    // Select trackers toggle by default for the time being until
    // blocked requests toggle is implemeted
    adw_toggle_group_set_active(self->content_toggle_group, 1);

    return GTK_POPOVER(self);
}

void
privacy_popover_refresh(PrivacyPopover* self, MainWindow* main_window)
{
    g_return_if_fail(PRIVACY_IS_POPOVER(self));
    g_return_if_fail(MAIN_IS_WINDOW(main_window));

#ifdef ENABLE_ADW_17_API
    cancel_scheduled_tracker_refresh(self);
    ++self->tracker_summary_generation;
    self->tracker_summary_loading = false;
    self->tracker_domains.clear();
    self->tracker_status_message.clear();

    OrionWebviewTabContent* content = main_window_get_current_tab_content(main_window);
    set_current_tab_content(self, content);
    bind_website_icon(self, content);
    if (!content) {
        gtk_label_set_label(self->website_label, "");
        self->tracker_status_message = _("No website is selected.");
        update_tracker_toggle_label(self);
        render_active_report(self);
        return;
    }

    const std::string uri = orion_webview_tab_content_get_uri(content).uri();

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    const std::string first_party_domain = orion::registrable_domain_from_uri(uri);
    gtk_label_set_label(self->website_label, first_party_domain.c_str());

    if (first_party_domain.empty()) {
        self->tracker_status_message = _("Tracker information is unavailable for this page.");
        update_tracker_toggle_label(self);
        render_active_report(self);
        return;
    }

    WebKitWebView* web_view = orion_webview_tab_content_get_webview(content);
    WebKitNetworkSession* network_session = web_view
        ? webkit_web_view_get_network_session(web_view)
        : nullptr;
    if (!network_session) {
        self->tracker_status_message = _("Tracker information is unavailable for this page.");
        update_tracker_toggle_label(self);
        render_active_report(self);
        return;
    }

    if (!webkit_network_session_get_itp_enabled(network_session)) {
        self->tracker_status_message = _("Tracking prevention is disabled for this website.");
        update_tracker_toggle_label(self);
        render_active_report(self);
        return;
    }

    self->tracker_summary_loading = true;
    adw_toggle_set_label(self->trackers_toggle, _("Loading trackers…"));
    render_active_report(self);

    auto* request = new TrackerSummaryRequest {
        .popover = orion::gtk::WeakRefWrapper(self),
        .generation = self->tracker_summary_generation,
        .first_party_domain = first_party_domain,
        .resource_domains = orion_webview_tab_content_get_current_page_resource_domains(content),
    };
    webkit_network_session_get_itp_summary(
        network_session, nullptr, get_itp_summary_ready_cb, request);
#else
    gtk_label_set_label(self->website_label, uri.c_str());
    self->tracker_status_message = _("Tracker information is unavailable in this build.");
    update_tracker_toggle_label(self);
    render_active_report(self);
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#else
    (void)main_window;
#endif // ENABLE_ADW_17_API
}

static void
privacy_popover_init(PrivacyPopover* self)
{
#ifdef ENABLE_ADW_17_API
    std::construct_at(&self->tracker_domains);
    std::construct_at(&self->tracker_status_message);
    g_weak_ref_init(&self->current_tab_content, nullptr);
#endif
    gtk_widget_init_template(GTK_WIDGET(self));
}

static void
privacy_popover_finalize(GObject* object)
{
#ifdef ENABLE_ADW_17_API
    PrivacyPopover* self = PRIVACY_POPOVER(object);
    cancel_scheduled_tracker_refresh(self);
    set_current_tab_content(self, nullptr);
    g_weak_ref_clear(&self->current_tab_content);
    unbind_website_icon(self);
    std::destroy_at(&self->tracker_domains);
    std::destroy_at(&self->tracker_status_message);
#endif
    G_OBJECT_CLASS(privacy_popover_parent_class)->finalize(object);
}

static void
privacy_popover_class_init(PrivacyPopoverClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = privacy_popover_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(
        widget_class, GRESOURCE_PREFIX "/src/window/privacy_popover.ui");

#ifdef ENABLE_ADW_17_API
    gtk_widget_class_bind_template_child(widget_class, PrivacyPopover, website_icon);
    gtk_widget_class_bind_template_child(widget_class, PrivacyPopover, website_label);
    gtk_widget_class_bind_template_child(widget_class, PrivacyPopover, content_toggle_group);
    gtk_widget_class_bind_template_child(widget_class, PrivacyPopover, requests_toggle);
    gtk_widget_class_bind_template_child(widget_class, PrivacyPopover, trackers_toggle);
    gtk_widget_class_bind_template_child(widget_class, PrivacyPopover, content_list_box);
    gtk_widget_class_bind_template_callback(
        widget_class, on_content_toggle_group_active_changed);
#endif
}
