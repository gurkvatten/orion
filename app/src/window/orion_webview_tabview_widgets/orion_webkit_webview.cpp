#define ENABLE_VERBOSE_LOGS 0

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>

#include "orion_webkit_webview.h"

#include "app.h"
#include "webextensions/context_menu_host.h"
#include "webextensions/profile_host.h"
#include "webextensions/webview_host.h"

#include <common/src/defer.h>
#include <core/downloader.h>
#include <core/settings.h>
#include <common/src/uuid.h>
#include <common/src/string_util.h>

#include "window/main_window/main_window.h"
#include "window/main_window/orion_window_iface.h"
#include "window/main_window/js_sidebar/js_sidebar.h"
#include "window/auth_dialog.h"
#include "window/permissions_request_dialog.h"
#include "window/permissions_request_binary_dialog.h"
#include "window/visit_unsecure_website_dialog.h"
#include "window/remember_password_dialog.h"
#include "window/update_password_dialog.h"

#include "util/text_util.h"
#include "util/file_util.h"
#include "util/html_injector.h"
#include "util/ref_util.h"
#include "util/enum_util.h"
#include "util/action_util.h"
#include "util/variant_util.h"
#include "util/jsc_util.h"

#include "window/orion_webview_tabview_widgets/scripts/safeguard_scripts.h"
#include "window/orion_webview_tabview_widgets/scripts/page_tweaks_scripts.h"
#include "certificate_error_page_helper.h"
#include "error_page_helper.h"
#include "context_menu_helper.h"
#include "scripts/autofill_scripts.h"
#include "orion_webview_tab_content.h"

const std::vector<std::string_view> webview_supported_mime_types {
    "text/html",
    "application/pdf",
    "application/json",
    "image/"
    //TODO: FIXME: properly fill out this list
};

static const char* ORION_SHOULD_CLICKED_LINK_OPEN_NEW_TAB_KEY = "orion-should-clicked-link-open-new-tab";

namespace {
    struct CppWrapper {
        bool ignore_next_download;
        std::string current_ua_override; // "" = factory default, non-empty = active override
        orion::gtk::GObjectPtr last_tls_certificate;
        GTlsCertificateFlags last_tls_errors{G_TLS_CERTIFICATE_NO_FLAGS};
        std::string last_tls_uri;
        
        std::vector<WebKitUserStyleSheet*> page_tweaks_font_size_style_sheets;
        std::vector<WebKitUserStyleSheet*> page_tweaks_font_family_style_sheets;
        orion::WebKitUserStyleSheetMap page_tweaks_element_blockers;

        orion::gtk::WeakRefWrapper last_profile_data_holder;

        std::string page_tweaks_epoch;

        orion::PrivateMode private_mode;
    };

    // Must match the UA set in webview_factory_host.cpp to preserve Cloudflare-fix behaviour.
    constexpr const char* k_default_user_agent =
        "Mozilla/5.0 (X11; Ubuntu; Linux x86_64) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/60.5 Safari/605.1.15";

    // UA preset strings keyed to UserAgentWebsiteSetting enum values.
    constexpr const char* k_ua_chrome =
        "Mozilla/5.0 (X11; Ubuntu; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36";
    constexpr const char* k_ua_firefox =
        "Mozilla/5.0 (X11; Ubuntu; Linux x86_64) Gecko/20100101 Firefox/126.0";
    constexpr const char* k_ua_safari =
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Safari/605.1.15";
    constexpr const char* k_ua_safari_iphone =
        "Mozilla/5.0 (iPhone; CPU iPhone OS 17_4_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.4.1 Mobile/15E148 Safari/604.1";
    constexpr const char* k_ua_safari_ipad =
        "Mozilla/5.0 (iPad; CPU OS 17_4_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.4.1 Mobile/15E148 Safari/604.1";
    constexpr const char* k_ua_edge =
        "Mozilla/5.0 (X11; Ubuntu; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36 Edg/125.0.0.0";

    // Maps a resolved UserAgentWebsiteSetting to the actual UA string.
    // Returns "" for automatic/not_set → the factory default UA (k_default_user_agent) is used.
    static std::string resolve_ua_string(
        orion::settings::UserAgentWebsiteSetting setting,
        const std::string& custom_ua)
    {
        using UA = orion::settings::UserAgentWebsiteSetting;
        switch (setting) {
            case UA::chrome:        return k_ua_chrome;
            case UA::firefox:       return k_ua_firefox;
            case UA::safari:        return k_ua_safari;
            case UA::safari_iphone: return k_ua_safari_iphone;
            case UA::safari_ipad:   return k_ua_safari_ipad;
            case UA::edge:          return k_ua_edge;
            case UA::custom:        return custom_ua;
            default:                return ""; // automatic / not_set → factory default
        }
    }
}

struct _OrionWebkitWebview
{
    WebKitWebView parent;

    gboolean constructed;

    WebKitHitTestResult* last_hit_test_result;

    orion::TlsStatus tls_status;

    GSimpleAction* go_back_action;
    GSimpleAction* go_forward_action;
    gboolean is_probably_readerable;

    GtkExpressionWatch* private_mode_watch;
    GtkExpressionWatch* data_holder_watch;
    gboolean strip_links_in_non_private;

    gdouble last_mouse_x; // X coordinate in the main window's coordinate space
    gdouble last_mouse_y; // Y coordinate in the main window's coordinate space


    WebKitUserStyleSheet* page_tweaks_dark_mode_style_sheet;

    GtkExpressionWatch* page_tweaks_font_size_style_sheets_watch;
    GtkExpressionWatch* page_tweaks_dark_mode_style_sheet_watch;
    GtkExpressionWatch* page_tweaks_font_family_style_sheets_watch;
    GtkExpressionWatch* page_tweaks_disable_sticky_list_watch; //is also our source of truth for whether this webview's disable sticky headers logic is enabled
    GtkExpressionWatch* page_tweaks_element_blockers_watch;

    
    CppWrapper cpp_data;
};

struct PermissionText {
    std::string primary;
    std::string secondary;
};

enum {
    PROP_0,
    //PROP_IS_PROBABLY_READERABLE,
    PROP_CAN_GO_BACK,
    PROP_CAN_GO_FORWARD,
    PROP_PAGE_TWEAKS_ENABLED,
    PROP_TLS_STATUS,
    N_PROPS,
};

enum {
    SIG_GO_BACK,
    SIG_GO_FORWARD,
    N_SIGNALS,
};

static GParamSpec* obj_properties[N_PROPS] = {NULL, };

static guint obj_signals[N_SIGNALS] = {0};

G_DEFINE_TYPE(OrionWebkitWebview, orion_webkit_webview, WEBKIT_TYPE_WEB_VIEW)

void
orion_webkit_webview_ignore_next_download(OrionWebkitWebview* webview)
{
    orion::log_verbose("webview should ignore next download");
    webview->cpp_data.ignore_next_download = true;
}

GTlsCertificate*
orion_webkit_webview_get_last_tls_certificate(OrionWebkitWebview* webview)
{
    return G_TLS_CERTIFICATE(webview->cpp_data.last_tls_certificate.get());
}

GTlsCertificateFlags
orion_webkit_webview_get_last_tls_errors(OrionWebkitWebview* webview)
{
    return webview->cpp_data.last_tls_errors;
}

const char*
orion_webkit_webview_get_last_tls_uri(OrionWebkitWebview* webview)
{
    return webview->cpp_data.last_tls_uri.c_str();
}

static gboolean
orion_webkit_webview_is_connection_secure(WebKitWebView* web_view)
{
    GTlsCertificateFlags errors = G_TLS_CERTIFICATE_NO_FLAGS;

    gboolean is_https = webkit_web_view_get_tls_info(web_view, NULL, &errors);

    return is_https && errors == G_TLS_CERTIFICATE_NO_FLAGS;
}


static void
orion_webkit_webview_on_print_finished(WebKitPrintOperation* self,  gpointer user_data)
{
    orion::log_debug("Handling print finished signal");
}

static void 
orion_webkit_webview_on_print_failed(WebKitPrintOperation* self, GError* error, gpointer user_data)
{
    orion::log_debug("Handling print failed signal");
}


static void
orion_webkit_webview_authenticated(WebKitAuthenticationRequest* self, WebKitCredential* credential, gpointer user_data){
    orion::log_debug("Handling authentificated signal");
    const gchar* realm = webkit_authentication_request_get_realm(self);
    const gchar* host = webkit_authentication_request_get_host(self);
    const gchar* user = webkit_credential_get_username(credential);

    orion::log_debug("Auth OK - for realm: \"%s\", host: \"%s\", username: \"%s\"", realm, host, user);
}

static void
orion_webkit_webview_cancelled(WebKitAuthenticationRequest* self, gpointer user_data){
    orion::log_debug("Handling canceled signal");
}

static void
handle_tls_certificate_flags(GTlsCertificateFlags flags)
{
    if (flags == G_TLS_CERTIFICATE_NO_FLAGS) {
        orion::log_debug("%s: no certificate errors", __func__);
        return;
    }

    if (flags & G_TLS_CERTIFICATE_UNKNOWN_CA) {
        orion::log_debug("%s: unknown certificate authority", __func__);
    }

    if (flags & G_TLS_CERTIFICATE_BAD_IDENTITY) {
        orion::log_debug("%s: bad certificate identity", __func__);
    }

    if (flags & G_TLS_CERTIFICATE_NOT_ACTIVATED) {
        orion::log_debug("%s: certificate not activated yet", __func__);
    }

    if (flags & G_TLS_CERTIFICATE_EXPIRED) {
        orion::log_debug("%s: certificate has expired", __func__);
    }

    if (flags & G_TLS_CERTIFICATE_REVOKED) {
        orion::log_debug("%s: certificate has been revoked", __func__);
    }

    if (flags & G_TLS_CERTIFICATE_INSECURE) {
        orion::log_debug("%s: certificate uses insecure algorithm", __func__);
    }

    if (flags & G_TLS_CERTIFICATE_GENERIC_ERROR) {
        orion::log_debug("%s: generic certificate error", __func__);
    }
}

//Start of webviews virtual methods =====================================

static void
launch_external_uri(const std::string& uri, GtkWidget* parent_window)
{
    GtkUriLauncher* launcher = gtk_uri_launcher_new(uri.c_str());
    gtk_uri_launcher_launch(launcher, GTK_WINDOW(parent_window), nullptr,
        [](GObject* source, GAsyncResult* result, gpointer) {
            GError* error = nullptr;
            if (!gtk_uri_launcher_launch_finish(GTK_URI_LAUNCHER(source), result, &error)) {
                if (error) {
                    orion::log_warning("Failed to launch external URI: %s", error->message);
                    g_error_free(error);
                }
            }
        }, nullptr);
    g_object_unref(launcher);
}

static void
show_external_app_ask_dialog(
    const std::string& website,
    GtkWidget* parent,
    std::function<void(bool allowed)> on_response)
{
    AdwDialog* alert = adw_alert_dialog_new(nullptr, nullptr);
    if (website.empty()) {
        adw_alert_dialog_set_heading(ADW_ALERT_DIALOG(alert), _("Open in external application?"));
    } else {
        gchar* heading = g_strdup_printf(_("Allow \"%s\" to open an external application?"), website.c_str());
        adw_alert_dialog_set_heading(ADW_ALERT_DIALOG(alert), heading);
        g_free(heading);
    }
    adw_alert_dialog_set_body(ADW_ALERT_DIALOG(alert), _("You can change this in Website Settings."));
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(alert),
        "deny",  _("Don't Allow"),
        "allow", _("Allow"),
        nullptr);
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(alert), "allow");

    auto* cb = new std::function<void(bool)>(std::move(on_response));
    g_signal_connect_data(alert, "response",
        G_CALLBACK(+[](AdwAlertDialog*, const char* resp, std::function<void(bool)>* data) {
            (*data)(std::string_view(resp) == "allow");
            delete data;
        }),
        cb, [](gpointer, GClosure*){}, G_CONNECT_DEFAULT);

    adw_dialog_present(alert, parent);
}

static gboolean
orion_webkit_webview_authenticate(WebKitWebView* web_view, WebKitAuthenticationRequest* request)
{
    const gchar* realm = webkit_authentication_request_get_realm(request);
    WebKitSecurityOrigin* origin =  webkit_authentication_request_get_security_origin(request);

    orion::log_debug("Authentification challenge - for realm: \"%s\", origin: \"%s\"",
        realm, webkit_security_origin_to_string(origin));

    g_signal_connect(request, "authenticated", G_CALLBACK(orion_webkit_webview_authenticated), NULL);
    g_signal_connect(request, "cancelled", G_CALLBACK(orion_webkit_webview_cancelled), NULL);

    AdwDialog* dialog = NULL;
    dialog = auth_dialog_new(request, orion_webkit_webview_is_connection_secure(web_view));
    GtkWidget *toplevel = gtk_widget_get_ancestor(GTK_WIDGET(web_view), GTK_TYPE_WINDOW);

    if (dialog != NULL) {
		adw_dialog_present(dialog, GTK_WIDGET(toplevel));
	}

    return TRUE;
}

static void
orion_webkit_webview_close(WebKitWebView* web_view)
{
}

static std::optional<std::string>
context_menu_optional_string(const gchar* value)
{
    if(!value || !*value) return std::nullopt;
    return std::string(value);
}

static std::string
context_menu_media_type(const std::string& uri)
{
    std::string lowercase = uri;
    std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
        [](unsigned char character) { return std::tolower(character); });
    const size_t query = lowercase.find_first_of("?#");
    if(query != std::string::npos) lowercase.resize(query);
    static const std::vector<std::string> audio_extensions {
        ".aac", ".flac", ".m4a", ".mp3", ".oga", ".ogg", ".opus", ".wav", ".weba"
    };
    for(const auto& extension : audio_extensions) {
        if(lowercase.ends_with(extension)) return "audio";
    }
    return "video";
}

static WebExtensions::WebExtensionContextMenuHitTest
decode_context_menu_hit_test(
    WebKitWebView* web_view,
    WebKitContextMenu* context_menu,
    WebKitHitTestResult* hit_test_result)
{
    WebExtensions::WebExtensionContextMenuHitTest hit_test;
    hit_test.page_url = context_menu_optional_string(
        webkit_web_view_get_uri(web_view));
    hit_test.frame_url = hit_test.page_url;
    hit_test.frame_id = 0;
    hit_test.editable =
        webkit_hit_test_result_context_is_editable(hit_test_result);

    if(webkit_hit_test_result_context_is_link(hit_test_result)) {
        hit_test.contexts.push_back("link");
        hit_test.link_url = context_menu_optional_string(
            webkit_hit_test_result_get_link_uri(hit_test_result));
        hit_test.link_text = context_menu_optional_string(
            webkit_hit_test_result_get_link_label(hit_test_result));
    }
    if(webkit_hit_test_result_context_is_image(hit_test_result)) {
        hit_test.contexts.push_back("image");
        hit_test.src_url = context_menu_optional_string(
            webkit_hit_test_result_get_image_uri(hit_test_result));
    }
    if(webkit_hit_test_result_context_is_media(hit_test_result)) {
        const std::string media_uri = context_menu_optional_string(
            webkit_hit_test_result_get_media_uri(hit_test_result)).value_or("");
        hit_test.media_type = context_menu_media_type(media_uri);
        hit_test.contexts.push_back(*hit_test.media_type);
        if(!media_uri.empty()) hit_test.src_url = media_uri;
    }
    if(hit_test.editable) hit_test.contexts.push_back("editable");
    if(webkit_hit_test_result_context_is_selection(hit_test_result)) {
        hit_test.contexts.push_back("selection");
    }
    if(hit_test.contexts.empty()) hit_test.contexts.push_back("page");

    if(GdkEvent* event = webkit_context_menu_get_event(context_menu)) {
        const GdkModifierType state = gdk_event_get_modifier_state(event);
        if(state & GDK_SHIFT_MASK) hit_test.modifiers.push_back("Shift");
        if(state & GDK_ALT_MASK) hit_test.modifiers.push_back("Alt");
        if(state & GDK_CONTROL_MASK) hit_test.modifiers.push_back("Ctrl");
        const GdkEventType type = gdk_event_get_event_type(event);
        if(type == GDK_BUTTON_PRESS || type == GDK_BUTTON_RELEASE) {
            const guint button = gdk_button_event_get_button(event);
            if(button > 0) hit_test.button = static_cast<int64_t>(button - 1);
        }
    }
    return hit_test;
}

static gboolean
orion_webkit_webview_context_menu(WebKitWebView* web_view, WebKitContextMenu* context_menu, WebKitHitTestResult* hit_test_result)
{
    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(web_view));
    if (!ORION_IS_WINDOW(root)) {
        orion::log_warning("orion_webkit_webview_context_menu: cannot find window");
        return FALSE;
    }
    OrionWindow* win = ORION_WINDOW(root);

    std::string link_uri{};
    if (webkit_hit_test_result_context_is_link(hit_test_result)) {
        const gchar* link_uri_c = webkit_hit_test_result_get_link_uri(hit_test_result);
        if (link_uri_c) {
            link_uri = std::string(link_uri_c);
        }
        orion::log_debug("orion_webkit_webview_context_menu: link_uri=%s", link_uri.c_str());
    }

    GdkEvent* event = webkit_context_menu_get_event(context_menu);
    double native_x, native_y;
    if(!event || !gdk_event_get_position(event, &native_x, &native_y))
    {
        native_x = 0;
        native_y = 0;
    }

    GtkWidget* self_widget = GTK_WIDGET(web_view);

    //TODO: possibly cause the popover to attach to tab content instead,
    //native coords produce a slight error positioning against the main window for an unknown reason
    ContextMenuHelper helper(ORION_WEBKIT_WEBVIEW(web_view), win, link_uri, native_x, native_y, true);
    std::vector<HitTestUtil::Element> elements = HitTestUtil::get_elements_from_hit_test(hit_test_result);

    WebKitSettings* settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(web_view));
    gboolean enable_inspector = webkit_settings_get_enable_developer_extras(settings);

    helper.populate(context_menu, elements, enable_inspector);
    const auto decoded_hit_test = decode_context_menu_hit_test(
        web_view, context_menu, hit_test_result);
    auto profile_world =
        orion_window_get_webview_factory(win).parent_world().lock();
    if(!profile_world) return false;
    auto profile_host = orion_app_get_or_create_profile_host(
        profile_world->profile_id());
    auto* context_menu_host = orion::app_context_menu_host(
        *profile_host, profile_world->private_mode());
    auto web_view_bridge = orion::app_web_view_bridge(
        reinterpret_cast<OpaqueWebView*>(web_view)).lock();
    return context_menu_host && web_view_bridge
        && context_menu_host->populate_context_menu(
            *web_view_bridge,
            reinterpret_cast<OpaqueContextMenu*>(context_menu),
            decoded_hit_test);
}

static void
orion_webkit_webview_context_menu_dismissed(WebKitWebView* web_view)
{
    orion::log_debug("orion_webkit_webview_context_menu_dismissed");
    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(web_view));
    if(ORION_IS_WINDOW(root)) {
        auto profile_world = orion_window_get_webview_factory(
            ORION_WINDOW(root)).parent_world().lock();
        if(!profile_world) return;
        auto profile_host = orion_app_get_or_create_profile_host(
            profile_world->profile_id());
        auto* context_menu_host = orion::app_context_menu_host(
            *profile_host, profile_world->private_mode());
        auto web_view_bridge = orion::app_web_view_bridge(
            reinterpret_cast<OpaqueWebView*>(web_view)).lock();
        if(context_menu_host && web_view_bridge) {
            context_menu_host->context_menu_dismissed(
                *web_view_bridge);
        }
    }
}

static void
use_policy_decision_with_autoplay(OrionWebkitWebview* self, WebKitPolicyDecision* decision);

static void
on_scrape_username_password_finish(GObject* source, GAsyncResult* res, gpointer user_data)
{
    WebKitPolicyDecision* decision = WEBKIT_POLICY_DECISION(user_data);
    defer([decision]{ g_object_unref(decision); });

    orion::log_verbose("on_scrape_username_password_finish");
    WebKitWebView* webview = WEBKIT_WEB_VIEW(source);
    OrionWebkitWebview* orion_webview = ORION_WEBKIT_WEBVIEW(source);

    orion::log_verbose("on_scrape_username_password_finish: webview_type=%s", G_OBJECT_TYPE_NAME(orion_webview));

    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(orion_webview)));
    if (!win) {
        orion::log_warning("on_scrape_username_password_finish: cannot find window");
        return;
    }
    orion::ProfileId profile_id = orion_window_get_profile_id(win);

    JSCValue* value = webkit_web_view_evaluate_javascript_finish(webview, res, NULL);

    if (value && jsc_value_is_object(value)) {
        JSCValue* user = jsc_value_object_get_property(value, "username");
        JSCValue* pass = jsc_value_object_get_property(value, "password");

        if (jsc_value_is_string(user) && jsc_value_is_string(pass)) {
            std::string username = orion::jsc::value_to_string(user);
            std::string password = orion::jsc::value_to_string(pass);

            if (!username.empty() && !password.empty()) {
                // Possible credentials found on the current webpage
                orion::Uri uri = orion::Uri::parse_strict(webkit_web_view_get_uri(webview));
                std::string url = uri.scheme() + "://" + uri.authority();
                orion::log_verbose("on_scrape_username_password_finish: scraped url=%s username=%s password=%s",
                    url.c_str(), username.c_str(), password.c_str());

                // Check if we already have this site with this password saved
                orion::GetPasswordItemIdsQuery query{
                    .profile_id = profile_id,
                    .authority = uri.authority(),
                    .username = username,
                };

                orion::gtk::WeakRefWrapper webview_weak(webview);

                global_core_api->get_password_item_ids(query,
                    [url,username,password,webview_weak,profile_id,uri](orion::GetPasswordItemIdsReply reply){
                    VERIFY_LOG(reply.err);

                    auto password_ids = reply.ids;

                    // Check "never save password" record for this website
                    orion::GetNeverSavePasswordItemQuery never_save_query{
                        .profile_id = profile_id,
                        .authority = uri.authority(),
                    };

                    global_core_api->get_never_save_password_item(never_save_query,
                        [url,username,password,webview_weak,password_ids]
                        (orion::GetNeverSavePasswordItemReply never_save_reply){
                        VERIFY_LOG(never_save_reply.err);

                        auto webview_strong = webview_weak.lock();
                        if (!webview_strong) return;
                        WebKitWebView* webview = WEBKIT_WEB_VIEW(webview_strong.get());

                        // Not saved yet, show "Would you like to save this password?" dialog
                        // but only if the website is not marked as "never save password"
                        if (password_ids.empty()) {
                            if (!never_save_reply.never_save) {
                                RememberPasswordDialogArgs args{
                                    .url = url,
                                    .username = username,
                                    .password = password,
                                };
                                
                                AdwDialog* remember_dialog = remember_password_dialog_new(args);
                                adw_dialog_present(remember_dialog, GTK_WIDGET(webview));
                                return;
                            } else {
                                orion::log_debug(
                                    "on_scrape_username_password_finish: website marked as never save password, "
                                    "not showing save dialog"
                                );
                            }
                        }

                        global_core_api->get_password_items(password_ids,
                            [password,webview_weak](orion::GetPasswordItemsReply reply){
                            VERIFY_LOG(reply.err);

                            auto webview_strong = webview_weak.lock();
                            if (!webview_strong) return;
                            WebKitWebView* webview = WEBKIT_WEB_VIEW(webview_strong.get());

                            if (reply.items.empty()) {
                                orion::log_warning("on_scrape_username_password_finish: password items not found"
                                    " for existing ids");
                                return;
                            }
                            orion::PasswordItemDto item = reply.items[0];
                            if (item.password == password) {
                                orion::log_debug("on_scrape_username_password_finish: password item already exists"
                                    " for this site and username, not showing save dialog");
                                return;
                            }

                            // Password changed, show "Would you like to update password for this website?" dialog
                            UpdatePasswordDialogArgs args{
                                .password_item_id = item.id,
                                .url = item.url,
                                .username = item.username,
                                .new_password = password,
                            };

                            AdwDialog* remember_dialog = update_password_dialog_new(args);
                            adw_dialog_present(remember_dialog, GTK_WIDGET(webview));
                        });
                    });
                });
            }
        }
    }

    // Allow navigation with autoplay policy attached to this decision (if present).
    use_policy_decision_with_autoplay(orion_webview, decision);
}

// Returns true for navigation types where we want to scrape username/password
// inputs (user-initiated navigations that may carry form credentials).
static bool
ua_nav_needs_password_scrape(WebKitNavigationType nav_type)
{
    return nav_type == WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED
        || nav_type == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED
        || nav_type == WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED
        || nav_type == WEBKIT_NAVIGATION_TYPE_OTHER;
}

static WebKitAutoplayPolicy
resolve_autoplay_policy(orion::settings::AutoplayWebsiteSetting setting)
{
    using APS = orion::settings::AutoplayWebsiteSetting;
    switch (setting) {
        case APS::allow_all_autoplay:
            return WEBKIT_AUTOPLAY_ALLOW;
        case APS::never_autoplay:
            return WEBKIT_AUTOPLAY_DENY;
        case APS::stop_media_with_sound:
        case APS::not_set:
        default:
            return WEBKIT_AUTOPLAY_ALLOW_WITHOUT_SOUND;
    }
}

static void
use_policy_decision_with_autoplay(OrionWebkitWebview* self, WebKitPolicyDecision* decision)
{
    gpointer autoplay_data = g_object_get_data(G_OBJECT(decision), "orion-autoplay-policy");



    //safety checks so that we don't mess up back forward cache state, form submissions etc.
    if (self->cpp_data.private_mode == orion::PrivateMode::yes ||
        self->strip_links_in_non_private)
    {
        if(WEBKIT_IS_NAVIGATION_POLICY_DECISION(decision))
        {
            WebKitNavigationPolicyDecision* nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);

            WebKitNavigationAction* nav_action = webkit_navigation_policy_decision_get_navigation_action(nav_decision);

            WebKitNavigationType nav_type = webkit_navigation_action_get_navigation_type(nav_action);

            bool is_safe_nav_type = false;

            switch(nav_type) {
                case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED:
                case WEBKIT_NAVIGATION_TYPE_RELOAD:
                case WEBKIT_NAVIGATION_TYPE_OTHER: //this should be a uri sourced from ui process
                    is_safe_nav_type = true;
                    break;
                case WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED:
                case WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED:
                case WEBKIT_NAVIGATION_TYPE_BACK_FORWARD:
                    break;
            }
            
            if(is_safe_nav_type)
            {
                WebKitURIRequest* request = webkit_navigation_action_get_request(nav_action);

                if(0 == g_strcmp0("GET", webkit_uri_request_get_http_method(request)))
                {
                    const gchar* original_uri = webkit_uri_request_get_uri(request);
                    std::string stripped_uri = orion::strip_tracking_data(original_uri);

                    if(0 != g_strcmp0(original_uri, stripped_uri.c_str()))
                    {
                        webkit_policy_decision_ignore(decision);
                        webkit_uri_request_set_uri(request, stripped_uri.c_str());
                        webkit_web_view_load_request(WEBKIT_WEB_VIEW(self), request);
                        return;
                    }
                }
            }
            
        }
    }


    int policy_plus_one = GPOINTER_TO_INT(autoplay_data);
    if (policy_plus_one > 0) {
        WebKitAutoplayPolicy policy = static_cast<WebKitAutoplayPolicy>(policy_plus_one - 1);
        WebKitWebsitePolicies* policies =
            webkit_website_policies_new_with_policies("autoplay", policy, NULL);
        if (policies) {
            webkit_policy_decision_use_with_policies(decision, policies);
            g_object_unref(policies);
            return;
        }
    }

    webkit_policy_decision_use(decision);
}

static void
attach_autoplay_policy_to_decision(
    WebKitPolicyDecision* decision,
    orion::settings::AutoplayWebsiteSetting setting)
{
    WebKitAutoplayPolicy policy = resolve_autoplay_policy(setting);
    // +1 because WEBKIT_AUTOPLAY_ALLOW is 0 and 0 is also "no data" for GObject data.
    int policy_plus_one = static_cast<int>(policy) + 1;
    g_object_set_data(
        G_OBJECT(decision),
        "orion-autoplay-policy",
        GINT_TO_POINTER(policy_plus_one));
}


static void
apply_user_agent_and_continue(
    orion::gtk::WeakRefWrapper webview_weak,
    std::shared_ptr<WebKitPolicyDecision> decision_guard,
    const std::string& uri_str,
    const std::string& wanted_ua,
    WebKitNavigationType nav_type)
{
    auto strong = webview_weak.lock();
    if (!strong) {
        return; // decision_guard destructs → g_object_unref
    }
    WebKitWebView* webview = WEBKIT_WEB_VIEW(strong.get());
    OrionWebkitWebview* orion_webview = ORION_WEBKIT_WEBVIEW(strong.get());

    if (wanted_ua != orion_webview->cpp_data.current_ua_override) {
        orion_webview->cpp_data.current_ua_override = wanted_ua;
        WebKitSettings* settings = webkit_web_view_get_settings(webview);
        webkit_settings_set_user_agent(
            settings,
            wanted_ua.empty() ? k_default_user_agent : wanted_ua.c_str());
        orion::log_debug("apply_user_agent_and_continue: UA changed, replaying navigation to %s",
            uri_str.c_str());
        webkit_policy_decision_ignore(decision_guard.get());
        webkit_web_view_load_uri(webview, uri_str.c_str());
        // decision_guard destructs at end of scope → g_object_unref
    } else if (ua_nav_needs_password_scrape(nav_type)) {
        // UA correct, user-initiated navigation — scrape credentials before allowing.
        // Add one ref so that on_scrape_username_password_finish can unref via defer,
        // and decision_guard can also unref at end of this scope.
        g_object_ref(decision_guard.get());
        webkit_web_view_evaluate_javascript(
            webview,
            orion::gtk::get_scrape_username_password_script().c_str(),
            -1, NULL, NULL, NULL,
            on_scrape_username_password_finish, decision_guard.get());
        // decision_guard destructs at end of scope → g_object_unref
        // on_scrape_username_password_finish will also unref via defer
    } else {
        // UA correct, reload / back-forward — no credential scrape needed.
        use_policy_decision_with_autoplay(orion_webview, decision_guard.get());
        // decision_guard destructs at end of scope → g_object_unref
    }
}

static gboolean
open_link_in_preview_window(WebKitWebView* self, const char* new_uri)
{
    OrionWebkitWebview* orion_webview = ORION_WEBKIT_WEBVIEW(self);
    orion::log_debug(
        "on_webview_decide_policy: shift+click detected, opening link in new preview window, last mouse pos: (%f, %f)",
        orion_webview->last_mouse_x, orion_webview->last_mouse_y
    );
    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) {
        orion::log_warning("on_webview_decide_policy: cannot find window");
        return FALSE;
    }
    gdouble x = orion_webview->last_mouse_x;
    gdouble y = orion_webview->last_mouse_y;
    //allowing even a preview to open another preview, the current one will be destroyed anyway
    gtk_widget_activate_action(GTK_WIDGET(win), "win.open-uri-in-preview-window", "(sdd)", new_uri, x, y);
    return TRUE;
}

static gboolean
orion_webkit_webview_decide_policy_navigation(WebKitWebView* self, WebKitPolicyDecision* decision)
{
    WebKitNavigationPolicyDecision* nav_policy = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
    OrionWebkitWebview* orion_webview = ORION_WEBKIT_WEBVIEW(self);

    WebKitNavigationAction* nav_action =
        webkit_navigation_policy_decision_get_navigation_action(nav_policy);
    WebKitURIRequest* request = webkit_navigation_action_get_request(nav_action);

    //marking this request for whether it is based on user gesture or not for use in decide_policy_response to never block user triggered "download" navigations
    g_object_set_data(G_OBJECT(request), "is-user-triggered", GINT_TO_POINTER(webkit_navigation_action_is_user_gesture(nav_action)));

    const char* current_uri = webkit_web_view_get_uri(self);
    const char* new_uri = webkit_uri_request_get_uri(request);

    WebKitNavigationType nav_type = webkit_navigation_action_get_navigation_type(nav_action);

    orion::log_verbose("on_webview_decide_policy: current_uri=%s new_uri=%s nav_type=%d",
        current_uri, new_uri, nav_type);

    unsigned mouse_button = webkit_navigation_action_get_mouse_button(nav_action);
    guint modifiers = webkit_navigation_action_get_modifiers(nav_action);

    bool is_control_left_click = mouse_button == GDK_BUTTON_PRIMARY && (modifiers & GDK_CONTROL_MASK);
    bool is_middle_click = mouse_button == GDK_BUTTON_MIDDLE;
    bool is_shift_left_click = mouse_button == GDK_BUTTON_PRIMARY && (modifiers & GDK_SHIFT_MASK);

    if (is_control_left_click || is_middle_click) {
        // Ctrl+click: open in the link new tab and ignore the decision here
        orion::log_debug("on_webview_decide_policy: Ctrl+click detected, opening link in new tab");
        OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
        if (!win) {
            orion::log_warning("on_webview_decide_policy: cannot find window");
            return FALSE;
        }
        GAction* show_nested_tabs_setting = g_action_map_lookup_action(G_ACTION_MAP(win), "show-nested-tabs");
        GVariant* show_nested_tabs_state = g_action_get_state(show_nested_tabs_setting);
        defer([show_nested_tabs_state]{ g_variant_unref(show_nested_tabs_state); });
        bool show_nested_tabs = g_variant_get_boolean(show_nested_tabs_state);
        GAction* open_new_tab_next_to_current_tab_setting = g_action_map_lookup_action(G_ACTION_MAP(win), "open-new-tab-next-to-current-tab");
        GVariant* open_new_tab_next_to_current_tab_state = g_action_get_state(open_new_tab_next_to_current_tab_setting);
        defer([open_new_tab_next_to_current_tab_state]{ g_variant_unref(open_new_tab_next_to_current_tab_state); });
        bool open_new_tab_next_to_current_tab = g_variant_get_boolean(open_new_tab_next_to_current_tab_state);

        if(show_nested_tabs && open_new_tab_next_to_current_tab)
        {
            gtk_widget_activate_action_variant(GTK_WIDGET(self), "win.open-uri-next-to-current-tab", orion::gtk::VariantWrapper(std::string_view(new_uri)).get());
        }
        else
        {
            gtk_widget_activate_action(GTK_WIDGET(self), "win.open-uri-in-new-tab", "s", new_uri);
        }
        
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }

    if (is_shift_left_click) {
        // Shift+click: open the link in new Preview Window and ignore the decision here
        if (!open_link_in_preview_window(self, new_uri)) {
            return FALSE;
        }
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }

    // Asynchronously resolve the per-site user agent from settings, then either replay
    // the navigation with the new UA or proceed with the password scrape.
    // The decision ref is managed by decision_guard (shared_ptr with g_object_unref deleter)
    // so all async paths, including VERIFY_LOG early returns, correctly release it.
    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) {
        // No window attached (e.g. popup webview before ready_to_show).
        // Credential scraping requires a window to show the dialog, so skip it.
        use_policy_decision_with_autoplay(orion_webview, decision);
        return TRUE;
    }

    orion::ProfileId profile_id = orion_window_get_profile_id(win);
    std::string domain = orion::Uri::parse_strict(new_uri).authority();

    if (domain.empty()) {
        // External app schemes — delegate to system via XDG portal,
        // subject to the per-site ExternalApps permission setting.
        // Only explicitly-known schemes (not Scheme::unknown) are treated as external apps;
        // unknown schemes (about:, javascript:, data:, …) are left to WebKit.
        auto known = orion::Uri::parse_strict(new_uri).known_scheme();

        if (known != orion::Uri::Scheme::unknown && known != orion::Uri::Scheme::empty)
        {
            std::string website = orion::Uri::parse_strict(current_uri ? current_uri : "").authority();

            g_object_ref(decision);
            auto decision_guard = std::shared_ptr<WebKitPolicyDecision>(
                decision, [](WebKitPolicyDecision* d){ g_object_unref(d); });

            orion::gtk::WeakRefWrapper webview_weak(self);
            std::string uri_str(new_uri);

            global_core_api->get_setting_or_default(
                profile_id,
                WEBSITE_SETTING(external_apps_default_setting, website),
                orion::settings::websites::external_apps_default_setting_for_all_websites,
                [decision_guard, webview_weak, uri_str, website]
                (orion::GetSettingReply<orion::settings::ExternalAppsWebsiteSetting> reply)
                {
                    VERIFY_LOG(reply.err);

                    using EAS = orion::settings::ExternalAppsWebsiteSetting;

                    GtkWidget* parent = nullptr;
                    if (auto wv = webview_weak.lock())
                        parent = gtk_widget_get_ancestor(GTK_WIDGET(wv.get()), GTK_TYPE_WINDOW);
                    if (!parent) {
                        OrionApp* app = ORION_APP(g_application_get_default());
                        if (app) parent = GTK_WIDGET(gtk_application_get_active_window(GTK_APPLICATION(app)));

                        //gtk_application_get_active_window can sometimes actually produce NULL even if documentation states otherwise
                        //in that case, our app is not in focus anyway, meaning this isn't experience breaking
                        if(!parent) return; 
                    }

                    switch (reply.value) {
                        case EAS::allow:
                            orion::log_debug("external_apps: launching %s", uri_str.c_str());
                            launch_external_uri(uri_str, parent);
                            webkit_policy_decision_ignore(decision_guard.get());
                            break;

                        case EAS::deny:
                            orion::log_debug("external_apps: deny for %s", uri_str.c_str());
                            webkit_policy_decision_ignore(decision_guard.get());
                            break;

                        case EAS::ask:
                        default:
                            show_external_app_ask_dialog(website, parent,
                                [decision_guard, webview_weak, uri_str](bool allowed) {
                                    if (allowed) {
                                        GtkWidget* win = nullptr;
                                        if (auto wv = webview_weak.lock())
                                            win = gtk_widget_get_ancestor(GTK_WIDGET(wv.get()), GTK_TYPE_WINDOW);
                                        launch_external_uri(uri_str, win);
                                    }
                                    webkit_policy_decision_ignore(decision_guard.get());
                                });
                            break;
                    }
                }
            );
            return true;
        }

        // Non-HTTP URL (e.g. about:blank) — skip UA lookup.
        if (ua_nav_needs_password_scrape(nav_type)) {
            g_object_ref(decision);
            webkit_web_view_evaluate_javascript(self,
                orion::gtk::get_scrape_username_password_script().c_str(),
                -1, NULL, NULL, NULL,
                on_scrape_username_password_finish, decision);
        } else {
            use_policy_decision_with_autoplay(orion_webview, decision);
        }
        return TRUE;
    }

    // Build the per-site UA setting query fields directly from the domain,
    // equivalent to what WEBSITE_SETTING(user_agent, domain) would produce.
    // key_name = "user_agent", type = "integer" (enum stored as integer).
    std::string site_realm = std::format("settings::website::specific::{}", domain);
    std::string site_key   = site_realm + "::user_agent";
    std::string site_autoplay_key = site_realm + "::autoplay_default_setting";

    // Hold one extra ref via shared_ptr; any VERIFY_LOG early return will
    // automatically trigger g_object_unref via the deleter.
    g_object_ref(decision);
    auto decision_guard = std::shared_ptr<WebKitPolicyDecision>(
        decision, [](WebKitPolicyDecision* d) { g_object_unref(d); });

    orion::gtk::WeakRefWrapper webview_weak(self);
    std::string uri_str(new_uri);

    // Step 1: read global default autoplay policy (fallback for per-site setting).
    global_core_api->get_setting(profile_id,
        orion::settings::websites::autoplay_default_setting_for_all_websites,
        [webview_weak, decision_guard, profile_id,
         site_key, site_realm, site_autoplay_key, uri_str, nav_type]
    (orion::GetSettingReply<orion::settings::AutoplayWebsiteSetting> global_autoplay_reply)
    {
        VERIFY_LOG(global_autoplay_reply.err);

        orion::GetSettingQuery autoplay_query{
            .profile_id    = profile_id,
            .key           = site_autoplay_key,
            .realm         = site_realm,
            .key_name      = "autoplay_default_setting",
            .type          = "integer",
            .default_value = orion::settings::marshal_value(global_autoplay_reply.value),
        };

        // Step 2: read per-site autoplay, falling back to global default.
        global_core_api->get_setting_raw(autoplay_query,
            [webview_weak, decision_guard, profile_id,
             site_key, site_realm, uri_str, nav_type]
            (orion::GetSettingReply<std::string> autoplay_reply)
        {
            VERIFY_LOG(autoplay_reply.err);
            orion::settings::AutoplayWebsiteSetting resolved_autoplay{};
            VERIFY_LOG(orion::settings::unmarshal_value(autoplay_reply.value, &resolved_autoplay));

            auto webview_strong = webview_weak.lock();
            if (!webview_strong) return;
            WebKitWebView* webview = WEBKIT_WEB_VIEW(webview_strong.get());

            attach_autoplay_policy_to_decision(decision_guard.get(), resolved_autoplay);

            // Step 3: read global default UA (fallback for per-site UA setting).
            global_core_api->get_setting(profile_id,
                orion::settings::websites::user_agent_default_setting_for_all_websites,
                [webview_weak, decision_guard, profile_id,
                 site_key, site_realm, uri_str, nav_type]
                (orion::GetSettingReply<orion::settings::UserAgentWebsiteSetting> global_reply)
            {
                VERIFY_LOG(global_reply.err);
                orion::GetSettingQuery query{
                    .profile_id    = profile_id,
                    .key           = site_key,
                    .realm         = site_realm,
                    .key_name      = "user_agent",
                    .type          = "integer",
                    .default_value = orion::settings::marshal_value(global_reply.value),
                };

                // Step 4: read per-site UA, falling back to the global default.
                global_core_api->get_setting_raw(query,
                    [webview_weak, decision_guard, profile_id, uri_str, nav_type]
                    (orion::GetSettingReply<std::string> per_site_reply)
                {
                    VERIFY_LOG(per_site_reply.err);
                    orion::settings::UserAgentWebsiteSetting resolved{};
                    VERIFY_LOG(orion::settings::unmarshal_value(per_site_reply.value, &resolved));

                    if (resolved == orion::settings::UserAgentWebsiteSetting::custom) {
                        // Step 5 (custom only): fetch the user-defined UA string.
                        global_core_api->get_setting(profile_id,
                            orion::settings::privacy::custom_user_agent,
                            [webview_weak, decision_guard, resolved, uri_str, nav_type]
                            (orion::GetSettingReply<std::string> custom_reply)
                        {
                            VERIFY_LOG(custom_reply.err);
                            apply_user_agent_and_continue(
                                webview_weak, decision_guard, uri_str,
                                resolve_ua_string(resolved, custom_reply.value), nav_type);
                        });
                    } else {
                        apply_user_agent_and_continue(
                            webview_weak, decision_guard, uri_str,
                            resolve_ua_string(resolved, ""), nav_type);
                    }
                });
            });
        });
    });
    return TRUE;
}

//region Download intercept


gboolean
orion_webkit_webview_decide_policy_response(WebKitWebView* self, WebKitPolicyDecision* decision)
{
    WebKitResponsePolicyDecision* response_policy_decision = WEBKIT_RESPONSE_POLICY_DECISION(decision);
    WebKitURIResponse* response = webkit_response_policy_decision_get_response(response_policy_decision);
    orion::log_verbose("%s", webkit_uri_response_get_mime_type(response));
    std::string mime_type = webkit_uri_response_get_mime_type(response);

    //This is the policy decision that allows us to intercept what kind of mime type we got, meaning we can decide to offload the download off to our backend
    //We need to do this because many downloads are actually handled as "fake navigations", I believe other browsers go through this very same mechanism

    auto it = std::find_if(webview_supported_mime_types.cbegin(), webview_supported_mime_types.cend(),
        [mime_type](std::string_view supported_type){ return mime_type.starts_with(supported_type); });
    if(it == webview_supported_mime_types.cend() && webkit_response_policy_decision_is_main_frame_main_resource(response_policy_decision))
    {
        //TODO: FIXME:
        //WebKitURIRequest* request = webkit_response_policy_decision_get_request(response_policy_decision);
        // if(ORION_WEBKIT_WEBVIEW(self)->cpp_data.ignore_next_download && !GPOINTER_TO_INT(g_object_get_data(G_OBJECT(request), "is-user-triggered")))
        // {
        //     orion::log_verbose("webview ignoring download");
        //     webkit_policy_decision_ignore(decision);
        //     return true;
        // }

        OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
        if(!win)
        {
            webkit_policy_decision_download(decision);
            return true;
        }

        orion::ProfileId profile_id = orion_window_get_profile_id(win);
        std::string website = orion::Uri::parse_strict(webkit_uri_response_get_uri(response)).authority();

        g_object_ref(decision);
        auto decision_guard = std::shared_ptr<WebKitPolicyDecision>(
            decision, [](WebKitPolicyDecision* d){ g_object_unref(d); });

        orion::gtk::WeakRefWrapper webview_weak(self);

        global_core_api->get_setting_or_default(
            profile_id,
            WEBSITE_SETTING(downloads_default_setting, website),
            orion::settings::websites::downloads_default_setting_for_all_websites,
            [decision_guard, webview_weak, website]
            (orion::GetSettingReply<orion::settings::DownloadsWebsiteSetting> reply) mutable
            {
                VERIFY_LOG(reply.err);

                switch(reply.value)
                {
                    case orion::settings::DownloadsWebsiteSetting::allow:
                        webkit_policy_decision_download(decision_guard.get());
                        break;

                    case orion::settings::DownloadsWebsiteSetting::deny:
                        webkit_policy_decision_ignore(decision_guard.get());
                        break;

                    case orion::settings::DownloadsWebsiteSetting::ask:
                    {
                        GtkWidget* parent = nullptr;
                        if(auto wv = webview_weak.lock())
                            parent = gtk_widget_get_ancestor(GTK_WIDGET(wv.get()), GTK_TYPE_WINDOW);
                        if(!parent)
                        {
                            OrionApp* app = ORION_APP(g_application_get_default());
                            if(app) parent = GTK_WIDGET(gtk_application_get_active_window(GTK_APPLICATION(app)));
                        }

                        AdwDialog* alert = adw_alert_dialog_new(nullptr, nullptr);
                        if (website.empty()) {
                            adw_alert_dialog_set_heading(ADW_ALERT_DIALOG(alert),
                                _("Do you want to allow downloads on this page?"));
                        } else {
                            gchar* heading = g_strdup_printf(_("Do you want to allow downloads on %s?"), website.c_str());
                            adw_alert_dialog_set_heading(ADW_ALERT_DIALOG(alert), heading);
                            g_free(heading);
                        }
                        adw_alert_dialog_set_body(ADW_ALERT_DIALOG(alert),
                            _("You can change which websites can download files in Orion Websites preferences."));
                        adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(alert),
                            "deny", _("Don't Allow"),
                            "allow", _("Allow"),
                            nullptr);
                        adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(alert), "allow");

                        auto* cb = new std::function<void(bool)>([decision_guard](bool allowed) {
                            if (allowed) webkit_policy_decision_download(decision_guard.get());
                            else webkit_policy_decision_ignore(decision_guard.get());
                        });
                        g_signal_connect_data(alert, "response",
                            G_CALLBACK(+[](AdwAlertDialog*, const char* resp, std::function<void(bool)>* data) {
                                (*data)(std::string_view(resp) == "allow");
                                delete data;
                            }),
                            cb, [](gpointer, GClosure*){}, G_CONNECT_DEFAULT);

                        adw_dialog_present(alert, parent);
                        break;
                    }

                    default:
                        webkit_policy_decision_download(decision_guard.get());
                        break;
                }
            }
        );

        return true;
    }

    return false;
}

//endregion Download intercept

static gboolean
orion_webkit_webview_decide_policy_new_window(WebKitWebView* self, WebKitPolicyDecision* decision)
{
    auto* navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);

    auto* nav_action = webkit_navigation_policy_decision_get_navigation_action(navigation_decision);

    WebKitURIRequest* request = webkit_navigation_action_get_request(nav_action);
    const char* new_uri = webkit_uri_request_get_uri(request);

    unsigned mouse_button = webkit_navigation_action_get_mouse_button(nav_action);
    guint modifiers = webkit_navigation_action_get_modifiers(nav_action);

    bool is_shift_left_click = mouse_button == GDK_BUTTON_PRIMARY && (modifiers & GDK_SHIFT_MASK);

    if (is_shift_left_click) {
        // Shift+click: open the link in new Preview Window and ignore the decision here
        if (!open_link_in_preview_window(self, new_uri)) {
            return FALSE;
        }
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }

    // Captures the target of for example <a target="_blank" href="..."> link that was clicked by the user
    const char* target = webkit_navigation_action_get_frame_name(nav_action);

    bool should_clicked_link_open_new_tab =
           target
        && (g_ascii_strcasecmp(target, "_blank") == 0 || g_ascii_strcasecmp(target, "_parent") == 0)
        && webkit_navigation_action_get_navigation_type(nav_action) == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED
        && webkit_navigation_action_is_user_gesture(nav_action);

    orion::log_verbose(
        "orion_webkit_webview_decide_policy_new_window: target=%s should_clicked_link_open_new_tab=%s self=%p",
        target,
        should_clicked_link_open_new_tab ? "true" : "false",
        self);

    g_object_set_data(
        G_OBJECT(self),
        ORION_SHOULD_CLICKED_LINK_OPEN_NEW_TAB_KEY,
        GINT_TO_POINTER(should_clicked_link_open_new_tab));

    return FALSE;
}

static gboolean
orion_webkit_webview_decide_policy(WebKitWebView* self, WebKitPolicyDecision* decision, WebKitPolicyDecisionType type)
{
    switch(type)
    {
        case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
            return orion_webkit_webview_decide_policy_navigation(self, decision);
        
        // Triggered when for example <a target="_blank" href="..."> link is clicked
        case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
            return orion_webkit_webview_decide_policy_new_window(self, decision);

        case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
            return orion_webkit_webview_decide_policy_response(self, decision);
    }
    orion::log_warning("orion_webkit_webview_decide_policy: unknown WebKitPolicyDecisionType: %d", type);
    return FALSE;
}

// Called from WebKit
//TODO: move fullscreen logic to OrionWindow interface
static gboolean
orion_webkit_webview_enter_fullscreen(WebKitWebView *web_view)
{
    orion::log_debug("orion_webkit_webview_enter_fullscreen");
    GtkWidget* toplevel = gtk_widget_get_ancestor(GTK_WIDGET(web_view), GTK_TYPE_WINDOW);

    if (G_TYPE_CHECK_INSTANCE_TYPE(toplevel, MAIN_WINDOW_TYPE)) 
    {
        MainWindow* main_win = MAIN_WINDOW(toplevel);
        main_window_set_is_webkit_fullscreen(main_win, TRUE);
        
        return TRUE;
    }

    return FALSE;
}

// Called from WebKit
static gboolean
orion_webkit_webview_leave_fullscreen(WebKitWebView *web_view)
{
    orion::log_debug("orion_webkit_webview_leave_fullscreen");
    GtkWidget* toplevel = gtk_widget_get_ancestor(GTK_WIDGET(web_view), GTK_TYPE_WINDOW);

    if (G_TYPE_CHECK_INSTANCE_TYPE(toplevel, MAIN_WINDOW_TYPE)) 
    {
        MainWindow* main_win = MAIN_WINDOW(toplevel);
        main_window_set_is_webkit_fullscreen(main_win, FALSE);
        return TRUE;
    }

    return FALSE;
}

static void
set_tls_status(OrionWebkitWebview* self, orion::TlsStatus status)
{
    if(self->tls_status != status)
    {
        self->tls_status = status;
        g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_TLS_STATUS]);
    }
}

static bool
is_local_uri(const std::string& uri_str)
{
    if(uri_str.empty())
    {
        return false;
    }

    const char* scheme = g_uri_peek_scheme(uri_str.data());
    if(!scheme)
    {
        return false;
    }

    //sadly hardcoded but here we go
    if(0 == g_strcmp0(scheme, "about"))
    {
        return true;
    }

    if(0 == g_strcmp0(scheme, "file"))
    {
        GFile* file = g_file_new_for_uri(uri_str.data());
        defer([file]{ g_object_unref(file); });
        return file && g_file_is_native(file);
    }

    if (0 != g_strcmp0(scheme, "http") &&
        0 != g_strcmp0(scheme, "https") &&
        0 != g_strcmp0(scheme, "ws") &&
        0 != g_strcmp0(scheme, "wss"))
    {
        return false;
    }

    GUri* parsed_uri = g_uri_parse(uri_str.c_str(), G_URI_FLAGS_NONE, nullptr);
    if(!parsed_uri)
    {
        return false;
    }
    defer([parsed_uri]{ g_uri_unref(parsed_uri); });

    const gchar* host = g_uri_get_host(parsed_uri);
    if(!host || *host == '\0')
    {
        return false;
    }

    gchar* host_lower = g_ascii_strdown(host, -1);
    defer([host_lower]{ g_free(host_lower); });

    if (0 == g_strcmp0(host_lower, "localhost") ||
        g_str_has_suffix(host, ".localhost")) 
    {
        return true;
    }


    //loopback ip address check
    {
        GInetAddress* addr = g_inet_address_new_from_string(host);
        if(addr)
        {
            defer([addr]{ g_object_unref(addr); });

            if(g_inet_address_get_is_loopback(addr))
            {
                return true;
            }
        }
    }
    
    return false;
}

static void
evaluate_tls_status(OrionWebkitWebview* self)
{
    WebKitWebView* webview = WEBKIT_WEB_VIEW(self);
    const gchar* uri_c_str = webkit_web_view_get_uri(webview);

    if(!uri_c_str)
    {
        set_tls_status(self, orion::TlsStatus::none);
        return;
    }
    std::string uri = uri_c_str;

    if (is_local_uri(uri))
    {
        set_tls_status(self, orion::TlsStatus::local);
        return;
    }

    GTlsCertificate* cert = nullptr;
    GTlsCertificateFlags errors = static_cast<GTlsCertificateFlags>(0);
    gboolean is_tls = webkit_web_view_get_tls_info(webview, &cert, &errors);

    if(!is_tls)
    {
        set_tls_status(self, orion::TlsStatus::insecure);
        return;
    }
    
    if (errors != 0) {
        set_tls_status(self, orion::TlsStatus::failure);
        return;
    }

    set_tls_status(self, orion::TlsStatus::secure);
}

static void
orion_webkit_webview_load_changed(WebKitWebView* web_view, WebKitLoadEvent load_event)
{
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(web_view);
    // std::string string{};
    switch(load_event)
    {
        case WEBKIT_LOAD_COMMITTED:
            //ORION_WEBKIT_WEBVIEW(web_view)->is_probably_readerable = false;
            //g_object_notify_by_pspec(G_OBJECT(web_view), obj_properties[PROP_IS_PROBABLY_READERABLE]);
            //string = "commited";
            evaluate_tls_status(self);
            break;
        case WEBKIT_LOAD_FINISHED:
            evaluate_tls_status(self);
            //string = "finished";
            break;
        case WEBKIT_LOAD_REDIRECTED:
            //string = "redirected";
            break;
        case WEBKIT_LOAD_STARTED:
            //string = "started";
            break;
    }
    // orion::log_verbose("load changed: %s", string.c_str());
}

/*
Error codes table
0 - Error resolving “www.casfdaf.com”: Name or service not known (Valid URL format but cant find)
1 - Error performing TLS handshake: The Diffie-Hellman prime sent by the server is not acceptable (not long enough).
3 - Peer failed to perform TLS handshake: A packet with illegal or unsupported version was received. - Here we throw a Certificate error screen
101 - The URL can’t be shown (Totaly invalid format of URL)
*/

static gboolean 
orion_webkit_webview_load_failed(WebKitWebView* web_view, WebKitLoadEvent load_event, const gchar* failing_uri,
    GError* error)
{
    std::string url = failing_uri;
    int error_code = error->code;

    if (g_error_matches(error, WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE)) {
        //this would mean that we have decided to not load this uri (e.g. download intercepted)
        return FALSE;
    }
    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED)) {
        //usually emitted when overriden through a different load request, safe to ignore 
        return FALSE;
    }

    orion::log_error(
        "while loading website \"%s\" - load_event: %d, Error message: \"%s\", code: %d",
        failing_uri, load_event, error->message, error_code
    );

    if (error_code == 1) {
        ErrorPageBuilder(web_view)
            .set_title(_("Orion can't open this page"))
            .set_description(_("An SSL error has occurred and a secure connection to the server cannot be made."))
            .set_url(url)
            .hide_adblocker_button(true)
            .load();

    } else if (error_code == 3) {
        std::string warning = orion::text::translatef(
            "This website has an outdated security configuration, which may allow an attacker to steal personal or financial information entered into “%1$s”.",
            failing_uri
        );

        CertificateErrorPageBuilder(web_view)
            .set_url(url)
            .set_warning(warning)
            .allow_bypass(true)
            .load();
    } else {
        //Generic network/load error
        ErrorPageBuilder(web_view)
            .set_title(_("Orion can't open this page"))
            .set_description(_("Could not connect to the server."))
            .set_url(url)
            .hide_adblocker_button(true)
            .load();
    }

    return TRUE;
}

/*TODO current version of webkit doesnt contain webkit_web_context_allow_tls_certificate_for_host () - this is since 2.6
Shell implement when the time arises.
*/
static gboolean
orion_webkit_webview_load_failed_with_tls_errors(
    WebKitWebView* web_view, const gchar* failing_uri, GTlsCertificate* certificate, GTlsCertificateFlags errors)
{
    std::string url = failing_uri;
    orion::log_warning("%s: failing_url=%s", __func__, url.c_str());

    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(web_view);
    set_tls_status(self, orion::TlsStatus::failure);
    self->cpp_data.last_tls_certificate.reset(certificate ? G_OBJECT(g_object_ref(certificate)) : nullptr);
    self->cpp_data.last_tls_errors = errors;
    self->cpp_data.last_tls_uri = url;

    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(web_view)));
    std::string domain = orion::Uri::parse_strict(url.c_str()).authority();

    if (win && !domain.empty()) {
        orion::ProfileId profile_id = orion_window_get_profile_id(win);
        std::string site_realm = std::format("settings::website::specific::{}", domain);
        std::string site_ssl_key = site_realm + "::ssl_certificate_validation_setting";

        orion::gtk::WeakRefWrapper webview_weak(web_view);

        global_core_api->get_setting(profile_id,
            orion::settings::websites::ssl_certificate_validation_setting_for_all_websites,
            [webview_weak, profile_id, site_realm, site_ssl_key, domain, url]
            (orion::GetSettingReply<orion::settings::SslCertificateValidationWebsiteSetting> global_reply)
        {
            VERIFY_LOG(global_reply.err);

            orion::GetSettingQuery query{
                .profile_id    = profile_id,
                .key           = site_ssl_key,
                .realm         = site_realm,
                .key_name      = "ssl_certificate_validation_setting",
                .type          = "integer",
                .default_value = orion::settings::marshal_value(global_reply.value),
            };

            global_core_api->get_setting_raw(query,
                [webview_weak, domain, url]
                (orion::GetSettingReply<std::string> per_site_reply)
            {
                VERIFY_LOG(per_site_reply.err);

                orion::settings::SslCertificateValidationWebsiteSetting resolved{};
                VERIFY_LOG(orion::settings::unmarshal_value(per_site_reply.value, &resolved));

                auto webview_strong = webview_weak.lock();
                if (!webview_strong) {
                    return;
                }

                WebKitWebView* current_web_view = WEBKIT_WEB_VIEW(webview_strong.get());

                if (resolved == orion::settings::SslCertificateValidationWebsiteSetting::ignore) {
                    OrionWebkitWebview* current_orion_webview = ORION_WEBKIT_WEBVIEW(current_web_view);
                    GTlsCertificate* current_certificate =
                        G_TLS_CERTIFICATE(current_orion_webview->cpp_data.last_tls_certificate.get());

                    if (current_certificate) {
                        orion::log_debug("TLS setting ignore for host '%s', allowing cert and reloading", domain.c_str());
                        webkit_network_session_allow_tls_certificate_for_host(
                            webkit_web_view_get_network_session(current_web_view),
                            current_certificate,
                            domain.c_str());
                        webkit_web_view_load_uri(current_web_view, url.c_str());
                        return;
                    }
                }

                std::string warning_text = orion::text::translatef(
                    "This website may be impersonating “%1$s” to steal your personal or financial information.%2$s To learn more, you can %3$s view the certificate %4$s",
                    url.c_str(),
                    "<br /><br />",
                    "<a href='javascript:CertificateWarningHandler.showCertificate()' role='button'>",
                    "</a>."
                );

                CertificateErrorPageBuilder(current_web_view)
                    .set_url(url)
                    .set_warning(warning_text)
                    .allow_bypass(true)
                    .load();
            });
        });

        handle_tls_certificate_flags(errors);
        return TRUE;
    }

    std::string warning_text = orion::text::translatef(
        "This website may be impersonating “%1$s” to steal your personal or financial information.%2$s To learn more, you can %3$s view the certificate %4$s",
        url.c_str(),
        "<br /><br />",
        "<a href='javascript:CertificateWarningHandler.showCertificate()' role='button'>",
        "</a>."
    );

    CertificateErrorPageBuilder(web_view)
        .set_url(url)
        .set_warning(warning_text)
        .allow_bypass(true)
        .load();

    handle_tls_certificate_flags(errors);

    return TRUE;
}

static void
orion_webkit_webview_mouse_target_changed(WebKitWebView* web_view, WebKitHitTestResult* hit_test_result,
    guint modifiers)
{
    orion::log_verbose("orion_webkit_webview_mouse_target_changed");

    OrionWindow *toplevel = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(web_view)));

    if(!MAIN_IS_WINDOW(toplevel)) return;
    
    std::vector<HitTestUtil::Element> elements = HitTestUtil::get_elements_from_hit_test(hit_test_result);

    if (std::find(elements.begin(), elements.end(), HitTestUtil::Element::link) != elements.end()) {        
        main_window_show_link_overlay(MAIN_WINDOW(toplevel), webkit_hit_test_result_get_link_uri(hit_test_result));
    } else {
        main_window_hide_link_overlay(MAIN_WINDOW(toplevel));
    }

    OrionWebkitWebview* orion_webview = ORION_WEBKIT_WEBVIEW(web_view);

    // free previous hit test result (if any)
    if (orion_webview->last_hit_test_result) {
        g_clear_object(&orion_webview->last_hit_test_result);
    }
    // store new hit test result
    orion_webview->last_hit_test_result = g_object_ref(hit_test_result);
}

static gboolean
orion_webkit_webview_permission_request(WebKitWebView* web_view, WebKitPermissionRequest* request)
{
    std::shared_ptr<orion::gtk::GObjectPtr> request_shared_unique_ptr =
        std::make_shared<orion::gtk::GObjectPtr>((GObject*)g_object_ref(request));

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(web_view));
    if (!root || !MAIN_IS_WINDOW(root)) {
        orion::log_error("Permission request received but no main window found");
        webkit_permission_request_deny(request);
        return TRUE;
    }

    OrionWindow* win = ORION_WINDOW(root);
    orion::ProfileId profile_id = orion_window_get_profile_id(win);

    PermissionHandler* handler = PermissionHandler::get_static_permission_handler_for(request, profile_id);
    if (!handler) {
        orion::log_error("No permission handler found for request type: %s", G_OBJECT_TYPE_NAME(request));
        webkit_permission_request_deny(request);
        return TRUE;
    }
    
    gboolean is_secure = orion_webkit_webview_is_connection_secure(web_view);
    if (handler->is_auto_deny_on_http() && !is_secure)
    {
        //FREE all the things
        orion::log_debug("Auto denying permission request on unsecure connection");
        webkit_permission_request_deny(request);
        return TRUE;
    }

    const gchar* uri = webkit_web_view_get_uri(web_view);
    WebKitSecurityOrigin* origin = webkit_security_origin_new_for_uri(uri);

    const char* host = webkit_security_origin_get_host(origin);
    std::string website = host ? host : "";

    orion::log_debug("Original website: %s", website.c_str());
    if (website.empty()) { //Typicaly happens when uri is local file not URL.
        orion::log_error("Unable to extract host from URI! Recovery: Replacing with placeholder.");
        website = "website";
        webkit_permission_request_deny(request);
        return TRUE;
    }


    //orion::gtk::WeakRefWrapper weak_request(request);
    orion::gtk::WeakRefWrapper weak_web_view(web_view);

    handler->start_request(website, request_shared_unique_ptr,
        [&handler, website, request_shared_unique_ptr, weak_web_view](bool handled){
            if (handled) {
                orion::log_debug("Permission request was autoresolved");
            } else {
                orion::log_debug("Permission request could not be autoresolved - prompting user");

                AdwDialog* dialog = nullptr;
                switch (handler->get_type_of_dialog())
                {
                case PermissionHandler::TypeOfDialog::binary:
                    orion::log_debug("Displaying binary dialog");

                    dialog = permissions_request_binary_dialog_new(handler, website, request_shared_unique_ptr);
                    break;
                case PermissionHandler::TypeOfDialog::quadruplet:
                    orion::log_debug("Displaying quadruple dialog");
                    dialog = permissions_request_dialog_new(handler, website, request_shared_unique_ptr);
                    break;
                }

                auto ptr = weak_web_view.lock();
                if(!ptr) return;

                WebKitWebView* web_view = WEBKIT_WEB_VIEW(ptr.get());

                if (dialog && web_view) { //Should be OK to prompt user
                    GtkWidget* parent = gtk_widget_get_ancestor(GTK_WIDGET(web_view), GTK_TYPE_WINDOW);
                    if (parent) {
                        adw_dialog_present(dialog, parent);
                    }
                } else { //Failed to display dialog - deny request and clean up
                    webkit_permission_request_deny(WEBKIT_PERMISSION_REQUEST(request_shared_unique_ptr.get()->get()));
                }
            }
        }
    );

    webkit_security_origin_unref(origin);
    return TRUE;
}

static gboolean 
orion_webkit_webview_print(WebKitWebView* web_view, WebKitPrintOperation* print_operation)
{
    orion::log_debug("Print requested");
    GtkWindow* parent = GTK_WINDOW(gtk_widget_get_ancestor(GTK_WIDGET(web_view), GTK_TYPE_WINDOW));
    
    gtk_widget_set_sensitive(GTK_WIDGET(parent),FALSE);
    webkit_print_operation_run_dialog(print_operation, parent);
    gtk_widget_set_sensitive(GTK_WIDGET(parent),TRUE);

    g_signal_connect(print_operation, "finished", G_CALLBACK(orion_webkit_webview_on_print_finished), parent);
    g_signal_connect(print_operation, "failed", G_CALLBACK(orion_webkit_webview_on_print_failed), parent);

    return TRUE;
}

static gboolean
orion_webkit_webview_query_permission_state(WebKitWebView* web_view, WebKitPermissionStateQuery* query)
{
    orion::log_verbose("Handling Permission query signal - name: %s", webkit_permission_state_query_get_name(query));
    //TODO wait till some backend preferences with permission are implemented
    webkit_permission_state_query_finish(query, WebKitPermissionState::WEBKIT_PERMISSION_STATE_PROMPT); // Orion protects!
    return TRUE;
}

static GtkWidget*
orion_webkit_webview_create(WebKitWebView* self, WebKitNavigationAction* action)
{
    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return nullptr;

    auto& factory = orion_window_get_webview_factory(win);
    auto new_view_strong = factory.web_view_for_content_synchronous(WebViewRef((OpaqueWebView*)g_object_ref(self)));
    if(!new_view_strong) return NULL;

    OrionWebkitWebview* new_view = ORION_WEBKIT_WEBVIEW(new_view_strong.get());

    g_object_set_data_full(G_OBJECT(new_view), "orion-popup-opener",
        g_object_ref(self), g_object_unref);

    const char* request_uri = webkit_uri_request_get_uri(
        webkit_navigation_action_get_request(action));
    if (request_uri) {
        g_object_set_data_full(G_OBJECT(new_view), "orion-popup-request-uri",
            g_strdup(request_uri), g_free);
    }

    // Calling webkit_navigation_action_get_frame_name(action) here to determine target of for example
    // <a target="_blank" href="..."> links does not work as expected, it is already consumed
    // and returns NULL. Capture the frame name in orion_webkit_webview_decide_policy instead.

    // Forward the state of whether the link should be opened in a new tab
    g_object_set_data(G_OBJECT(new_view), ORION_SHOULD_CLICKED_LINK_OPEN_NEW_TAB_KEY,
        g_object_get_data(G_OBJECT(self), ORION_SHOULD_CLICKED_LINK_OPEN_NEW_TAB_KEY));

    // Consume and reset the key in the original web view
    g_object_set_data(G_OBJECT(self), ORION_SHOULD_CLICKED_LINK_OPEN_NEW_TAB_KEY, NULL);

    g_object_weak_ref(G_OBJECT(new_view), [](gpointer data, GObject* deleted){
        orion::log_verbose("orion_webkit_webview_create: new webview destroyed ok");
    }, NULL);
    
    orion::log_verbose("orion_webkit_webview_create: created new webview: ref_count=%d", G_OBJECT(new_view)->ref_count);
    return GTK_WIDGET(new_view_strong.release());
}

static void
block_popup(orion::gtk::WeakRefWrapper opener_weak,
            const std::string& request_uri,
            orion::settings::PopupWindowsWebsiteSetting setting)
{
    orion::log_info("popup blocked: %s", request_uri.c_str());

    // Always notify the user about popups being blocked (similar to Mac Orion)
    //using PWS = orion::settings::PopupWindowsWebsiteSetting;
    //if (setting != PWS::block_and_notify) return;

    auto opener_strong = opener_weak.lock();
    if (!opener_strong) return;
    WebKitWebView* opener = WEBKIT_WEB_VIEW(opener_strong.get());

    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(
        gtk_widget_get_ancestor(GTK_WIDGET(opener), ORION_WEBVIEW_TAB_CONTENT_TYPE));
    if (!content) return;

    orion_webview_tab_content_add_blocked_popup(content, request_uri);

    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(content));
    if (win) {
        main_window_show_popup_blocked_message(win);
    }
}

// Testing: https://webbrowsertools.com/popup-blocker/
// TODO: Popup windows open as: New window / New tab / Nested tab
static void
open_popup_window(orion::gtk::WeakRefWrapper popup_weak,
                  orion::ProfileId profile_id,
                  orion::PrivateMode private_mode)
{
    orion::CreateWindowQuery win_query{};
    win_query.profile_id = profile_id;
    win_query.icon_name = "window-flat-symbolic";
    win_query.is_tab_group = true;

    global_core_api->create_window(win_query,
        [popup_weak, profile_id, private_mode](orion::CreateWindowReply win_reply)
    {
        VERIFY_LOG(win_reply.err);
        orion::WindowId window_id = win_reply.id;

        orion::CreateTabQuery tab_query{};
        tab_query.window_id = window_id;

        global_core_api->create_tab(tab_query,
            [popup_weak, profile_id, private_mode, window_id](orion::CreateTabReply tab_reply)
        {
            VERIFY_LOG(tab_reply.err);

            auto popup_strong = popup_weak.lock();
            if (!popup_strong) return;
            OrionWebkitWebview* popup = ORION_WEBKIT_WEBVIEW(popup_strong.get());

            // Build a tab content backed by the already-loading popup webview.
            OrionWebviewTabContent* content =
                orion_webview_tab_content_new_from_webview(tab_reply.id, popup);
            g_object_ref_sink(content);
            defer([content]{ g_object_unref(content); });

            // Register it so insert_page picks it up instead of creating a fresh webview.
            OrionApp* app = ORION_APP(g_application_get_default());
            orion_app_add_shared_pinned_tab(app, content);

            MainWindow* new_win = main_window_new(
                g_application_get_default(), profile_id, window_id, private_mode);
            gtk_window_present(GTK_WINDOW(new_win));
        });
    });
}

static void
orion_webkit_webview_ready_to_show(WebKitWebView* web_view)
{
    // This ensures that the web_view that was created in create() virtual method is properly unreferenced
    // (and finalized). Hopefully, this does not break WebKit internals by unreferencing too early.
    defer([web_view]{ g_object_unref(web_view); });

    WebKitWebView* opener_raw = WEBKIT_WEB_VIEW(
        g_object_get_data(G_OBJECT(web_view), "orion-popup-opener"));
    if (!opener_raw) return;

    OrionWindow* opener_win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(opener_raw)));
    if (!opener_win) return;

    orion::ProfileId profile_id = orion_window_get_profile_id(opener_win);
    orion::PrivateMode private_mode = orion_window_get_private_mode(opener_win);

    const char* opener_uri_cstr = webkit_web_view_get_uri(opener_raw);
    std::string domain = orion::Uri::parse_strict(opener_uri_cstr ? opener_uri_cstr : "").authority();

    // Capture the request URI stored in create() for use when blocked.
    const char* request_uri_cstr = static_cast<const char*>(
        g_object_get_data(G_OBJECT(web_view), "orion-popup-request-uri"));
    std::string request_uri = request_uri_cstr ? request_uri_cstr : "";

    orion::gtk::WeakRefWrapper popup_weak(ORION_WEBKIT_WEBVIEW(web_view));
    orion::gtk::WeakRefWrapper opener_weak(ORION_WEBKIT_WEBVIEW(opener_raw));

    bool should_clicked_link_open_new_tab = GPOINTER_TO_INT(
        g_object_get_data(
            G_OBJECT(web_view),
            ORION_SHOULD_CLICKED_LINK_OPEN_NEW_TAB_KEY));

    // Consume and reset the key
    g_object_set_data(G_OBJECT(web_view), ORION_SHOULD_CLICKED_LINK_OPEN_NEW_TAB_KEY, NULL);

    orion::log_verbose(
        "orion_webkit_webview_ready_to_show: should_clicked_link_open_new_tab=%s request_uri=%s webview=%p",
        should_clicked_link_open_new_tab ? "true" : "false",
        request_uri.c_str(),
        web_view);

    // If the popup request URI is an external app scheme (e.g. mailto:), launch it
    // via XDG portal instead of opening a popup window.
    {
        std::string req_domain = orion::Uri::parse_strict(request_uri.c_str()).authority();
        auto req_scheme = orion::Uri::parse_strict(request_uri.c_str()).known_scheme();
        using S = orion::Uri::Scheme;
        if (req_domain.empty() && req_scheme != S::unknown && req_scheme != S::empty) {
            std::string website = domain;

            global_core_api->get_setting_or_default(
                profile_id,
                WEBSITE_SETTING(external_apps_default_setting, website),
                orion::settings::websites::external_apps_default_setting_for_all_websites,
                [opener_weak, request_uri, website]
                (orion::GetSettingReply<orion::settings::ExternalAppsWebsiteSetting> reply)
                {
                    VERIFY_LOG(reply.err);
                    using EAS = orion::settings::ExternalAppsWebsiteSetting;

                    GtkWidget* parent = nullptr;
                    if (auto wv = opener_weak.lock())
                        parent = gtk_widget_get_ancestor(GTK_WIDGET(wv.get()), GTK_TYPE_WINDOW);
                    if (!parent) {
                        OrionApp* app = ORION_APP(g_application_get_default());
                        if (app) parent = GTK_WIDGET(gtk_application_get_active_window(GTK_APPLICATION(app)));
                    }

                    switch (reply.value) {
                        case EAS::allow:
                            orion::log_debug("external_apps (popup): launching %s", request_uri.c_str());
                            launch_external_uri(request_uri, parent);
                            break;

                        case EAS::deny:
                            orion::log_debug("external_apps (popup): deny for %s", request_uri.c_str());
                            break;

                        case EAS::ask:
                        default:
                            show_external_app_ask_dialog(website, parent,
                                [opener_weak, request_uri](bool allowed) {
                                    if (allowed) {
                                        GtkWidget* win = nullptr;
                                        if (auto wv = opener_weak.lock())
                                            win = gtk_widget_get_ancestor(GTK_WIDGET(wv.get()), GTK_TYPE_WINDOW);
                                        launch_external_uri(request_uri, win);
                                    }
                                });
                            break;
                    }
                }
            );
            return;
        }
    }

    // Handle for example <a target="_blank" href="..."> links
    if (should_clicked_link_open_new_tab) {
        // Attach/present as a normal tab; do not apply user-configurable popup blocking
        OrionWebviewTabView* tab_view = ORION_WEBVIEW_TAB_VIEW(
            gtk_widget_get_ancestor(GTK_WIDGET(opener_raw), ORION_WEBVIEW_TAB_VIEW_TYPE));
        if (!tab_view) {
            orion::log_warning("orion_webkit_webview_ready_to_show: failed to get OrionWebviewTabView");
            return;
        }
        orion_webview_tab_view_new_tab_with_uri(tab_view, request_uri.c_str(), {
            .autoselect = true,
            .ignore_when_opening_setting = true,
            .force_open_next_to_current = true,
        });
        return;
    }
    
    // Step 1: read global default popup windows setting.
    global_core_api->get_setting(profile_id,
        orion::settings::websites::popup_windows_default_setting_for_all_websites,
        [popup_weak, opener_weak, profile_id, private_mode, domain, request_uri]
        (orion::GetSettingReply<orion::settings::PopupWindowsWebsiteSetting> global_reply)
    {
        VERIFY_LOG(global_reply.err);
        using PWS = orion::settings::PopupWindowsWebsiteSetting;

        if (domain.empty()) {
            if (global_reply.value == PWS::allow)
                open_popup_window(popup_weak, profile_id, private_mode);
            else
                block_popup(opener_weak, request_uri, global_reply.value);
            return;
        }

        std::string site_realm = std::format("settings::website::specific::{}", domain);
        orion::GetSettingQuery popup_query{
            .profile_id    = profile_id,
            .key           = site_realm + "::popup_windows_default_setting",
            .realm         = site_realm,
            .key_name      = "popup_windows_default_setting",
            .type          = "integer",
            .default_value = orion::settings::marshal_value(global_reply.value),
        };

        // Step 2: read per-site popup setting, falling back to global default.
        global_core_api->get_setting_raw(popup_query,
            [popup_weak, opener_weak, profile_id, private_mode, request_uri]
            (orion::GetSettingReply<std::string> per_site_reply)
        {
            VERIFY_LOG(per_site_reply.err);
            orion::settings::PopupWindowsWebsiteSetting resolved{};
            VERIFY_LOG(orion::settings::unmarshal_value(per_site_reply.value, &resolved));

            using PWS = orion::settings::PopupWindowsWebsiteSetting;
            if (resolved == PWS::allow)
                open_popup_window(popup_weak, profile_id, private_mode);
            else
                block_popup(opener_weak, request_uri, resolved);
        });
    });
}

static gboolean
orion_webkit_webview_run_color_chooser(WebKitWebView* web_view, WebKitColorChooserRequest* request)
{
    return FALSE;
}

namespace {
    struct FileChooserRequestData {
        explicit FileChooserRequestData(WebKitFileChooserRequest* request)
            : request(WEBKIT_FILE_CHOOSER_REQUEST(g_object_ref(request)))
        {
        }

        ~FileChooserRequestData()
        {
            g_object_unref(request);
        }

        WebKitFileChooserRequest* request{};
    };
}

static void
file_chooser_cancel_file_chooser_request(WebKitFileChooserRequest* request, GError* error)
{
    if (error && !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        orion::log_error("failed to open file chooser: %s", error->message);

    webkit_file_chooser_request_cancel(request);
    g_clear_error(&error);
}

static void
file_chooser_single_file_selected(GObject* source, GAsyncResult* result, gpointer user_data)
{
    std::unique_ptr<FileChooserRequestData> data(static_cast<FileChooserRequestData*>(user_data));

    GError* error = nullptr;
    GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
    if (!file) {
        file_chooser_cancel_file_chooser_request(data->request, error);
        return;
    }
    defer([file] { g_object_unref(file); });

    gchar* path = g_file_get_path(file);
    if (!path) {
        orion::log_error("file chooser returned non-local file");
        webkit_file_chooser_request_cancel(data->request);
        return;
    }
    defer([path] { g_free(path); });

    const gchar* selected_files[] = { path, nullptr };
    webkit_file_chooser_request_select_files(data->request, selected_files);
}

static void
file_chooser_multiple_files_selected(GObject* source, GAsyncResult* result, gpointer user_data)
{
    std::unique_ptr<FileChooserRequestData> data(static_cast<FileChooserRequestData*>(user_data));

    GError* error = nullptr;
    GListModel* files = gtk_file_dialog_open_multiple_finish(GTK_FILE_DIALOG(source), result, &error);
    if (!files) {
        file_chooser_cancel_file_chooser_request(data->request, error);
        return;
    }
    defer([files] { g_object_unref(files); });

    GPtrArray* paths = g_ptr_array_new_with_free_func(g_free);
    defer([paths] { g_ptr_array_unref(paths); });

    const guint file_count = g_list_model_get_n_items(files);
    for (guint i = 0; i < file_count; ++i) {
        GFile* file = G_FILE(g_list_model_get_item(files, i));
        if (!file)
            continue;

        if (gchar* path = g_file_get_path(file))
            g_ptr_array_add(paths, path);
        g_object_unref(file);
    }

    if (paths->len == 0) {
        orion::log_error("file chooser returned no local files");
        webkit_file_chooser_request_cancel(data->request);
        return;
    }

    g_ptr_array_add(paths, nullptr);
    webkit_file_chooser_request_select_files(
        data->request,
        reinterpret_cast<const gchar* const*>(paths->pdata)
    );
}

static gboolean
orion_webkit_webview_run_file_chooser(WebKitWebView* web_view, WebKitFileChooserRequest* request)
{
    GtkFileDialog* dialog = gtk_file_dialog_new();
    defer([dialog] { g_object_unref(dialog); });

    const gboolean select_multiple = webkit_file_chooser_request_get_select_multiple(request);
    gtk_file_dialog_set_title(dialog, select_multiple ? _("Select Files") : _("Select File"));

    if (GtkFileFilter* filter = webkit_file_chooser_request_get_mime_types_filter(request)) {
        GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
        g_list_store_append(filters, filter);
        gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
        gtk_file_dialog_set_default_filter(dialog, filter);
        g_object_unref(filters);
    }

    if (const gchar* const* selected_files = webkit_file_chooser_request_get_selected_files(request)) {
        if (selected_files[0]) {
            GFile* file = g_file_new_for_path(selected_files[0]);
            gtk_file_dialog_set_initial_file(dialog, file);
            g_object_unref(file);
        }
    }

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(web_view));
    GtkWindow* parent = GTK_IS_WINDOW(root) ? GTK_WINDOW(root) : nullptr;
    auto* data = new FileChooserRequestData(request);

    if (select_multiple) {
        gtk_file_dialog_open_multiple(
            dialog,
            parent,
            nullptr,
            file_chooser_multiple_files_selected,
            data
        );
    } else {
        gtk_file_dialog_open(
            dialog,
            parent,
            nullptr,
            file_chooser_single_file_selected,
            data
        );
    }

    return TRUE;
}

static gboolean
orion_webkit_webview_script_dialog(WebKitWebView* web_view, WebKitScriptDialog* dialog)
{
    const char* message = webkit_script_dialog_get_message(dialog);
    if(message) orion::log_verbose("script dialog message: %s", message);
    return WEBKIT_WEB_VIEW_CLASS(orion_webkit_webview_parent_class)->script_dialog(web_view, dialog);
}

static gboolean
orion_webkit_webview_show_notification(WebKitWebView* web_view, WebKitNotification* notification)
{
    const char* title = webkit_notification_get_title(notification);
    const char* body  = webkit_notification_get_body(notification);
    const char* tag   = webkit_notification_get_tag(notification);

    GNotification* gnotif = g_notification_new(title ? title : "");
    if (body && *body)
        g_notification_set_body(gnotif, body);

    char fallback_id[32];
    const char* notif_id;
    if (tag && *tag) {
        notif_id = tag;
    } else {
        g_snprintf(fallback_id, sizeof(fallback_id), "wn-%p", (void*)notification);
        notif_id = fallback_id;
    }

    g_application_send_notification(g_application_get_default(), notif_id, gnotif);
    g_object_unref(gnotif);

    g_signal_connect(notification, "closed", G_CALLBACK(+[](WebKitNotification*, gpointer id) {
        g_application_withdraw_notification(g_application_get_default(), (const char*)id);
        g_free(id);
    }), g_strdup(notif_id));

    return TRUE;
}

static gboolean
orion_webkit_webview_show_option_menu(WebKitWebView* web_view, WebKitOptionMenu* menu, GdkRectangle* rectangle)
{
    return FALSE;
}

static void
orion_webkit_webview_submit_form(WebKitWebView* web_view, WebKitFormSubmissionRequest* request)
{
    orion::log_verbose("Handling form submit");
    webkit_form_submission_request_submit(request);
}

static gboolean
orion_webkit_webview_user_message_received(WebKitWebView* web_view, WebKitUserMessage* message)
{
    return FALSE;
}

static gboolean
orion_webkit_webview_web_process_crashed(WebKitWebView* web_view)
{
    orion::log_error("orion_webkit_webview_web_process_crashed");
    return FALSE;
}

static void
orion_webkit_webview_insecure_content_detected(WebKitWebView* web_view, WebKitInsecureContentEvent event)
{
    OrionWebkitWebview*self = ORION_WEBKIT_WEBVIEW(web_view);
    if (self->tls_status == orion::TlsStatus::secure) {
        set_tls_status(self, orion::TlsStatus::mixed);
    }
}


//End of webviews virtual methods =====================================


//Start of webviews signal handlers

//TODO Implement some error handler

static void
orion_webkit_webview_web_process_terminated(WebKitWebView* self, WebKitWebProcessTerminationReason reason)
{
    if (reason == WEBKIT_WEB_PROCESS_CRASHED) {
        orion::log_error("orion_webkit_webview_web_process_terminated: web process crashed");
    } else if (reason == WEBKIT_WEB_PROCESS_TERMINATED_BY_API) {
        // This happens for example when returning to Start Page from another webview page
        orion::log_verbose("orion_webkit_webview_web_process_terminated: web process terminated gracefully by API call");
        return;
    } else {
        orion::log_debug("orion_webkit_webview_web_process_terminated: reason=%d", reason);
    }

    std::string title = _("Web process error occurred");
    std::string description = orion::text::translatef("Error code %1$d", reason);

    ErrorPageBuilder(self)
        .set_title(title)
        .set_description(description)
        .set_url("about:blank")
        .hide_adblocker_button(true)
        .load();
}

static void
orion_webkit_webview_on_back_forward_list_changed(WebKitBackForwardList* back_forward_list, WebKitBackForwardListItem* item_added, gpointer items_removed, OrionWebkitWebview* self)
{
    GList_autoptr back_list = webkit_back_forward_list_get_back_list(back_forward_list);
    GList_autoptr forward_list = webkit_back_forward_list_get_forward_list(back_forward_list);
    orion::log_verbose("back forward list changed: n forward: %i n back %i added %p n removed %i", g_list_length(forward_list), g_list_length(back_list), item_added, g_list_length((GList*)(items_removed)));

    if(ORION_WEBKIT_WEBVIEW(self)->cpp_data.ignore_next_download)
    {
        orion::log_verbose("webview reseting ignore download, navigated");
        ORION_WEBKIT_WEBVIEW(self)->cpp_data.ignore_next_download = false;
    }
}

std::string
orion_webkit_webview_get_current_website(OrionWebkitWebview* webview)
{
    const gchar* uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(webview));
    if (!uri) return "";
    return orion::Uri::parse_strict(uri).authority();
}

enum class UserMediaPermissionResponseType {
    allow,
    deny,
    ask,
};

struct UserMediaPermissionRequestArg {
    using SettingValueResolver = std::function<
        orion::Error(std::string setting_value, UserMediaPermissionResponseType* out_type)
    >;

    OrionWebkitWebview* webview{};
    WebKitUserMediaPermissionRequest* request{};
    std::string type{}; // eg., camera, microphone
    std::string setting_key_name{}; // eg., camera_default_setting, microphone_default_setting
    SettingValueResolver setting_value_resolver{};
};

// Call g_object_unref(request) when done with the request
static void
orion_webkit_webview_user_media_request_permission_handler(const UserMediaPermissionRequestArg& arg)
{
    orion::gtk::StrongRefWrapper request_strong(arg.request); // calls g_object_ref to keep the request alive

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(arg.webview));
    if (!root || !MAIN_IS_WINDOW(root)) {
        orion::log_error("orion_webkit_webview_user_media_request_permission_handler: no main window found");
        webkit_permission_request_deny(WEBKIT_PERMISSION_REQUEST(arg.request));
        return;
    }

    OrionWindow* win = ORION_WINDOW(root);
    orion::ProfileId profile_id = orion_window_get_profile_id(win);
    std::string website = orion_webkit_webview_get_current_website(arg.webview);
    auto win_weak = orion::gtk::WeakRefWrapper(win);
    
    orion::GetSettingForWebsiteOrDefaultQuery query{
        .profile_id = profile_id,
        .website = website,
        .key_name = arg.setting_key_name,
    };

    global_core_api->get_setting_for_website_or_default(query,
    [request_strong,website,win_weak,profile_id,arg](orion::GetSettingForWebsiteOrDefaultReply reply){
        VERIFY_LOG(reply.err);

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        OrionWindow* win = ORION_WINDOW(win_strong.get());

        UserMediaPermissionResponseType response_type{};
        VERIFY_LOG(arg.setting_value_resolver(reply.value, &response_type));

        orion::log_debug(
            "orion_webkit_webview_user_media_request_permission_handler: website=%s response_type=%lld",
            website.c_str(), static_cast<int64_t>(response_type)
        );

        switch (response_type) {
            case UserMediaPermissionResponseType::allow: {
                orion::log_info(
                    "allowing user media request of type '%s' for website '%s' automatically",
                    arg.type.c_str(), website.c_str()
                );
                webkit_permission_request_allow(WEBKIT_PERMISSION_REQUEST(request_strong.get()));
                break;
            }
            case UserMediaPermissionResponseType::deny: {
                orion::log_info(
                    "denying user media request of type '%s' for website '%s' automatically",
                    arg.type.c_str(), website.c_str()
                );
                webkit_permission_request_deny(WEBKIT_PERMISSION_REQUEST(request_strong.get()));
                break;
            }
            default: {
                // By default, ask the user
                WebKitPermissionRequest* request = WEBKIT_PERMISSION_REQUEST(request_strong.get());
                PermissionHandler* handler = PermissionHandler::get_static_permission_handler_for(request, profile_id);
                if (!handler) {
                    orion::log_error("no permission handler found for request type: %s", G_OBJECT_TYPE_NAME(request));
                    webkit_permission_request_deny(request);
                    return;
                }
                auto request_shared_unique_ptr = std::make_shared<orion::gtk::GObjectPtr>(
                    (GObject*)g_object_ref(request)
                );
                AdwDialog* dialog = permissions_request_dialog_new(handler, website, request_shared_unique_ptr);
                adw_dialog_present(dialog, GTK_WIDGET(win));
                break;
            }
        }
    });
}

static void
orion_webkit_webview_camera_request_handler(OrionWebkitWebview* self, WebKitUserMediaPermissionRequest* request)
{
    orion_webkit_webview_user_media_request_permission_handler({
        .webview = self,
        .request = request,
        .type = "camera",
        .setting_key_name = "camera_default_setting",
        .setting_value_resolver = [](std::string setting_value, UserMediaPermissionResponseType* out_type){
            RAISE_CHECK(out_type);
            orion::settings::CameraWebsiteSetting setting{};
            int64_t value{};
            VERIFY(orion::settings::unmarshal_value(setting_value, &value));
            setting = static_cast<orion::settings::CameraWebsiteSetting>(value);
            switch (setting) {
                case orion::settings::CameraWebsiteSetting::allow:
                    *out_type = UserMediaPermissionResponseType::allow;
                    break;
                case orion::settings::CameraWebsiteSetting::deny:
                    *out_type = UserMediaPermissionResponseType::deny;
                    break;
                default:
                    *out_type = UserMediaPermissionResponseType::ask;
                    break;
            }
            SUCCEED();
        },
    });
}

static void
orion_webkit_webview_microphone_request_handler(OrionWebkitWebview* self, WebKitUserMediaPermissionRequest* request)
{
    orion_webkit_webview_user_media_request_permission_handler({
        .webview = self,
        .request = request,
        .type = "microphone",
        .setting_key_name = "microphone_default_setting",
        .setting_value_resolver = [](std::string setting_value, UserMediaPermissionResponseType* out_type){
            RAISE_CHECK(out_type);
            orion::settings::MicrophoneWebsiteSetting setting{};
            int64_t value{};
            VERIFY(orion::settings::unmarshal_value(setting_value, &value));
            setting = static_cast<orion::settings::MicrophoneWebsiteSetting>(value);
            switch (setting) {
                case orion::settings::MicrophoneWebsiteSetting::allow:
                    *out_type = UserMediaPermissionResponseType::allow;
                    break;
                case orion::settings::MicrophoneWebsiteSetting::deny:
                    *out_type = UserMediaPermissionResponseType::deny;
                    break;
                default:
                    *out_type = UserMediaPermissionResponseType::ask;
                    break;
            }
            SUCCEED();
        },
    });
}

static void
orion_webkit_webview_screen_sharing_request_handler(OrionWebkitWebview* self, WebKitUserMediaPermissionRequest* request)
{
    orion_webkit_webview_user_media_request_permission_handler({
        .webview = self,
        .request = request,
        .type = "screen_sharing",
        .setting_key_name = "screen_sharing_default_setting",
        .setting_value_resolver = [](std::string setting_value, UserMediaPermissionResponseType* out_type){
            RAISE_CHECK(out_type);
            orion::settings::ScreenSharingWebsiteSetting setting{};
            int64_t value{};
            VERIFY(orion::settings::unmarshal_value(setting_value, &value));
            setting = static_cast<orion::settings::ScreenSharingWebsiteSetting>(value);
            switch (setting) {
                case orion::settings::ScreenSharingWebsiteSetting::allow:
                    *out_type = UserMediaPermissionResponseType::allow;
                    break;
                case orion::settings::ScreenSharingWebsiteSetting::deny:
                    *out_type = UserMediaPermissionResponseType::deny;
                    break;
                default:
                    *out_type = UserMediaPermissionResponseType::ask;
                    break;
            }
            SUCCEED();
        },
    });
}

// Return value:
// TRUE = stop propagation of the event to other handlers
// FALSE = propagate the event to other handlers
static gboolean
orion_webkit_webview_permission_request_signal_handler(
    WebKitWebView* webview, WebKitPermissionRequest* request, gpointer user_data)
{
    orion::log_debug("orion_webkit_webview_permission_request_signal_handler");

    // If the request cannot be handled synchronously, use g_object_ref(request) to keep it alive until
    // the async callback handles it.

    if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(request)) {
        WebKitUserMediaPermissionRequest* media_request = WEBKIT_USER_MEDIA_PERMISSION_REQUEST(request);

        gboolean is_video = webkit_user_media_permission_is_for_video_device(media_request);
        gboolean is_audio = webkit_user_media_permission_is_for_audio_device(media_request);
        gboolean is_display = webkit_user_media_permission_is_for_display_device(media_request);

        orion::log_debug(
            "user media request: video=%d audio=%d display=%d",
            is_video, is_audio, is_display
        );

        if (is_video) {
            orion_webkit_webview_camera_request_handler(ORION_WEBKIT_WEBVIEW(webview), media_request);
        } else if (is_audio) {
            orion_webkit_webview_microphone_request_handler(ORION_WEBKIT_WEBVIEW(webview), media_request);
        } else if (is_display) {
            orion_webkit_webview_screen_sharing_request_handler(ORION_WEBKIT_WEBVIEW(webview), media_request);
        }
        return TRUE;
    }
    if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(request)) {
        return orion_webkit_webview_permission_request(webview, request);
    }
    return TRUE;
}

static void
on_camera_capture_state_changed(WebKitWebView* web_view, GParamSpec* pspec, gpointer user_data)
{
    WebKitMediaCaptureState state = webkit_web_view_get_camera_capture_state(web_view);
    orion::log_debug("camera capture state: %d", state);  // 0=none, 1=active, 2=muted
}

static void
motion_cb(GtkEventControllerMotion* controller, gdouble x, gdouble y, gpointer user_data)
{
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
    OrionWebkitWebview* orion_webview = ORION_WEBKIT_WEBVIEW(widget);
    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(widget));
    if (!win) {
        orion::log_warning("motion_cb: no window found for webview");
        return;
    }

    // Shift the coordinates from the webview's coordinate space to the main window's coordinate space
    graphene_point_t point = {static_cast<float>(x), static_cast<float>(y)};
    graphene_point_t result{};

    if (!gtk_widget_compute_point(GTK_WIDGET(orion_webview), GTK_WIDGET(win), &point, &result)) {
        orion::log_warning("motion_cb: failed to compute point for webview");
        return;
    }

    orion_webview->last_mouse_x = result.x;
    orion_webview->last_mouse_y = result.y;
}

static void
on_disable_sticky_headers_trigger(GObject* source, GAsyncResult* res, gpointer data)
{
    //clear epoch if matches

    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(source);

    GError* err = NULL;
    JSCValue* val = webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(self), res, &err);
    if(err)
    {
        orion::log_error("on_disable_sticky_headers_trigger(): %s", err->message);
        g_error_free(err);
        return;
    }

    defer([val]{ g_object_unref(val); });

    if(!jsc_value_is_string(val))
    {
        orion::log_error("on_disable_sticky_headers_trigger(): got invalid value");
        return;
    }

    char* str = jsc_value_to_string(val);
    defer([str]{ g_free(str); });

    if(self->cpp_data.page_tweaks_epoch == str)
    {
        self->cpp_data.page_tweaks_epoch.clear();
    }
}

static void
on_page_tweaks_epoch_received(WebKitUserContentManager* mgr, JSCValue* value, OrionWebkitWebview* self)
{
    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    ProfileDataHolder* holder = orion_window_get_profile_data_holder(win);
    
    const char* uri_c_str = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(self));
    if(!uri_c_str) return;

    orion::Uri uri = orion::Uri::parse_strict(uri_c_str);
    std::string host = uri.authority();

    auto& page_tweaks_disabled_filter = profile_data_holder_get_page_tweaks_disabled_filter(holder);
    auto& sticky_headers_disabled_filter = profile_data_holder_get_page_tweaks_sticky_disabled_filter(holder);
    

    //epoch script emits a UUID epoch upon first document show, then on subsequent pageshow events with the persisted flag (bfcache hit):
    //UUID epoch if still enabled (so our disable_sticky_headers_trigger_script hasn't run yet) 
    //null if disabled (our disable_sticky_headers_trigger_script has run)
    if(self->page_tweaks_disable_sticky_list_watch && jsc_value_is_string(value)) 
    {
        char* epoch_c_str = jsc_value_to_string(value);
        defer([epoch_c_str]{ g_free(epoch_c_str); });

        orion::log_verbose("page tweaks epoch received: %s", epoch_c_str);

        self->cpp_data.page_tweaks_epoch = epoch_c_str;

        if (sticky_headers_disabled_filter.is_match_or_subdomain(host) && !page_tweaks_disabled_filter.is_match_or_subdomain(host))
        {
            std::string script = orion::gtk::run_against_epoch(orion::gtk::get_disable_sticky_headers_trigger_script(), self->cpp_data.page_tweaks_epoch);

            webkit_web_view_evaluate_javascript(
                WEBKIT_WEB_VIEW(self),
                orion::gtk::get_disable_sticky_headers_trigger_script().data(),
                orion::gtk::get_disable_sticky_headers_trigger_script().length(),
                PAGE_TWEAKS_SCRIPT_WORLD,
                NULL,
                NULL,
                on_disable_sticky_headers_trigger,
                NULL
            );
        }
    }
    else if(jsc_value_is_null(value))
    {
        orion::log_debug("null received");
        if (!self->page_tweaks_disable_sticky_list_watch ||
            !sticky_headers_disabled_filter.is_match_or_subdomain(host) || page_tweaks_disabled_filter.is_match_or_subdomain(host))
        {
            webkit_web_view_reload(WEBKIT_WEB_VIEW(self));
        }
    }
}

//method will always be set to non NULL specifically on success
//arguments are nullable
static bool
orion_internals_call_validate_extract(JSCValue* value, int32_t* epoch, char** method)
{
    if(!jsc_value_is_object(value))
    {
        char* str = jsc_value_to_string(value);
        defer([str]{ g_free(str); });
        orion::log_error("orion internals call with invalid js value %s", str);
        return false;
    }

    if(!jsc_value_object_has_property(value, "epoch"))
    {
        orion::log_error("orion internals call without epoch");
        return false;
    }
    if(!jsc_value_object_has_property(value, "method"))
    {
        orion::log_error("orion internals call without method");
        return false;
    }

    JSCValue* epoch_val = jsc_value_object_get_property(value, "epoch");
    defer([epoch_val]{ g_object_unref(epoch_val); });
    if(!jsc_value_is_number(epoch_val))
    {
        orion::log_error("orion internals call with non numeric epoch");
        return false;
    }
    if(epoch) *epoch = jsc_value_to_int32(epoch_val);

    JSCValue* method_val = jsc_value_object_get_property(value, "method");
    defer([method_val]{ g_object_unref(method_val); });
    if(!jsc_value_is_string(method_val))
    {
        orion::log_error("orion internals call with non numeric epoch");
        return false;
    }
    if(method) *method = jsc_value_to_string(method_val);
    return true;
}

static void
on_orion_internals_call_received(WebKitUserContentManager* mgr, JSCValue* value, OrionWebkitWebview* self)
{
    int32_t epoch = 0;
    char* method = NULL;
    if(!orion_internals_call_validate_extract(value, &epoch, &method)) return;

    OrionWindow* orion_win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!MAIN_IS_WINDOW(orion_win)) return;
    MainWindow* win = MAIN_WINDOW(orion_win);
    AdwOverlaySplitView* js_sidebar_split_view = main_window_get_js_sidebar_split_view(win);
    JsSidebar* js_sidebar = APP_JS_SIDEBAR(adw_overlay_split_view_get_sidebar(js_sidebar_split_view));
    
    if(!js_sidebar_match_epoch(js_sidebar, epoch))
    {
        orion::log_verbose("failed to match current js sidebar epoch, attempted value: " PRId32, epoch);
        return;
    }

    if(0 == g_strcmp0(method, "setSidebarContent")) {

        if(!jsc_value_object_has_property(value, "content"))
        {
            orion::log_error("orion internals call: setSidebarContent didn't receive content");
            return;
        }

        JSCValue* content_val = jsc_value_object_get_property(value, "content");
        defer([content_val]{ g_object_unref(content_val); });
        //intentionally not validating string status and instead letting jsc convert to string
        char* str = jsc_value_to_string(content_val);
        defer([str]{ g_free(str); }); //rather unnecessary
        
        js_sidebar_set_content(js_sidebar, epoch, str);

    } else if(0 == g_strcmp0(method, "addSidebarContent")) {

        if(!jsc_value_object_has_property(value, "content"))
        {
            orion::log_error("orion internals call: setSidebarContent didn't receive content");
            return;
        }

        JSCValue* content_val = jsc_value_object_get_property(value, "content");
        defer([content_val]{ g_object_unref(content_val); });
        //intentionally not validating string status and instead letting jsc convert to string
        char* str = jsc_value_to_string(content_val);
        defer([str]{ g_free(str); }); //rather unnecessary
        
        js_sidebar_add_content(js_sidebar, epoch, str);

    } else if(0 == g_strcmp0(method, "clearSidebarContent")) {

        js_sidebar_clear(js_sidebar, epoch);

    } else if(0 == g_strcmp0(method, "openSidebarContent")) {

        adw_overlay_split_view_set_show_sidebar(js_sidebar_split_view, TRUE);

    } else if(0 == g_strcmp0(method, "closeSidebarContent")) {

        adw_overlay_split_view_set_show_sidebar(js_sidebar_split_view, FALSE);

    } else {
        //method should never be NULL here
        orion::log_error("orion internals call got invalid method: %s", method);
    }
}

static void
on_orion_internals_async_call_received(WebKitUserContentManager* mgr, JSCValue* value, WebKitScriptMessageReply* reply, OrionWebkitWebview* self)
{
    int32_t epoch = 0;
    char* method = NULL;
    if(!orion_internals_call_validate_extract(value, &epoch, &method))
    {
        webkit_script_message_reply_return_error_message(reply, "failed to extract epoch or method from call");
        return;
    }

    OrionWindow* orion_win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!MAIN_IS_WINDOW(orion_win)) return;
    MainWindow* win = MAIN_WINDOW(orion_win);
    AdwOverlaySplitView* js_sidebar_split_view = main_window_get_js_sidebar_split_view(win);
    JsSidebar* js_sidebar = APP_JS_SIDEBAR(adw_overlay_split_view_get_sidebar(js_sidebar_split_view));

    if(!js_sidebar_match_epoch(js_sidebar, epoch))
    {
        webkit_script_message_reply_return_error_message(reply, "epoch doesn't match current");
        return;
    }

    if(0 == g_strcmp0(method, "getSidebarContent")) {

        char* text = js_sidebar_get_content(js_sidebar);
        defer([text]{ g_free(text); });

        JSCContext* ctx = jsc_context_new();
        JSCValue* reply_value = jsc_value_new_string(ctx, text);
        defer([reply_value]{ g_object_unref(reply_value); });

        webkit_script_message_reply_return_value(reply, reply_value);

    } else if(0 == g_strcmp0(method, "readerContent")) {

        //FIXME: implement
        webkit_script_message_reply_return_error_message(reply, "TODO: implement");
        //webkit_script_message_reply_ref(reply);

    } else {
        //method should never be NULL here
        orion::log_error("orion internals async call got invalid method: %s", method);
    }
}


//End of webviews signal handlers

static void
register_page_tweaks_disable_sticky_handlers(OrionWebkitWebview* self)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(self));

    webkit_user_content_manager_register_script_message_handler(mgr, "pageTweaksEpoch", PAGE_TWEAKS_SCRIPT_WORLD);
    g_signal_connect(mgr, "script-message-received::pageTweaksEpoch", G_CALLBACK(on_page_tweaks_epoch_received), self);

    std::string epoch_script = orion::gtk::get_page_tweaks_epoch_script("pageTweaksEpoch");
    //TODO: this should probably be shared globally
    WebKitUserScript* script = webkit_user_script_new_for_world(
        epoch_script.c_str(),
        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
        PAGE_TWEAKS_SCRIPT_WORLD,
        NULL,
        NULL
    );

    webkit_user_content_manager_add_script(mgr, script);

    webkit_user_script_unref(script);
}

static void
register_orion_internals_handler(OrionWebkitWebview* self)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(self));

    webkit_user_content_manager_register_script_message_handler(mgr, "orionInternals", PROGRAMMABLE_BUTTON_SCRIPT_WORLD);
    webkit_user_content_manager_register_script_message_handler_with_reply(mgr, "orionInternalsAsync", PROGRAMMABLE_BUTTON_SCRIPT_WORLD);
    g_signal_connect(mgr, "script-message-received::orionInternals", G_CALLBACK(on_orion_internals_call_received), self);
    g_signal_connect(mgr, "script-message-with-reply-received::orionInternalsAsync", G_CALLBACK(on_orion_internals_async_call_received), self);
}

void
orion_webkit_webview_leave_webkit_fullscreen(OrionWebkitWebview* self)
{
    webkit_web_view_evaluate_javascript(
        WEBKIT_WEB_VIEW(self),
        "document.exitFullscreen();",
        -1,
        LEAVE_FULLSCREEN_SCRIPT_WORLD,
        NULL,
        NULL,
        NULL,
        NULL
    );
}

static void
orion_webview_constructed(GObject* object)
{
    G_OBJECT_CLASS(orion_webkit_webview_parent_class)->constructed(object);

    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(object);
    WebKitBackForwardList* back_forward_list = webkit_web_view_get_back_forward_list(WEBKIT_WEB_VIEW(self));
    g_signal_connect_object(back_forward_list, "changed", G_CALLBACK(orion_webkit_webview_on_back_forward_list_changed), self, G_CONNECT_DEFAULT);

    g_signal_connect(
        WEBKIT_WEB_VIEW(self), "permission-request",
        G_CALLBACK(orion_webkit_webview_permission_request_signal_handler), NULL);
    g_signal_connect(
        WEBKIT_WEB_VIEW(self), "notify::camera-capture-state",
        G_CALLBACK(on_camera_capture_state_changed), NULL);

    GtkEventController* motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "motion", G_CALLBACK(motion_cb), NULL);
    // Takes ownership of the controller object
    gtk_widget_add_controller(GTK_WIDGET(self), motion_controller);

    WebKitSettings* settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(self));

    // Attempt to make camera/microphone access work
    webkit_settings_set_enable_media_stream(settings, TRUE);
    webkit_settings_set_enable_webrtc(settings, TRUE);   // often needed too

    register_page_tweaks_disable_sticky_handlers(self);
    register_orion_internals_handler(self);

    self->constructed = true;
}

static void
orion_webkit_webview_back_action(OrionWebkitWebview* self, GVariant* parameter)
{
    g_signal_emit(self, obj_signals[SIG_GO_BACK], 0);
}

static void
orion_webkit_webview_forward_action(OrionWebkitWebview* self, GVariant* parameter)
{
    g_signal_emit(self, obj_signals[SIG_GO_FORWARD], 0);
}

GAction*
orion_webkit_webview_get_go_back_action(OrionWebkitWebview* webview)
{
    return G_ACTION(webview->go_back_action);
}

GAction*
orion_webkit_webview_get_go_forward_action(OrionWebkitWebview* webview)
{
    return G_ACTION(webview->go_forward_action);
}

static void
apply_remove_trackers_from_urls_mode(OrionWebkitWebview* self, orion::settings::RemoveTrackersFromUrlsMode mode)
{
    using Mode = orion::settings::RemoveTrackersFromUrlsMode;

    if(mode == Mode::not_set)
    {
        mode = orion::settings::privacy::remove_trackers_from_urls_mode.default_value();
    }

    switch(mode)
    {
        case Mode::not_set:
        case Mode::for_private_browsing_only:
            self->strip_links_in_non_private = false;
            break;
        case Mode::for_all_browsing:
            self->strip_links_in_non_private = true;
            break;
    }
}

static void
on_setting_changed(ProfileDataHolder* holder, const char* key, const char* raw_value, OrionWebkitWebview* self)
{
    if(orion::settings::privacy::remove_trackers_from_urls_mode.key() == key)
    {
        using Mode = orion::settings::RemoveTrackersFromUrlsMode;

        auto mode = Mode::not_set;
        if(raw_value)
        {
            if(orion::Error err = orion::settings::unmarshal_value(raw_value, &mode))
            {
                LOG_ERROR(err);
            } 
        }
        apply_remove_trackers_from_urls_mode(self, mode);
    }
}

static void
data_holder_notify(gpointer data)
{
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(data);

    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    ProfileDataHolder* holder = orion_window_get_profile_data_holder(win);

    auto old_holder_strong = self->cpp_data.last_profile_data_holder.lock();
    ProfileDataHolder* old_holder = APP_PROFILE_DATA_HOLDER(old_holder_strong.get());

    if(old_holder == holder) return;

    if(old_holder)
    {
        g_signal_handlers_disconnect_by_data(old_holder, self);
    }

    if(holder)
    {
        g_signal_connect_object(holder, "setting-changed", G_CALLBACK(on_setting_changed), self, G_CONNECT_DEFAULT);
        auto mode = profile_data_holder_get_setting_val(holder, orion::settings::privacy::remove_trackers_from_urls_mode);
        apply_remove_trackers_from_urls_mode(self, mode);
    }

    self->cpp_data.last_profile_data_holder.reset(holder);
}

static void
private_mode_notify(gpointer data)
{
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(data);

    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    self->cpp_data.private_mode = orion_window_get_private_mode(win);
}

static void
register_link_strip_setting_observer(OrionWebkitWebview* self)
{
    GtkExpression* root_expression = gtk_property_expression_new(GTK_TYPE_WIDGET, NULL, "root");
    
    GtkExpression* data_holder_expression = gtk_property_expression_new(ORION_TYPE_WINDOW, gtk_expression_ref(root_expression), "profile-data-holder");

    self->data_holder_watch = gtk_expression_watch(data_holder_expression, self, data_holder_notify, self, NULL);
    gtk_expression_unref(data_holder_expression);


    GtkExpression* private_mode_expression = gtk_property_expression_new(ORION_TYPE_WINDOW, root_expression, "private-mode");

    self->private_mode_watch = gtk_expression_watch(private_mode_expression, self, private_mode_notify, self, NULL);
    gtk_expression_unref(private_mode_expression);
}

static void
orion_webkit_webview_init(OrionWebkitWebview* self)
{
    std::construct_at(&self->cpp_data);

    g_signal_connect(WEBKIT_WEB_VIEW(self), "web-process-terminated", G_CALLBACK(orion_webkit_webview_web_process_terminated), NULL);

    GSimpleActionGroup* group = g_simple_action_group_new();

    self->go_back_action = g_simple_action_new("go-back", NULL);
    self->go_forward_action = g_simple_action_new("go-forward", NULL);

    g_signal_connect_data(self->go_back_action, "activate", G_CALLBACK(orion_webkit_webview_back_action), self, NULL, G_CONNECT_SWAPPED);
    g_signal_connect_data(self->go_forward_action, "activate", G_CALLBACK(orion_webkit_webview_forward_action), self, NULL, G_CONNECT_SWAPPED);

    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(self->go_back_action));
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(self->go_forward_action));

    gtk_widget_insert_action_group(GTK_WIDGET(self), "webview", G_ACTION_GROUP(group));
    g_object_unref(group);

    register_link_strip_setting_observer(self);

    // GtkGesture* click = gtk_gesture_click_new();
    // gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0); // all buttons
    // gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(click));
    // g_signal_connect(click, "pressed", G_CALLBACK(orion_webkit_webview_pressed), self);
}

static void
orion_webview_finalize(GObject* object)
{
    orion::log_verbose("orion_webview_finalize");
    
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(object);
    self->cpp_data.last_tls_certificate.reset(nullptr);
    std::destroy_at(&self->cpp_data);
    g_clear_object(&self->go_back_action);
    g_clear_object(&self->go_forward_action);

    G_OBJECT_CLASS(orion_webkit_webview_parent_class)->finalize(object);
}

static void
orion_webview_dispose(GObject* object)
{
    orion::log_verbose("orion_webview_dispose");
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(object);

    g_clear_object(&self->last_hit_test_result);
    
    g_clear_pointer(&self->data_holder_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->private_mode_watch, gtk_expression_watch_unwatch);

    g_clear_pointer(&self->page_tweaks_font_size_style_sheets_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->page_tweaks_dark_mode_style_sheet_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->page_tweaks_font_family_style_sheets_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->page_tweaks_disable_sticky_list_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->page_tweaks_element_blockers_watch, gtk_expression_watch_unwatch);

    g_clear_pointer(&self->page_tweaks_dark_mode_style_sheet, webkit_user_style_sheet_unref);
    for(WebKitUserStyleSheet*& sheet : self->cpp_data.page_tweaks_font_size_style_sheets)
    {
        g_clear_pointer(&sheet, webkit_user_style_sheet_unref);
    }
    self->cpp_data.page_tweaks_font_size_style_sheets.clear();

    for(auto& pair : self->cpp_data.page_tweaks_element_blockers)
    {
        if(pair.second) webkit_user_style_sheet_unref(pair.second);
    }
    self->cpp_data.page_tweaks_element_blockers.clear();

    G_OBJECT_CLASS(orion_webkit_webview_parent_class)->dispose(object);
}

static void
page_tweaks_dark_mode_expression_notify(gpointer data)
{
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(data);

    GValue val = G_VALUE_INIT;
    g_value_init(&val, WEBKIT_TYPE_USER_STYLE_SHEET);
    if(gtk_expression_watch_evaluate(self->page_tweaks_dark_mode_style_sheet_watch, &val))
    {
        WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(self));
        if(self->page_tweaks_dark_mode_style_sheet)
        {
            webkit_user_content_manager_remove_style_sheet(mgr, self->page_tweaks_dark_mode_style_sheet);
            g_clear_pointer(&self->page_tweaks_dark_mode_style_sheet, webkit_user_style_sheet_unref);
            
        }
        WebKitUserStyleSheet* sheet = static_cast<WebKitUserStyleSheet*>(g_value_get_boxed(&val));
        if(sheet)
        {
            self->page_tweaks_dark_mode_style_sheet = webkit_user_style_sheet_ref(sheet);
            webkit_user_content_manager_add_style_sheet(mgr, self->page_tweaks_dark_mode_style_sheet);
        }
    }
    g_value_unset(&val);
}

static void
page_tweaks_disable_sticky_expression_notify(gpointer data)
{
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(data);

    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    ProfileDataHolder* holder = orion_window_get_profile_data_holder(win);
    
    const char* uri_c_str = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(self));
    if(!uri_c_str) return;

    orion::Uri uri = orion::Uri::parse_strict(uri_c_str);
    std::string host = uri.authority();

    auto& page_tweaks_disabled_filter = profile_data_holder_get_page_tweaks_disabled_filter(holder);
    auto& sticky_headers_disabled_filter = profile_data_holder_get_page_tweaks_sticky_disabled_filter(holder);

    if (sticky_headers_disabled_filter.is_match_or_subdomain(host) && !page_tweaks_disabled_filter.is_match_or_subdomain(host))
    {
        if(!self->cpp_data.page_tweaks_epoch.length()) return;

        std::string script = orion::gtk::run_against_epoch(orion::gtk::get_disable_sticky_headers_trigger_script(), self->cpp_data.page_tweaks_epoch);

        webkit_web_view_evaluate_javascript(
            WEBKIT_WEB_VIEW(self),
            orion::gtk::get_disable_sticky_headers_trigger_script().data(),
            orion::gtk::get_disable_sticky_headers_trigger_script().length(),
            PAGE_TWEAKS_SCRIPT_WORLD,
            NULL,
            NULL,
            on_disable_sticky_headers_trigger,
            NULL
        );
    }
    else
    {
        if(self->cpp_data.page_tweaks_epoch.length()) return;

        webkit_web_view_reload(WEBKIT_WEB_VIEW(self));
    }
}

static inline void
apply_new_style_sheets_vector(WebKitUserContentManager* mgr, std::vector<WebKitUserStyleSheet*>& curr_sheets, std::vector<WebKitUserStyleSheet*>& new_sheets)
{
    for(auto it_curr = curr_sheets.begin(); it_curr != curr_sheets.end(); )
    {
        auto it_new = std::find(new_sheets.begin(), new_sheets.end(), *it_curr);

        if(it_new == new_sheets.end())
        {
            webkit_user_content_manager_remove_style_sheet(mgr, *it_curr);
            webkit_user_style_sheet_unref(*it_curr);
            it_curr = curr_sheets.erase(it_curr);
        }
        else
        {
            new_sheets.erase(it_new);
            ++it_curr;
        }
    }

    for(auto& new_sheet : new_sheets)
    {
        curr_sheets.push_back(webkit_user_style_sheet_ref(new_sheet));
        webkit_user_content_manager_add_style_sheet(mgr, new_sheet);
    }
}

static void
page_tweaks_font_size_expression_notify(gpointer data)
{
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(data);

    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    ProfileDataHolder* holder = orion_window_get_profile_data_holder(win);
    if(!holder) return;

    const orion::TypeToWebKitUserStyleSheetMap<orion::settings::PageZoom>& map = profile_data_holder_get_page_tweaks_font_size_map(holder);

    std::vector<WebKitUserStyleSheet*> new_sheets;
    for(auto& map_pair : map)
    {
        WebKitUserStyleSheet* sheet = map_pair.second.second;
        if(sheet)
        {
            new_sheets.push_back(sheet);
        }
    }

    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(self));

    auto& curr_sheets = self->cpp_data.page_tweaks_font_size_style_sheets;
    apply_new_style_sheets_vector(mgr, curr_sheets, new_sheets);
}

static void
page_tweaks_font_family_expression_notify(gpointer data)
{
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(data);

    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    ProfileDataHolder* holder = orion_window_get_profile_data_holder(win);
    if(!holder) return;

    const orion::TypeToWebKitUserStyleSheetMap<std::string>& map = profile_data_holder_get_page_tweaks_font_family_map(holder);

    std::vector<WebKitUserStyleSheet*> new_sheets;
    for(auto& map_pair : map)
    {
        WebKitUserStyleSheet* sheet = map_pair.second.second;
        if(sheet)
        {
            new_sheets.push_back(sheet);
        }
    }

    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(self));

    auto& curr_sheets = self->cpp_data.page_tweaks_font_family_style_sheets;
    apply_new_style_sheets_vector(mgr, curr_sheets, new_sheets);
}

static void
page_tweaks_element_blockers_expression_notify(gpointer data)
{
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(data);

    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    ProfileDataHolder* holder = orion_window_get_profile_data_holder(win);
    if(!holder) return;

    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(self));

    auto& curr_state = self->cpp_data.page_tweaks_element_blockers;
    auto& new_state = profile_data_holder_get_page_tweaks_content_blockers_map(holder);

    auto& page_tweaks_disabled_filter = profile_data_holder_get_page_tweaks_disabled_filter(holder);
    auto is_enabled = [&page_tweaks_disabled_filter](const std::string& key){
        return !page_tweaks_disabled_filter.is_match_or_subdomain(key);
    };

    auto curr_it = curr_state.begin();
    auto new_it = new_state.begin();

    //updates current state by iterating through both states to find added/removed/updated, preserves NULLs in curr_state as they represent temporarily disabled sheets
    while(curr_it != curr_state.end() && new_it != new_state.end())
    {
        if(curr_it->first < new_it->first) //item was removed - delete if not NULL
        {
            if(curr_it->second) //check to preserve NULL
            {
                webkit_user_content_manager_remove_style_sheet(mgr, curr_it->second);
                webkit_user_style_sheet_unref(curr_it->second);
                curr_it = curr_state.erase(curr_it);
            }
            else
            {
                ++curr_it;
            }
        }
        else if(curr_it->first > new_it->first) //item was added - add if enabled
        {
            if(is_enabled(new_it->first))
            {
                curr_state.insert(curr_it, {new_it->first, webkit_user_style_sheet_ref(new_it->second)});
                webkit_user_content_manager_add_style_sheet(mgr, new_it->second);
            }
            ++new_it;
        }
        else //item is same - swap if enabled, different and current is non NULL, otherwise remove if non NULL
        {
            if(is_enabled(curr_it->first))
            {
                if(curr_it->second != new_it->second)
                {
                    if(curr_it->second) //check to preserve NULL
                    {
                        webkit_user_content_manager_remove_style_sheet(mgr, curr_it->second);
                        webkit_user_style_sheet_unref(curr_it->second);

                        curr_it->second = webkit_user_style_sheet_ref(new_it->second);
                        webkit_user_content_manager_add_style_sheet(mgr, new_it->second);
                    }
                }
                ++curr_it;
                ++new_it;
            }
            else
            {
                if(curr_it->second) //check to preserve NULL
                {
                    webkit_user_content_manager_remove_style_sheet(mgr, curr_it->second);
                    webkit_user_style_sheet_unref(curr_it->second);
                    curr_it = curr_state.erase(curr_it);
                }
                else
                {
                    ++curr_it;
                }
                ++new_it;
            }
        }
    }

    while(curr_it != curr_state.end()) //removed items tail - remove if non NULL
    {
        if(curr_it->second) //check to preserve NULL
        {
            webkit_user_content_manager_remove_style_sheet(mgr, curr_it->second);
            webkit_user_style_sheet_unref(curr_it->second);
            curr_it = curr_state.erase(curr_it);
        }
        else
        {
            ++curr_it;
        }
    }

    while(new_it != new_state.end()) //addeed items tail - add if enabled
    {
        if(is_enabled(new_it->first))
        {
            curr_state.insert(curr_state.end(), {new_it->first, webkit_user_style_sheet_ref(new_it->second)});
            webkit_user_content_manager_add_style_sheet(mgr, new_it->second);
        }
        ++new_it;
    }
}

void
orion_webkit_webview_set_page_tweaks_enabled(OrionWebkitWebview* self, gboolean enabled)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(self));
    
    g_clear_pointer(&self->page_tweaks_font_size_style_sheets_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->page_tweaks_dark_mode_style_sheet_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->page_tweaks_font_family_style_sheets_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->page_tweaks_disable_sticky_list_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->page_tweaks_element_blockers_watch, gtk_expression_watch_unwatch);
    
    if(enabled)
    {
        GtkExpression* root_expression = gtk_property_expression_new(GTK_TYPE_WIDGET, NULL, "root");
        GtkExpression* profile_data_holder_expression = gtk_property_expression_new(MAIN_WINDOW_TYPE, root_expression, "profile-data-holder");
        
        GtkExpression* page_tweaks_font_size_style_sheets_expr = gtk_property_expression_new(
            APP_TYPE_PROFILE_DATA_HOLDER, gtk_expression_ref(profile_data_holder_expression), "page-tweaks-font-size-map");
        self->page_tweaks_font_size_style_sheets_watch = gtk_expression_watch(
            page_tweaks_font_size_style_sheets_expr, self, page_tweaks_font_size_expression_notify, self, NULL);
        gtk_expression_unref(page_tweaks_font_size_style_sheets_expr);
        page_tweaks_font_size_expression_notify(self);

        GtkExpression* dark_mode_style_sheet_expression = gtk_property_expression_new(
            APP_TYPE_PROFILE_DATA_HOLDER, gtk_expression_ref(profile_data_holder_expression), "page-tweaks-dark-mode-style-sheet");
        self->page_tweaks_dark_mode_style_sheet_watch = gtk_expression_watch(
            dark_mode_style_sheet_expression,self, page_tweaks_dark_mode_expression_notify, self, NULL);
        gtk_expression_unref(dark_mode_style_sheet_expression);
        page_tweaks_dark_mode_expression_notify(self);

        GtkExpression* page_tweaks_font_family_style_sheets_expr = gtk_property_expression_new(
            APP_TYPE_PROFILE_DATA_HOLDER, gtk_expression_ref(profile_data_holder_expression), "page-tweaks-font-family-map");
        self->page_tweaks_font_family_style_sheets_watch = gtk_expression_watch(
            page_tweaks_font_family_style_sheets_expr, self, page_tweaks_font_family_expression_notify, self, NULL);
        gtk_expression_unref(page_tweaks_font_family_style_sheets_expr);
        page_tweaks_font_family_expression_notify(self);
        
        GtkExpression* disable_sticky_list_expression = gtk_property_expression_new(
            APP_TYPE_PROFILE_DATA_HOLDER, gtk_expression_ref(profile_data_holder_expression), "page-tweaks-disable-sticky-filter");
        self->page_tweaks_disable_sticky_list_watch = gtk_expression_watch(
            disable_sticky_list_expression,self, page_tweaks_disable_sticky_expression_notify, self, NULL);
        gtk_expression_unref(disable_sticky_list_expression);
        page_tweaks_disable_sticky_expression_notify(self);
        
        GtkExpression* page_tweaks_element_blockers_expression = gtk_property_expression_new(
            APP_TYPE_PROFILE_DATA_HOLDER, profile_data_holder_expression, "page-tweaks-element-blockers");
        self->page_tweaks_element_blockers_watch = gtk_expression_watch(
            page_tweaks_element_blockers_expression, self, page_tweaks_element_blockers_expression_notify, self, NULL);
        gtk_expression_unref(page_tweaks_element_blockers_expression);
        page_tweaks_element_blockers_expression_notify(self);
    }
    else
    {

        for(WebKitUserStyleSheet* sheet : self->cpp_data.page_tweaks_font_size_style_sheets)
        {
            webkit_user_content_manager_remove_style_sheet(mgr, sheet);
            webkit_user_style_sheet_unref(sheet);
        }
        self->cpp_data.page_tweaks_font_size_style_sheets.clear();

        if(self->page_tweaks_dark_mode_style_sheet)
        {
            webkit_user_content_manager_remove_style_sheet(mgr, self->page_tweaks_dark_mode_style_sheet);
            webkit_user_style_sheet_unref(self->page_tweaks_dark_mode_style_sheet);
            self->page_tweaks_dark_mode_style_sheet = NULL;
        }
        
        for(WebKitUserStyleSheet* sheet : self->cpp_data.page_tweaks_font_family_style_sheets)
        {
            webkit_user_content_manager_remove_style_sheet(mgr, sheet);
            webkit_user_style_sheet_unref(sheet);
        }
        self->cpp_data.page_tweaks_font_family_style_sheets.clear();

        if(self->constructed && self->cpp_data.page_tweaks_epoch.length() == 0)
        {
            webkit_web_view_reload(WEBKIT_WEB_VIEW(self));
        }
    }
}

void
orion_webkit_webview_set_preview_erased(OrionWebkitWebview* self, std::string_view selectors, std::string_view host, gboolean value)
{
    auto& map = self->cpp_data.page_tweaks_element_blockers;
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(self));
    if(value)
    {
        if(map.contains(host))
        {
            auto it = map.find(host);
            WebKitUserStyleSheet* sheet = it->second;
            if(sheet)
            {
                webkit_user_content_manager_remove_style_sheet(mgr, sheet);
                webkit_user_style_sheet_unref(sheet);
                it->second = NULL;
            }
        }
        else
        {
            map.try_emplace(std::string(host), (WebKitUserStyleSheet*)NULL);
        }

        
        std::string script = orion::gtk::get_set_already_erased_selectors_script(selectors);
        webkit_web_view_evaluate_javascript(
            WEBKIT_WEB_VIEW(self),
            script.c_str(),
            script.length(),
            PAGE_TWEAKS_SCRIPT_WORLD,
            NULL,
            NULL,
            NULL,
            NULL
        );
    }
    else
    {
        if(!map.contains(host) || map.find(host)->second)
        {
            //NOTE: not treating this as an error, to allow erase_elements_bar to call this unconditionally
            //orion::log_error("orion_webkit_webview_set_preview_erased(): entry does not exist or style sheet not NULL for host: %s", host.c_str());
            return;
        }
        
        OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
        if(!win) return;
        auto* profile_data_holder = orion_window_get_profile_data_holder(win);
        if(!profile_data_holder) return;
        auto& page_tweaks_element_blockers_map = profile_data_holder_get_page_tweaks_content_blockers_map(profile_data_holder);

        auto it = map.find(host);
        if(page_tweaks_element_blockers_map.contains(host))
        {
            WebKitUserStyleSheet* sheet = page_tweaks_element_blockers_map.find(host)->second;
            it->second = webkit_user_style_sheet_ref(sheet);
            webkit_user_content_manager_add_style_sheet(mgr, sheet);
        }
        else
        {
            map.erase(it);
        }

        std::string script = orion::gtk::get_set_already_erased_selectors_script("");
        webkit_web_view_evaluate_javascript(
            WEBKIT_WEB_VIEW(self),
            script.c_str(),
            script.length(),
            PAGE_TWEAKS_SCRIPT_WORLD,
            NULL,
            NULL,
            NULL,
            NULL
        );
    }
}

void
orion_webkit_webview_update_preview_erased_selectors(OrionWebkitWebview* self, std::string_view selectors)
{
    std::string script = orion::gtk::get_set_already_erased_selectors_script(selectors);
    webkit_web_view_evaluate_javascript(
        WEBKIT_WEB_VIEW(self),
        script.c_str(),
        script.length(),
        PAGE_TWEAKS_SCRIPT_WORLD,
        NULL,
        NULL,
        NULL,
        NULL
    );
}

static void
orion_webview_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(object);

    switch(prop_id)
    {
        // case PROP_IS_PROBABLY_READERABLE:
        //     self->is_probably_readerable = g_value_get_boolean(value);
        //     return;
        case PROP_CAN_GO_BACK:
            g_simple_action_set_enabled(self->go_back_action, g_value_get_boolean(value));
            return;
        case PROP_CAN_GO_FORWARD:
            g_simple_action_set_enabled(self->go_forward_action, g_value_get_boolean(value));
            return;
        case PROP_PAGE_TWEAKS_ENABLED:
            orion_webkit_webview_set_page_tweaks_enabled(self, g_value_get_boolean(value));
            return;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            return;
    }
}

static void
orion_webview_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    OrionWebkitWebview* self = ORION_WEBKIT_WEBVIEW(object);

    switch(prop_id)
    {
        // case PROP_IS_PROBABLY_READERABLE:
        //     g_value_set_boolean(value, self->is_probably_readerable);
        //     return;
        case PROP_TLS_STATUS:
            g_value_set_int64(value, gint64(self->tls_status));
            return;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            return;
    }
}

static void
orion_webkit_webview_class_init(OrionWebkitWebviewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = orion_webview_finalize;
    object_class->dispose = orion_webview_dispose;
    object_class->constructed = orion_webview_constructed;
    object_class->set_property = orion_webview_set_property;
    object_class->get_property = orion_webview_get_property;

    // obj_properties[PROP_IS_PROBABLY_READERABLE] = g_param_spec_boolean("is-readerable", NULL, NULL,
    //     FALSE, GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_CAN_GO_BACK] = g_param_spec_boolean("can-go-back", NULL, NULL,
        FALSE, GParamFlags(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
    obj_properties[PROP_CAN_GO_FORWARD] = g_param_spec_boolean("can-go-forward", NULL, NULL,
        FALSE, GParamFlags(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
    obj_properties[PROP_PAGE_TWEAKS_ENABLED] = g_param_spec_boolean("page-tweaks-enabled", NULL, NULL,
        FALSE, GParamFlags(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
    obj_properties[PROP_PAGE_TWEAKS_ENABLED] = g_param_spec_boolean("page-tweaks-enabled", NULL, NULL,
        FALSE, GParamFlags(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));
    obj_properties[PROP_TLS_STATUS] = g_param_spec_int64("tls-status", NULL, NULL, INT64_MIN, INT64_MAX, gint64(orion::TlsStatus::insecure),
        GParamFlags(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_properties(object_class, N_PROPS, obj_properties);

    obj_signals[SIG_GO_BACK] = g_signal_new(
        "go-back",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL,
        NULL,
        NULL,
        G_TYPE_NONE,
        0
    );
    obj_signals[SIG_GO_FORWARD] = g_signal_new(
        "go-forward",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        NULL,
        NULL,
        NULL,
        G_TYPE_NONE,
        0
    );
    

    WebKitWebViewClass *web_view_class = WEBKIT_WEB_VIEW_CLASS(klass);
    web_view_class->authenticate = orion_webkit_webview_authenticate;
    web_view_class->close = orion_webkit_webview_close;
    web_view_class->context_menu = orion_webkit_webview_context_menu;
    web_view_class->context_menu_dismissed = orion_webkit_webview_context_menu_dismissed;
    web_view_class->create = orion_webkit_webview_create;
    web_view_class->decide_policy = orion_webkit_webview_decide_policy;
    web_view_class->enter_fullscreen = orion_webkit_webview_enter_fullscreen;
    web_view_class->leave_fullscreen = orion_webkit_webview_leave_fullscreen;
    web_view_class->load_changed = orion_webkit_webview_load_changed;
    web_view_class->load_failed = orion_webkit_webview_load_failed;
    web_view_class->load_failed_with_tls_errors = orion_webkit_webview_load_failed_with_tls_errors;
    web_view_class->mouse_target_changed = orion_webkit_webview_mouse_target_changed;
    web_view_class->permission_request = orion_webkit_webview_permission_request;
    web_view_class->print = orion_webkit_webview_print;
    web_view_class->query_permission_state = orion_webkit_webview_query_permission_state;
    web_view_class->ready_to_show = orion_webkit_webview_ready_to_show;
    web_view_class->run_color_chooser = orion_webkit_webview_run_color_chooser;
    web_view_class->run_file_chooser = orion_webkit_webview_run_file_chooser;
    web_view_class->script_dialog = orion_webkit_webview_script_dialog;
    web_view_class->show_notification = orion_webkit_webview_show_notification;
    web_view_class->show_option_menu = orion_webkit_webview_show_option_menu;
    web_view_class->submit_form = orion_webkit_webview_submit_form;
    web_view_class->user_message_received = orion_webkit_webview_user_message_received;
    web_view_class->web_process_crashed = orion_webkit_webview_web_process_crashed;
    web_view_class->insecure_content_detected = orion_webkit_webview_insecure_content_detected;
}
#endif
