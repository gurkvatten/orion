#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <cstring>
#include <cctype>

#include <common/src/uri.h>
#include <common/src/log.h>
#include <common/src/defer.h>
#include <common/src/string_util.h>
#include <core/settings.h>
#include <core/scripts/edit_text_on_page_scripts.h>
#include <core/scripts/allow_copy_and_paste_scripts.h>

#include "app.h"

#include "orion_webview_tab_content.h"

#include "util/click_util.h"
#include "util/image_util.h"
#include "util/favicon_util.h"
#include "util/custom_uri_util.h"
#include "util/menu_util.h"
#include "util/dialog_util.h"
#include "util/action_util.h"
#include "util/css_util.h"
#include "util/jsc_util.h"
#include "util/uri_util.h"

#include "data_item/back_forward_list.h"

#include "window/main_window/main_window.h"
#include "window/search_recommendations.h"
#include "window/fill_password_dialog.h"
#include "window/remember_password_dialog.h"
#include "window/update_password_dialog.h"
#include "window/visit_unsecure_website_dialog.h"
#include "action/main_window/action.h"
#include "content_block/embedded_filter_lists.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#include "orion_webkit_webview.h"
#include "webextensions/webview_factory_host.h"
#include "scripts/autofill_scripts.h"
#include "scripts/safeguard_scripts.h"
#include "scripts/find_selection_scripts.h"
#include "scripts/console_redirect_scripts.h"
#include "scripts/page_tweaks_scripts.h"
#include <jsc/jsc.h>
#endif

#define READER_MODE_TRANSITION_DURATION_US 300000
#define EDIT_TEXT_ON_PAGE_WORLD "edit-text-on-page-world"
#define AUTOFILL_WORLD "autofill-world"
#define CONSOLE_REDIRECT_WORLD "console-redirect-world"

namespace {
    struct CppWrapper {
        orion::settings::Font prev_font_name;
        //NOTE: logic surrounding epoch: every page load (injection at end), pageShow event produces a UUID epoch message (check reader_epoch_scripts.cpp)
        //we attempt to eval javascript against that epoch that scrapes page for reader mode data, early exits if it ends up running against wrong epoch (check auto_reader.js, check_reader_mode())
        //we then send a message with epoch and readerData to handle_reader_data_received(), that checks against stale data by comparing against epoch known about in ui
        //preventing data from previously loaded site from overwriting data for next loaded site
        //we can never receive data newer than currently known epoch, if we end up running against new epoch because of settings change,
        //that operation gets discarded based on epoch mismatch and the newly received epoch forces a rerun
        std::string current_reader_epoch;

        uint find_total_matches = 0;
        uint find_current_match_index = 0;
        bool find_wrap_around = false;
    };
}

struct _OrionWebviewTabContent
{
	GtkWidget parent;

    //page content

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    //Contains OrionWebkitWebview
    WebKitWebView* content_webview;

    WebKitWebView* reader_mode_webview; //lazy initialized

    gboolean web_inspector_is_visible;
    gint64 animation_start_time;
    gboolean start_animation_on_next_frame;
    guint anim_cb_id;
    gboolean reader_mode_is_shown;
    float reader_mode_upwards_coverage;
    gboolean last_webview_page_was_readered;
    gboolean webview_is_reader_mode_enabled;

    WebKitUserScript* prev_reader_template_script;
    WebKitUserScript* prev_reader_init_script;
    GtkExpressionWatch* reader_template_script_watch;
    gboolean reader_mode_template_script_invalid;

    std::function<void(guint, gboolean)> find_result_callback;
    // webkit_find_controller_search_next/_previous also emit found-text/failed-to-find-text,
    // but with match_count hardcoded to 1 (WebKit only fully (re)counts matches on a plain
    // search() call, see FindController::updateFindUIAfterPageScroll). Set right before calling
    // search_next/search_previous so the handler can ignore that bogus count.
    gboolean find_suppress_next_result_callback;
    std::unordered_set<std::string> current_page_resource_domains;
#endif

    //logic data

    orion::TabId id;
    //These should only ever be used in webview_constructed
    std::optional<orion::TabDto> dto;
    std::optional<orion::Uri> uri;
    GIcon* icon;
    gboolean is_suspended;

    gboolean already_initialized;
    std::vector<orion::TabBackForwardListEntryDto> custom_uris_back_forward_list;
    //indexes go from 1 to back_forward_list_len, 0 is an unset state caused by tab creation
    int64_t current_back_forward_index; //gets incremented in load_uri when dealing with custom scheme, decremented in go back
    int64_t webview_current_back_forward_index; //reflects where in the custom back forward list the webview currently is
    int64_t back_forward_list_len; //changes when loading a new page (load_uri, load_changed)
    guint prev_webkit_forward_list_len;
    gboolean is_on_custom_widget; // is Start Page?
    gboolean is_readerable;
    gboolean next_load_finished_should_show_webview;
    gboolean show_favicons_inside_tabs;
    orion::ObserverId show_favicons_inside_tabs_observer;
    gboolean always_show_website_titles_in_tabs;
    orion::ObserverId always_show_website_titles_in_tabs_observer;
    orion::ObserverId settings_observer;
    std::string title;

    gboolean is_loading;
    gboolean is_playing_audio;
    gboolean is_muted;

    gboolean is_login_pending;
    std::string login_url;
    bool is_autofill_popover_showing;
    bool is_initial_load;
    bool reader_mode_enabled;

    std::vector<std::string> blocked_popup_urls;

    GtkWidget* current_widget;

    gint width;
    gint height;
    bool edit_text_on_page;

    bool element_picker_running;

    GtkExpressionWatch* reader_font_size_index_watch;
    GtkExpressionWatch* reader_font_name_watch;
    GtkExpressionWatch* reader_text_alignment_watch;
    GtkExpressionWatch* reader_background_type_watch;
    GtkExpressionWatch* reader_border_shadow_enabled_watch;
    GtkExpressionWatch* reader_document_width_watch;

    CppWrapper cpp_data;
};

enum {
    PROP_0,
    PROP_ID,
    PROP_TAB_TITLE,
    PROP_ICON,
    PROP_IS_LOADING,
    PROP_URI,
    PROP_CAN_GO_BACK,
    PROP_CAN_GO_FORWARD,
    PROP_BLOCKED_POPUPS_COUNT,
    PROP_IS_READERABLE,
    PROP_READER_MODE_ENABLED,
    PROP_IS_PLAYING_AUDIO,
    PROP_IS_MUTED,
    PROP_TLS_STATUS,
    PROP_BACK_LIST,
    PROP_FORWARD_LIST,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS] = {};

enum {
    SIG_ELEMENT_PICKER_PICK,
    SIG_ELEMENT_PICKER_FAIL,
    SIG_CURRENT_PAGE_RESOURCES_CHANGED,
    N_SIGS,
};

static guint obj_signals[N_SIGS] = {};

const bool show_console_redirect_messages = true;

G_DEFINE_TYPE(OrionWebviewTabContent, orion_webview_tab_content, GTK_TYPE_WIDGET)

using SnapshotCb = void(*)(GBytes* buffer, orion::gtk::WeakRefWrapper content_weak);

struct SnapshotData
{
    SnapshotCb cb{};
    orion::gtk::WeakRefWrapper content_weak;
};
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static void
orion_webview_tab_content_web_view_obtained(OrionWebviewTabContent* self, bool run_constructed = true);
static void orion_webview_tab_content_hide_reader_mode_webview(OrionWebviewTabContent* self);
static void orion_webview_tab_content_show_reader_mode_webview(OrionWebviewTabContent* self);
static void orion_webview_tab_content_check_reader_mode(OrionWebviewTabContent* self);
static void page_tweaks_element_blockers_val_cb(gpointer self);
#endif

static void notify_tls_status(OrionWebviewTabContent* self) {g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_TLS_STATUS]); }

static void
orion_webview_tab_content_set_enable_content_blockers(OrionWebviewTabContent* content, bool enable, bool force_reload = false)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview = ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
    if (!webview) return;
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(content)));
    if (!win) return;
    orion::log_verbose("orion_webview_tab_content_set_enable_content_blockers: enable=%d tab_id=%lld webview=%p",
        enable, content->id.value, webview
    );

    orion::gtk::WeakRefWrapper webview_weak(webview);
    auto& factory = main_window_get_current_webview_factory(win);
    orion::contentblock::reload_webview_filters_using_setting_value_with_cb(&factory, webview, enable,
        force_reload?[webview_weak](){
            auto webview_strong = webview_weak.lock();
            if(webview_strong)
            {
                webkit_web_view_reload_bypass_cache(WEBKIT_WEB_VIEW(webview_weak.get()));
            }
        }:std::function<void()>(nullptr));
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

static void
orion_webview_tab_content_set_enable_intelligent_tracking_prevention(OrionWebviewTabContent* content, bool enable)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview = ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
    if (!webview) return;
    WebKitNetworkSession* network_session = webkit_web_view_get_network_session(webview);
    if (!network_session) return;
    gboolean old_value = webkit_network_session_get_itp_enabled(network_session);
    orion::log_verbose(
        "orion_webview_tab_content_set_enable_intelligent_tracking_prevention: enabled:%d->%d network_session=%p",
        old_value, enable, network_session
    );
    webkit_network_session_set_itp_enabled(network_session, enable);
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

static void
orion_webview_tab_content_set_block_web_fonts(OrionWebviewTabContent* content, bool block)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview = ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
    if (!webview) return;
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(content)));
    if (!win) return;
    orion::log_verbose("orion_webview_tab_content_set_block_web_fonts: block=%d tab_id=%lld webview=%p",
        block, content->id.value, webview
    );

    auto& factory = main_window_get_current_webview_factory(win);
    orion::contentblock::reload_block_web_fonts_webview_filters_using_website_setting(&factory, webview);
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

static void
orion_webview_tab_content_set_enable_javascript(OrionWebviewTabContent* content, bool enable)
{
    #if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview = ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
    if (!webview) return;

    WebKitSettings* settings = webkit_web_view_get_settings(webview);
    gboolean old_value = webkit_settings_get_enable_javascript(settings);
    orion::log_verbose(
        "orion_webview_tab_content_set_enable_javascript: enabled:%d->%d",
        old_value, enable
    );
    webkit_settings_set_enable_javascript(settings, enable);
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

static void
orion_webview_tab_content_set_enable_cookies(OrionWebviewTabContent* content, bool enable)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview = ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
    if (!webview) return;
    WebKitNetworkSession* network_session = webkit_web_view_get_network_session(webview);
    if (!network_session) return;

    WebKitCookieAcceptPolicy new_policy = enable
        ? WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS
        : WEBKIT_COOKIE_POLICY_ACCEPT_NEVER;
    WebKitCookieManager* manager = webkit_network_session_get_cookie_manager(network_session);
    webkit_cookie_manager_set_accept_policy(manager, new_policy);
    orion::log_debug(
        "orion_webview_tab_content_set_enable_cookies: enable:%d",
        enable
    );

    if (enable) {
        return;
    }

    // Note that orion_webview_tab_content_get_uri may still return stale data here
    orion::Uri current_uri = orion::Uri::parse_user_input(webkit_web_view_get_uri(webview));
    std::string website = current_uri.authority();

    if (website.empty()) {
        // New Tab: nothing to do
        return;
    }

    struct AsyncData {
        std::string website;
    } *data = new AsyncData{
        .website = website,
    };

    // Delete all website cookies (asynchronously)
    webkit_cookie_manager_get_all_cookies(
        manager,
        NULL, // cancellable
        [](GObject* source, GAsyncResult* result, gpointer user_data){
            AsyncData* data = reinterpret_cast<AsyncData*>(user_data);
            defer([data]{ delete data; });

            WebKitCookieManager* manager = WEBKIT_COOKIE_MANAGER(source);
            GError* error = NULL;
            GList* cookies = webkit_cookie_manager_get_all_cookies_finish(manager, result, &error);
            if (error) {
                defer([error]{ g_error_free(error); });
                orion::log_warning(
                    "orion_webview_tab_content_set_enable_cookies: failed to get cookies for deletion: %s",
                    error->message
                );
                return;
            }
            defer([cookies]{
                g_list_free_full(cookies, [](gpointer cookie){
                    soup_cookie_free(reinterpret_cast<SoupCookie*>(cookie));
                });
            });

            for (GList* iter = cookies; iter != NULL; iter = iter->next) {
                SoupCookie* cookie = reinterpret_cast<SoupCookie*>(iter->data);
                const char* domain = soup_cookie_get_domain(cookie);
                // Skip cookies that do not match the current website
                if (!soup_cookie_domain_matches(cookie, data->website.c_str())) {
                    continue;
                }

                orion::log_verbose("orion_webview_tab_content_set_enable_cookies: deleting cookie '%s' for website '%s'",
                    soup_cookie_get_name(cookie), data->website.c_str()
                );

                webkit_cookie_manager_delete_cookie(
                    manager,
                    cookie,
                    NULL, // cancellable
                    [](GObject* source, GAsyncResult* result, gpointer user_data){
                        WebKitCookieManager* manager = WEBKIT_COOKIE_MANAGER(source);
                        GError* error = NULL;
                        webkit_cookie_manager_delete_cookie_finish(manager, result, &error);
                        if (error) {
                            defer([error]{ g_error_free(error); });
                            orion::log_warning(
                                "orion_webview_tab_content_set_enable_cookies: failed to delete cookie: %s",
                                error->message
                            );
                        }
                    },
                    NULL // user_data
                );
            }
        },
        data // user_data
    );
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

static void
evaluate_edit_text_on_page_script(OrionWebviewTabContent* self, bool enable)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    orion_webview_tab_content_evaluate_javascript(
        self,
        orion::scripts::get_edit_text_on_page_script(enable).c_str(),
        -1,
        EDIT_TEXT_ON_PAGE_WORLD,
        NULL, // source_uri
        NULL, // cancellable
        NULL, // callback
        NULL  // data
    );
#endif
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static void
webkit_web_view_snapshot_finished_cb(GObject* web_view, GAsyncResult* result, gpointer user_data)
{
    SnapshotData* data = static_cast<SnapshotData*>(user_data);
    defer([data]{ delete data; });

    WebKitWebView* self = WEBKIT_WEB_VIEW(web_view);

    GdkTexture* texture = NULL;
    {
        GError* error = NULL;
        // TODO: this does not work reliably at the moment, some tabs/webpages result in
        // "There was an error creating the snapshot" error. This could probably by worked
        // around by offscreen rendering using a realized (but not visible) GtkWindow (not
        // GtkOffscreenWindow because it is not part of GTK4 anymore).
        texture = webkit_web_view_get_snapshot_finish(self, result, &error);
        if (error) {
            defer([error]{ g_error_free(error); });
            orion::log_verbose("webkit_web_view_snapshot_finished_cb: failed to get snapshot: %s", error->message);
            data->cb(NULL, data->content_weak);
            return;
        }
    }
    defer([texture]{ g_object_unref(texture); });

    GBytes* bytes = gdk_texture_save_to_png_bytes(texture);
    data->cb(bytes, data->content_weak);
    if(!bytes) return;
    defer([bytes]{ g_bytes_unref(bytes); });

    orion::log_verbose("webkit_web_view_snapshot_finished_cb: snapshot size=%zu bytes", g_bytes_get_size(bytes));
}

static void
snapshot_cb(GBytes* buffer, orion::gtk::WeakRefWrapper content_weak)
{
    auto content_strong = content_weak.lock();
    if (!content_strong) return;
    auto content = ORION_WEBVIEW_TAB_CONTENT(content_strong.get());

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(content));
    if (!root) {
        orion::log_verbose("snapshot_cb: tab content not yet parented to a window, skipping");
        return;
    }
    MainWindow* main_window = MAIN_WINDOW(root);

    std::vector<uint8_t> snapshot_data{};
    if(buffer)
    {
        gsize size;
        gconstpointer data = g_bytes_get_data(buffer, &size);
        snapshot_data.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data)+size);
    }
    else {
        orion::log_verbose("snapshot_cb: no snapshot data received");
    }
    if(!snapshot_data.size())
    {
        return;
    }
    orion::UpdateTabQuery tab_query{};
    tab_query.id = content->id;
    tab_query.update_uri = true;
    tab_query.uri = orion_webview_tab_content_get_uri(content).uri().c_str();
    tab_query.update_name = true;
    tab_query.name = orion_webview_tab_content_get_title(content);
    tab_query.update_back_forward_list_index = true;
    tab_query.back_forward_list_index = content->current_back_forward_index;
    tab_query.update_webkit_back_forward_list_index = true;
    tab_query.webkit_back_forward_list_index = content->webview_current_back_forward_index;
    tab_query.update_merged_back_forward_list_length = content->back_forward_list_len;
    tab_query.merged_back_forward_list_length = content->back_forward_list_len;
    tab_query.update_snapshot_image = true;
    tab_query.snapshot_image = std::move(snapshot_data);

    WebKitWebViewSessionState* state = webkit_web_view_get_session_state(content->content_webview);
    GBytes* buf = webkit_web_view_session_state_serialize(state);
    gsize len;
    uint8_t* start = static_cast<uint8_t*>(g_bytes_unref_to_data(buf, &len));
    tab_query.update_session_state = true;
    tab_query.session_state.assign(start, start+len);
    g_free(start);


    orion::gtk::WeakRefWrapper main_window_weak(main_window);

    global_core_api->update_tab(tab_query, [main_window_weak](orion::Error err){
        VERIFY_LOG(err);

        auto main_window_strong = main_window_weak.lock();
        if (!main_window_strong) return;
        MainWindow* main_window = MAIN_WINDOW(main_window_strong.get());
        
        // Refresh bookmarked state of the current web page
        main_window_refresh_bookmark_icon(main_window);
    });


    // NOTE: This could technically leave the tab in an inconsistent state if the application tries to exit
    // during as no new actions can be queued by main thread
    // it would probably be best to either save back forward list incrementally or implement backend functionality
    // that also saves back forward list entries in the same transaction as the tab update

    orion::DeleteTabBackForwardListEntriesQuery query{};
    query.start_webkit_index = 0;
    query.tab_id = content->id;
    
    global_core_api->delete_tab_back_forward_list_entries(query,
    [back_forward_list=content->custom_uris_back_forward_list](orion::Error err){
        VERIFY_LOG(err);

        for(orion::TabBackForwardListEntryDto entry : back_forward_list)
        {
            orion::CreateTabBackForwardListEntryQuery query{};
            query.tab_id = entry.tab_id;
            query.uri = entry.uri;
            query.webkit_index = entry.webkit_index;
            global_core_api->create_tab_back_forward_list_entry(query,
            [](orion::CreateTabBackForwardListEntryReply reply){
                VERIFY_LOG(reply.err);
            });
        }
    });
}

static cairo_status_t
png_write_func(void* closure, const unsigned char* data, unsigned int length)
{
    GByteArray* byte_array = (GByteArray*)closure;

    g_byte_array_append(byte_array, data, length);
    return CAIRO_STATUS_SUCCESS;
}

static void
on_widget_paintable_invalidate_contents(GdkPaintable* paintable, gpointer data)
{
    GtkSnapshot* snapshot = gtk_snapshot_new();
    defer([snapshot]{ g_object_unref(snapshot); });
    defer([paintable]{ g_object_unref(paintable); }); //TODO: properly check whether this is dangerous or not, probably is
    int width = gdk_paintable_get_intrinsic_width(paintable);
    int height = gdk_paintable_get_intrinsic_height(paintable);
    gdk_paintable_snapshot(paintable, snapshot, width, height);

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* context = cairo_create(surface);
    defer([context]{ cairo_destroy(context); });
    defer([surface]{ cairo_surface_destroy(surface); });
    
    GskRenderNode* node = gtk_snapshot_to_node(snapshot);
    if(!node) return;
    defer([node]{ gsk_render_node_unref(node); });

    gsk_render_node_draw(node, context);
    GByteArray* byte_array = g_byte_array_new();
    if(cairo_surface_write_to_png_stream(surface, png_write_func, byte_array))
    {
        g_byte_array_free(byte_array, true);
        return;
    }
    GBytes* image_data = g_byte_array_free_to_bytes(byte_array);
    defer([image_data]{ g_bytes_unref(image_data); });
    snapshot_cb(image_data, *static_cast<orion::gtk::WeakRefWrapper*>(data));
    g_signal_handlers_disconnect_by_data(paintable, data);
}

static void
orion_webview_tab_content_save_state(OrionWebviewTabContent* content)
{
    orion::log_verbose("async orion_webview_tab_content_save_state: tab_id=%lld", content->id.value);
    

    if(content->is_on_custom_widget)
    {
        GdkPaintable* paintable = gtk_widget_paintable_new(GTK_WIDGET(content));
        orion::gtk::WeakRefWrapper* content_weak_ref_ptr = new orion::gtk::WeakRefWrapper(content);
        auto weak_ref_destroy = [](gpointer data, GClosure* closure){
            delete static_cast<orion::gtk::WeakRefWrapper*>(data);
        };
        g_signal_connect_data(paintable, "invalidate-contents", G_CALLBACK(on_widget_paintable_invalidate_contents), content_weak_ref_ptr, weak_ref_destroy, G_CONNECT_DEFAULT);
    }
    else
    {
        SnapshotData* snapshot_data = new SnapshotData{
            .cb = snapshot_cb,
            .content_weak = orion::gtk::WeakRefWrapper(content),
        };

        webkit_web_view_get_snapshot(
            content->content_webview,
            WebKitSnapshotRegion::WEBKIT_SNAPSHOT_REGION_VISIBLE,
            // note that WebKitSnapshotOptions::WEBKIT_SNAPSHOT_OPTIONS_TRANSPARENT_BACKGROUND can be more reliable
            // than WebKitSnapshotOptions::WEBKIT_SNAPSHOT_OPTIONS_NONE
            WebKitSnapshotOptions::WEBKIT_SNAPSHOT_OPTIONS_NONE,
            NULL, // cancellable
            webkit_web_view_snapshot_finished_cb,
            (gpointer)snapshot_data // user_data
        );
    }
}

//takes a new ref of icon
static void
orion_webview_tab_content_set_tab_favicon(OrionWebviewTabContent* self, GIcon* icon)
{
    if(self->icon == icon) return;
    
    if(self->icon)
    {
        g_clear_object(&self->icon);
    }
    if (icon) {
        g_object_ref(icon);
    }
    self->icon = icon;

    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_ICON]);
}
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

static void
orion_webview_tab_content_set_tab_title_internal(OrionWebviewTabContent* self, const char* title)
{
    if(!title) return;
    orion::log_verbose("orion_webview_tab_content_set_tab_title_internal: tab_id=%lld title=%s",
        self->id.value, title
    );

    self->title = title;
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_TAB_TITLE]);
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
// world_name can be nullptr
static void
inject_script(
    WebKitWebView* webview,
    WebKitUserScriptInjectionTime injection_time,
    const std::string& source,
    const std::string& world_name)
{
    if (world_name.empty()) {
        orion::log_warning("inject_script: world_name is empty, script will be injected into the default world, was this intended?");
    }
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(webview);
    WebKitUserScript* user_script = webkit_user_script_new_for_world(
        source.c_str(),
        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
        injection_time,
        world_name.c_str(),
        NULL, // allow list
        NULL  // block list
    );
    webkit_user_content_manager_add_script(mgr, user_script);
}
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT



static void
orion_webview_tab_reload_on_default_autoplay_setting_change(OrionWebviewTabContent* self, const std::string& website)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper self_weak(self);
    global_core_api->get_setting(profile_id, WEBSITE_SETTING(autoplay_default_setting, website),
    [self_weak](orion::GetSettingReply<orion::settings::AutoplayWebsiteSetting> reply) {
        VERIFY_LOG(reply.err);
        if (reply.value != orion::settings::AutoplayWebsiteSetting::not_set) return;
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(self_strong.get());
        orion_webview_tab_content_reload(self);
    });
}

static void
orion_webview_tab_reload_on_default_user_agent_setting_change(OrionWebviewTabContent* self, const std::string& website)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper self_weak(self);
    global_core_api->get_setting(profile_id,
        WEBSITE_SETTING(user_agent, website),
        [self_weak](orion::GetSettingReply<orion::settings::UserAgentWebsiteSetting> reply) {
            VERIFY_LOG(reply.err);
            if (reply.value != orion::settings::UserAgentWebsiteSetting::automatic) return;
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(self_strong.get());
            orion_webview_tab_content_reload(self);
        });
}

static void
orion_webview_tab_reload_on_custom_user_agent_setting_change(OrionWebviewTabContent* self, const std::string& website)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper self_weak(self);
    global_core_api->get_setting_or_default(profile_id,
        WEBSITE_SETTING(user_agent, website),
        orion::settings::websites::user_agent_default_setting_for_all_websites,
        [self_weak](orion::GetSettingReply<orion::settings::UserAgentWebsiteSetting> reply) {
            VERIFY_LOG(reply.err);
            if (reply.value != orion::settings::UserAgentWebsiteSetting::custom) return;
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(self_strong.get());
            orion_webview_tab_content_reload(self);
        });
}
    


//region Methods to swap between start widget and webview

// this terminates the underlying webkit web process
void
orion_webview_tab_content_suspend_webview(OrionWebviewTabContent* content)
{
    orion::log_debug("orion_webview_tab_content_suspend_webview");
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    g_return_if_fail(ORION_IS_WEBVIEW_TAB_CONTENT(content));
    if(content->content_webview)
    {
        // forcefully terminate the web process immediately
        webkit_web_view_terminate_web_process(content->content_webview);
        //g_clear_object(&content->content_webview);
    }
#endif
}

void
orion_webview_tab_content_set_is_suspended(OrionWebviewTabContent* content, bool is_suspended)
{
    if(content->is_suspended != is_suspended)
    {
        content->is_suspended = is_suspended;
        g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_ICON]);
    }
}

gboolean
orion_webview_tab_content_get_is_suspended(OrionWebviewTabContent* content)
{
    return content->is_suspended;
}

gboolean
orion_webview_tab_content_get_is_readerable(OrionWebviewTabContent* content)
{
    return content->is_readerable;
}

gboolean
orion_webview_tab_content_get_reader_mode_enabled(OrionWebviewTabContent* content)
{
    return content->reader_mode_enabled;
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static void
orion_webview_tab_content_show_webview(OrionWebviewTabContent* content)
{
    orion::log_verbose("orion_webview_tab_content_show_webview: tab_id=%lld", content->id.value);
    g_return_if_fail(ORION_IS_WEBVIEW_TAB_CONTENT(content));
    g_clear_pointer(&content->current_widget, gtk_widget_unparent);
    if(content->is_on_custom_widget)
    {
        content->is_on_custom_widget = false;
        orion_webview_tab_content_set_tab_favicon(content, G_ICON(webkit_web_view_get_favicon(content->content_webview)));

        g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_IS_READERABLE]);
        gtk_widget_queue_resize(GTK_WIDGET(content));
        const char* title = webkit_web_view_get_title(content->content_webview);

        orion_webview_tab_content_set_tab_title_internal(content, title);

        GdkTexture* texture = webkit_web_view_get_favicon(WEBKIT_WEB_VIEW(content->content_webview));
        if(texture)
        {
            orion_webview_tab_content_set_tab_favicon(content, G_ICON(texture));
        }

        content->current_back_forward_index = content->webview_current_back_forward_index;
        g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_BACK]);
        g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_FORWARD]);
        g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_BACK_LIST]);
        g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_FORWARD_LIST]);
        
        orion_webview_tab_content_save_state(content);
        g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_URI]);
        notify_tls_status(content);

        if(content->webview_is_reader_mode_enabled)
        {
            content->reader_mode_enabled = true;
            g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_READER_MODE_ENABLED]);
        }

        if(content->current_widget)
        {
            //shouldn't really happen but leaving this here for a little code cleanup
            gtk_widget_set_visible(content->current_widget, false);
        }
        gtk_widget_set_visible(GTK_WIDGET(content->content_webview), true);
    }
}

//requires tab to have an up to date back forward list and corresponding indexes
static void
orion_webview_tab_content_show_widget(OrionWebviewTabContent* content, GtkWidget* widget)
{
    g_clear_pointer(&content->current_widget, gtk_widget_unparent);
    content->current_widget = widget;
    //will inherently cause a rerun of the layout engine
    gtk_widget_set_parent(widget, GTK_WIDGET(content));

    if(!content->is_on_custom_widget)
    {
        if(content->content_webview)
        {
            webkit_web_view_terminate_web_process(content->content_webview);
            gtk_widget_set_visible(GTK_WIDGET(content->content_webview), false);
        }
        orion_webview_tab_content_hide_reader_mode_webview(content);
    }
    
    content->is_on_custom_widget = true;

    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_IS_READERABLE]);
    orion_webview_tab_content_save_state(content);


    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_BACK]);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_FORWARD]);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_BACK_LIST]);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_FORWARD_LIST]);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_URI]);
    content->reader_mode_enabled = false;
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_READER_MODE_ENABLED]);
    notify_tls_status(content);

    gtk_widget_set_visible(content->current_widget, true);
}
#endif

//endregion



//region Navigation methods

gboolean
orion_webview_tab_content_can_go_back(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    g_return_val_if_fail(ORION_IS_WEBVIEW_TAB_CONTENT(content), false);
    //we can always go back to at least the start widget, unless we are on it
    return content->current_back_forward_index>1;
#else
    return true;
#endif
}

gboolean
orion_webview_tab_content_can_go_forward(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    g_return_val_if_fail(ORION_IS_WEBVIEW_TAB_CONTENT(content), false);
    return content->current_back_forward_index < (content->back_forward_list_len);
#else
    return true;
#endif
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static GList*
get_webkit_fb_glist(OrionWebviewTabContent* content)
{
    if(!content->content_webview) return NULL;

    GList* webkit_fb_list = NULL;
    WebKitBackForwardList* wk_webview_bf_list = webkit_web_view_get_back_forward_list(content->content_webview);

    GList* back_list = webkit_back_forward_list_get_back_list(wk_webview_bf_list); //ordered from highest index to lowest
    
    WebKitBackForwardListItem* current_item = webkit_back_forward_list_get_current_item(wk_webview_bf_list);
    webkit_fb_list = current_item ? g_list_prepend(back_list, current_item) : back_list; //this is the high index end of the list

    GList* forward_list = webkit_back_forward_list_get_forward_list(wk_webview_bf_list); //fun fact, this is ordered newest first
    
    webkit_fb_list = g_list_concat(forward_list, webkit_fb_list); //automatically null checks

    //we mask items that shouldn't be in the bf list, because a custom widget load should've cut them from the list
    guint webkit_fb_list_len = g_list_length(webkit_fb_list);

    //sanity check which should never happen
    if((int64_t)(webkit_fb_list_len + content->custom_uris_back_forward_list.size()) < content->back_forward_list_len)
    {
        orion::log_error("attempted to get webkit fb list in an inconsistent tab state");
        g_list_free(webkit_fb_list);
        return NULL;
    }

    guint n_items_to_cut = webkit_fb_list_len + content->custom_uris_back_forward_list.size() - content->back_forward_list_len;

    if(n_items_to_cut)
    {
        GList* old_head = webkit_fb_list;
        webkit_fb_list = g_list_nth(old_head, n_items_to_cut);
        
        if(webkit_fb_list)
        {
            //this check should be strictly unnecessary, webkit_fb_list should be pointing to a valid element of index >= 1
            //just looks healthier
            if(webkit_fb_list->prev)
            {
                webkit_fb_list->prev->next = NULL;
            }
            webkit_fb_list->prev = NULL;
            
        }

        g_list_free(old_head);
    }

    return webkit_fb_list;
}
#endif

//newest (closest to current index) first
orion::BackForwardList*
orion_webview_tab_content_get_back_list(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    GList* webkit_bf_list = g_list_reverse(get_webkit_fb_glist(content));

    //captures the original state, meaning we are free to modify our local webkit_bf_list variable after this while still ensuring correct freeing
    defer([webkit_bf_list]{ g_list_free(webkit_bf_list); });

    
    //walks from the start of custom uris list (lowest index first) as long as the item's index is less or eq to current index,
    //fills back_list with items from webkit_bf_list that are between last custom uri item and currently checked one
    //inserts this custom uri if it should also be a part of the back list
    //after last strictly smaller is found, inserts the rest from webkit_bf_list
    GList* back_list = NULL;
    int64_t searched_wrapped_index = 0;
    for(auto& item : content->custom_uris_back_forward_list)
    {
        if(item.webkit_index <= content->current_back_forward_index)
        {
            for(int64_t i = searched_wrapped_index + 1; i < item.webkit_index; i++)
            {
                if(webkit_bf_list)
                {
                    auto* webkit_item = static_cast<WebKitBackForwardListItem*>(webkit_bf_list->data);
                    const gchar* title = webkit_back_forward_list_item_get_title(webkit_item);
                    back_list = g_list_prepend(back_list, new orion::BackForwardListEntry{
                        .uri = webkit_back_forward_list_item_get_uri(webkit_item),
                        .title = title?title:""
                    });
                    webkit_bf_list = webkit_bf_list->next;
                }
                else
                {
                    orion::log_debug("orion_webview_tab_content_get_back_list():");
                    orion::log_error("couldn't find enough webkit back forward list entries to fill back_list");
                    break;
                }
            }
            if(item.webkit_index < content->current_back_forward_index)
            {
                back_list = g_list_prepend(back_list, new orion::BackForwardListEntry{
                    .uri = item.uri,
                    .title = orion::custom_uri_get_corresponding_title(item.uri)
                });
            }
            searched_wrapped_index = item.webkit_index;
        }
        else
        {
            break;
        }
    }

    for(int64_t i = searched_wrapped_index + 1; i<content->current_back_forward_index; i++)
    {
        if(webkit_bf_list)
        {
            auto* webkit_item = static_cast<WebKitBackForwardListItem*>(webkit_bf_list->data);
            const gchar* title = webkit_back_forward_list_item_get_title(webkit_item);
            back_list = g_list_prepend(back_list, new orion::BackForwardListEntry{
                .uri = webkit_back_forward_list_item_get_uri(webkit_item),
                .title = title?title:""
            });
            webkit_bf_list = webkit_bf_list->next;
        }
        else
        {
            orion::log_debug("orion_webview_tab_content_get_back_list():");
            orion::log_error("couldn't find enough webkit back forward list entries to fill back_list");
            break;
        }
    }



    return back_list;
#else
    return NULL;
#endif
}

//oldest (closest to current index) first
orion::BackForwardList*
orion_webview_tab_content_get_forward_list(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    //could be made more efficient by adapting the code from get_webkit_bf_glist but working through it in reverse order
    GList* webkit_fb_list = get_webkit_fb_glist(content);

    //captures the original state, meaning we are free to modify our local webkit_fb_list variable after this while still ensuring correct freeing
    defer([webkit_fb_list]{ g_list_free(webkit_fb_list); });

    
    //walks from the end of custom uris list (highest index first) as long as the item's index is higher or eq to current index,
    //fills forward_list with items from webkit_fb_list that are between last custom uri item and currently checked one
    //inserts this custom uri if it should also be a part of the forward list
    //after last strictly smaller is found, inserts the rest from webkit_fb_list
    
    int64_t webkit_fb_list_len = int64_t(g_list_length(webkit_fb_list));
    
    //NOTE: int64_t indexing should be safe, the user would run out of memory way before this becomes an issue,
    //overflow would mean we've consumed on the order of 10s of exabytes of ram

    GList* forward_list = NULL;
    int64_t searched_wrapped_index = content->back_forward_list_len + 1;

    for(auto custom_rit = content->custom_uris_back_forward_list.rbegin(); custom_rit != content->custom_uris_back_forward_list.rend(); ++custom_rit)
    {
        if(custom_rit->webkit_index >= content->current_back_forward_index)
        {
            for(int64_t i = searched_wrapped_index - 1; i > custom_rit->webkit_index; i--)
            {
                if(webkit_fb_list)
                {
                    auto* webkit_item = static_cast<WebKitBackForwardListItem*>(webkit_fb_list->data);
                    const gchar* title = webkit_back_forward_list_item_get_title(webkit_item);
                    forward_list = g_list_prepend(forward_list, new orion::BackForwardListEntry{
                        .uri = webkit_back_forward_list_item_get_uri(webkit_item),
                        .title = title?title:""
                    });
                    webkit_fb_list = webkit_fb_list->next;
                }
                else
                {
                    orion::log_debug("orion_webview_tab_content_get_forward_list():");
                    orion::log_error("couldn't find enough webkit back forward list entries to fill forward_list");
                    break;
                }
            }
            if(custom_rit->webkit_index > content->current_back_forward_index)
            {
                forward_list = g_list_prepend(forward_list, new orion::BackForwardListEntry{
                    .uri = custom_rit->uri,
                    .title = orion::custom_uri_get_corresponding_title(custom_rit->uri)
                });
            }
            if(custom_rit->webkit_index >= searched_wrapped_index)
            {
                orion::log_debug("orion_webview_tab_content_get_forward_list():");
                orion::log_error("custom uris back forward list is misordered/misindexed");
                return forward_list;
            }
            searched_wrapped_index = custom_rit->webkit_index;
        }
        else
        {
            break;
        }
    }

    for(int64_t i = searched_wrapped_index - 1; i > content->current_back_forward_index; i--)
    {
        if(webkit_fb_list)
        {
            auto* webkit_item = static_cast<WebKitBackForwardListItem*>(webkit_fb_list->data);
            const gchar* title = webkit_back_forward_list_item_get_title(webkit_item);
            forward_list = g_list_prepend(forward_list, new orion::BackForwardListEntry{
                .uri = webkit_back_forward_list_item_get_uri(webkit_item),
                .title = title?title:""
            });
            webkit_fb_list = webkit_fb_list->next;
        }
        else
        {
            orion::log_debug("orion_webview_tab_content_get_forward_list():");
            orion::log_error("couldn't find enough webkit back forward list entries to fill forward_list");
            break;
        }
    }



    return forward_list;
#else
    return NULL;
#endif
}

void
orion_webview_tab_content_navigate_to_relative_bf_list_index(OrionWebviewTabContent* content, int64_t relative_index)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitBackForwardList* wk_webview_bf_list = webkit_web_view_get_back_forward_list(content->content_webview);
    int64_t new_index = content->current_back_forward_index + relative_index;
    int64_t max_index = content->back_forward_list_len;
    if(!(1 <= new_index && new_index <= max_index))
    {
        orion::log_debug("orion_webview_tab_content_navigate_to_relative_bf_list_index():");
        orion::log_error("invalid relative back forward list index");
        return;
    }

    auto custom_back_forward_list_it = std::find_if(
        content->custom_uris_back_forward_list.cbegin(),
        content->custom_uris_back_forward_list.cend(),
        [new_index](orion::TabBackForwardListEntryDto a){return a.webkit_index==new_index;});

    if(custom_back_forward_list_it != content->custom_uris_back_forward_list.cend())
    {
        std::string uri = custom_back_forward_list_it->uri;

        std::string title;
        GtkWidget* widget = NULL;
        GIcon* icon = NULL;

        if(orion::custom_uri_get_corresponding_widget(uri, &widget, &title, &icon))
        {
            content->current_back_forward_index = new_index;
            orion_webview_tab_content_show_widget(content, widget);
            orion_webview_tab_content_set_tab_title_internal(content, title.c_str());
            if(icon)
            {
                defer([&icon]{ g_clear_object(&icon); });
                orion_webview_tab_content_set_tab_favicon(content, icon);
            }
        }
        else
        {
            orion::log_debug("orion_webview_tab_content_navigate_to_relative_bf_list_index():");
            orion::log_error("failed to parse %s from back forward list as a custom uri", uri.c_str());
            //non working custom uris should never appear in the custom back forward list, this ends up being a noop
            return;
        }
    }
    else
    {
        if(content->webview_current_back_forward_index == new_index)
        {
            if(content->last_webview_page_was_readered) orion_webview_tab_content_show_reader_mode_webview(content);
            webkit_web_view_reload(content->content_webview);
            orion_webview_tab_content_show_webview(content);
        }
        else
        {
            GList* webkit_bf_list = g_list_reverse(get_webkit_fb_glist(content));
            defer([webkit_bf_list]{ g_list_free(webkit_bf_list); });
            
            //this index also starts from 1 for the first item in the GList
            int64_t webkit_bf_list_index = new_index - std::count_if(
                content->custom_uris_back_forward_list.cbegin(),
                content->custom_uris_back_forward_list.cend(),
                [new_index](orion::TabBackForwardListEntryDto a){ return a.webkit_index<new_index; }); //should never equal based on our prior search
            
            WebKitBackForwardListItem* item = WEBKIT_BACK_FORWARD_LIST_ITEM(g_list_nth_data(webkit_bf_list, webkit_bf_list_index - 1));
            if(!item)
            {
                orion::log_debug("orion_webview_tab_content_navigate_to_relative_bf_list_index():");
                orion::log_error("failed to find new index in webkits back forward list");
                return;
            }
            else
            {
                if(content->is_on_custom_widget)
                {
                    content->next_load_finished_should_show_webview = true;
                    //empyrically found as necessary, web process logic doesn't initialise properly without this forced reload, no clue, why exactly
                    webkit_web_view_reload(content->content_webview);
                }
                //orion_webkit_webview_ignore_next_download(ORION_WEBKIT_WEBVIEW(content->content_webview));
                webkit_web_view_go_to_back_forward_list_item(content->content_webview, item);
            }
        }
    }
#endif
}

void
orion_webview_tab_content_go_back(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(orion_webview_tab_content_can_go_back(content))
    {
        orion_webview_tab_content_navigate_to_relative_bf_list_index(content, -1);
    }
    else
    {
        orion::log_error("Webview tab page go back called but can't go back for page 0x%lx", content);
    }
#endif
}

void
orion_webview_tab_content_go_forward(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    g_return_if_fail(ORION_IS_WEBVIEW_TAB_CONTENT(content));
    if(orion_webview_tab_content_can_go_forward(content))
    {
        orion_webview_tab_content_navigate_to_relative_bf_list_index(content, 1);
    }
    else
    {
        orion::log_error("Webview tab page go forward called but can't go forward for page 0x%lx", content);
    }
#endif
}

void
orion_webview_tab_content_reload(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    g_return_if_fail(ORION_IS_WEBVIEW_TAB_CONTENT(content));
    if(content->is_on_custom_widget)
    {
        auto it = std::find_if(
            content->custom_uris_back_forward_list.cbegin(),
            content->custom_uris_back_forward_list.cend(),
            [index = content->current_back_forward_index](orion::TabBackForwardListEntryDto a){return a.webkit_index == index;});

        if(it == content->custom_uris_back_forward_list.cend())
        {
            orion::log_debug("error: tab content is_on_custom_widget set but coresponding uri not found in its custom_uris_back_forward_list");
            return;
        }

        //TODO: possibly reload page title, icon as well
        std::string uri = it->uri;

        std::string title;
        GtkWidget* widget = NULL;
        GIcon* icon = NULL;

        if(orion::custom_uri_get_corresponding_widget(uri, &widget, &title, &icon))
        {
            orion_webview_tab_content_show_widget(content, widget);
            orion_webview_tab_content_set_tab_title_internal(content, title.c_str());
            if(icon)
            {
                defer([&icon]{ g_clear_object(&icon); });
                orion_webview_tab_content_set_tab_favicon(content, icon);
            }
        }
        else
        {
            orion::log_error("failed to parse %s from back forward list as a custom uri", uri.c_str());
            //non working custom uris should never appear in the custom back forward list
            return;
        }
    }
    else
    {
        webkit_web_view_reload(content->content_webview);
    }
#endif
}

void
orion_webview_tab_content_close(OrionWebviewTabContent* content, std::function<void(orion::Error)> done_cb)
{
    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(content)));
    g_assert(main_window);
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);
    orion::WindowId window_id = main_window_get_window_id(main_window);
    orion::gtk::main_window::close_tab_tracking_recent(content->id, profile_id, window_id, std::move(done_cb));
}

static int64_t
page_zoom_to_zoom_percent(orion::settings::PageZoom zoom)
{
    switch (zoom) {
        case orion::settings::PageZoom::zoom_50:
            return 50;
        case orion::settings::PageZoom::zoom_75:
            return 75;
        case orion::settings::PageZoom::zoom_80:
            return 80;
        case orion::settings::PageZoom::zoom_85:
            return 85;
        case orion::settings::PageZoom::zoom_90:
            return 90;
        case orion::settings::PageZoom::zoom_95:
            return 95;
        case orion::settings::PageZoom::zoom_100:
            return 100;
        case orion::settings::PageZoom::zoom_115:
            return 115;
        case orion::settings::PageZoom::zoom_125:
            return 125;
        case orion::settings::PageZoom::zoom_150:
            return 150;
        case orion::settings::PageZoom::zoom_175:
            return 175;
        case orion::settings::PageZoom::zoom_200:
            return 200;
        case orion::settings::PageZoom::zoom_250:
            return 250;
        case orion::settings::PageZoom::zoom_300:
            return 300;
        default:
            orion::log_warning("page_zoom_to_zoom_percent: unknown PageZoom %d, defaulting to %d, zoom percent %d",
                static_cast<int>(zoom), static_cast<int>(orion::settings::PageZoom::zoom_100), 100
            );
            return 100;
    }
}

void
orion_webview_tab_content_change_zoom(OrionWebviewTabContent* self, int64_t delta, bool reset)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::Uri current_uri = orion_webview_tab_content_get_uri(self);
    std::string website = current_uri.authority();
    orion::gtk::WeakRefWrapper self_weak(self);

    if (reset) {
        auto setting = WEBSITE_SETTING(page_zoom, website);
        orion::SetSettingQuery query{
            .profile_id = profile_id,
            .key = setting.key(),
            .realm = setting.realm(),
            .key_name = setting.key_name(),
            .type = setting.type(),
            .value = setting.default_value_as_string(),
            .default_value = setting.default_value_as_string(),
        };

        global_core_api->set_setting_raw(query,
        [self_weak,website](orion::SetSettingReply<std::string> reply){
            VERIFY_LOG(reply.err);
            if (reply.err) return;

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(self_strong.get());

            orion::log_verbose("orion_webview_tab_content_change_zoom: reset website=%s", website.c_str());
            orion_webview_tab_content_sync_webview_zoom_level(self);
        });
        return;
    }

    global_core_api->get_setting_or_default(
        profile_id,
        WEBSITE_SETTING(page_zoom, website),
        orion::settings::websites::page_zoom_default_setting_for_all_websites,
        [self_weak,profile_id,website,delta](orion::GetSettingReply<orion::settings::PageZoom> reply){
        VERIFY_LOG(reply.err);
        
        orion::settings::PageZoom zoom = reply.value;
        int64_t val = static_cast<int64_t>(zoom) + delta;
        if (val < static_cast<int64_t>(orion::settings::PageZoom::zoom_50)) {
            val = static_cast<int64_t>(orion::settings::PageZoom::zoom_50);
        }
        if (val > static_cast<int64_t>(orion::settings::PageZoom::zoom_300)) {
            val = static_cast<int64_t>(orion::settings::PageZoom::zoom_300);
        }
        zoom = static_cast<orion::settings::PageZoom>(val);

        global_core_api->set_setting(profile_id, WEBSITE_SETTING(page_zoom, website), zoom,
        [self_weak,zoom,website](orion::SetSettingReply<orion::settings::PageZoom> reply){
            VERIFY_LOG(reply.err);

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(self_strong.get());

            orion::log_verbose("orion_webview_tab_content_change_zoom: website=%s zoom_percent=%d",
                website.c_str(), page_zoom_to_zoom_percent(zoom)
            );
            orion_webview_tab_content_set_zoom(self, zoom);
        });
    });
}

void
orion_webview_tab_content_zoom_in(OrionWebviewTabContent* self)
{
    orion_webview_tab_content_change_zoom(self, 1, false);
}

void
orion_webview_tab_content_zoom_out(OrionWebviewTabContent* self)
{
    orion_webview_tab_content_change_zoom(self, -1, false);
}

void
orion_webview_tab_content_reset_zoom(OrionWebviewTabContent* self)
{
    orion_webview_tab_content_change_zoom(self, 0, true);
}

void
orion_webview_tab_content_sync_webview_zoom_level(OrionWebviewTabContent* self)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    // Note that orion_webview_tab_content_get_uri may still return stale data here
    orion::Uri current_uri = orion::Uri::parse_user_input(webkit_web_view_get_uri(self->content_webview));
    std::string website = current_uri.authority();
    orion::gtk::WeakRefWrapper self_weak(self);

    global_core_api->get_setting_or_default(
        profile_id,
        WEBSITE_SETTING(page_zoom, website),
        orion::settings::websites::page_zoom_default_setting_for_all_websites,
        [self_weak,website](orion::GetSettingReply<orion::settings::PageZoom> reply){
        VERIFY_LOG(reply.err);
        orion::settings::PageZoom zoom = reply.value;

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(self_strong.get());

        int64_t zoom_percent = page_zoom_to_zoom_percent(zoom);
        orion::log_verbose("orion_webview_tab_content_sync_webview_zoom_level: website=%s zoom_percent=%d",
            website.c_str(), zoom_percent
        );
        orion_webview_tab_content_set_zoom(self, zoom);
    });
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}
void
orion_webview_tab_content_evaluate_javascript(OrionWebviewTabContent* self, const char* script, gssize length,
    const char* world_name, const char* source_uri, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer data)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(!self->is_on_custom_widget && self->content_webview)
    {
        webkit_web_view_evaluate_javascript(self->content_webview, script, length, world_name, source_uri, cancellable, callback, data);
    }
#endif
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
struct GetSelectedTextData {
    std::function<void(std::string)> done_cb;
};

static void
on_get_selected_text_finish(GObject* source_object, GAsyncResult* res, gpointer user_data)
{
    auto data = static_cast<GetSelectedTextData*>(user_data);
    defer([data]{ delete data; });

    WebKitWebView* webview = WEBKIT_WEB_VIEW(source_object);

    GError* error = NULL;
    JSCValue* value = webkit_web_view_evaluate_javascript_finish(webview, res, &error);
    if (error) {
        defer([error]{ g_error_free(error); });
        orion::log_error("on_get_selected_text_finish: failed to run javascript: %s", error->message);
        data->done_cb("");
        return;
    }

    if (!jsc_value_is_string(value)) {
        data->done_cb("");
        return;
    }

    char* text = jsc_value_to_string(value);
    defer([text]{ g_free(text); });

    data->done_cb(std::string(text));
}
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

void
orion_webview_tab_content_get_selected_text(OrionWebviewTabContent* content, std::function<void(std::string)> done_cb)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(content);
    if (self->is_on_custom_widget || !self->content_webview) {
        done_cb("");
        return;
    }

    GetSelectedTextData* data = new GetSelectedTextData{ .done_cb = done_cb };
    std::string script = orion::gtk::get_selected_text_script();

    webkit_web_view_evaluate_javascript(
        self->content_webview, script.c_str(), -1, NULL, NULL, NULL,
        on_get_selected_text_finish, data
    );
#else
    done_cb("");
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

void
orion_webview_tab_content_scroll_to_selection(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(content);
    if (!self->is_on_custom_widget && self->content_webview) {
        std::string script = orion::gtk::get_scroll_to_selection_script();
        webkit_web_view_evaluate_javascript(self->content_webview, script.c_str(), -1, NULL, NULL, NULL, NULL, NULL);
    }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

static void
orion_webview_tab_content_sync_website_setting_bool(
    OrionWebviewTabContent* self,
    std::string setting_name,
    std::function<void(OrionWebviewTabContent*, bool)> apply_setting_func)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    // Note that orion_webview_tab_content_get_uri may still return stale data here
    orion::Uri current_uri = orion::Uri::parse_user_input(webkit_web_view_get_uri(self->content_webview));
    std::string website = current_uri.authority();
    orion::gtk::WeakRefWrapper self_weak(self);

    orion::GetSettingForWebsiteOrDefaultQuery query{
        .profile_id = profile_id,
        .website = website,
        .key_name = setting_name,
    };

    global_core_api->get_setting_for_website_or_default(query,
    [self_weak,website,apply=std::move(apply_setting_func),setting_name]
    (orion::GetSettingForWebsiteOrDefaultReply reply){
        VERIFY_LOG(reply.err);
        bool enabled{};
        VERIFY_LOG(orion::settings::unmarshal_value(reply.value, &enabled));

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(self_strong.get());

        orion::log_verbose(
            "orion_webview_tab_content_sync_website_setting_bool: setting=%s website=%s enabled=%d",
            setting_name.c_str(), website.c_str(), enabled
        );
        apply(self, enabled);
    });
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

static void
orion_webview_tab_content_sync_content_blockers(OrionWebviewTabContent* self)
{
    orion_webview_tab_content_sync_website_setting_bool(
        self,
        orion::settings::website::specific::enable_content_blockers.key_name(),
        [](OrionWebviewTabContent* self, bool enable){ orion_webview_tab_content_set_enable_content_blockers(self, enable); }
    );
}

static void
orion_webview_tab_content_sync_intelligent_tracking_prevention(OrionWebviewTabContent* self)
{
    orion_webview_tab_content_sync_website_setting_bool(
        self,
        orion::settings::website::specific::enable_intelligent_tracking_prevention.key_name(),
        orion_webview_tab_content_set_enable_intelligent_tracking_prevention
    );
}

// Not used in the current implementation
static void
orion_webview_tab_content_sync_block_web_fonts(OrionWebviewTabContent* self)
{
    orion_webview_tab_content_sync_website_setting_bool(
        self,
        orion::settings::website::specific::block_web_fonts.key_name(),
        orion_webview_tab_content_set_block_web_fonts
    );
}

static void
orion_webview_tab_content_sync_enable_javascript(OrionWebviewTabContent* self)
{
    orion_webview_tab_content_sync_website_setting_bool(
        self,
        orion::settings::website::specific::enable_javascript.key_name(),
        orion_webview_tab_content_set_enable_javascript
    );
}

static void
orion_webview_tab_content_sync_enable_cookies(OrionWebviewTabContent* self)
{
    orion_webview_tab_content_sync_website_setting_bool(
        self,
        orion::settings::website::specific::enable_cookies.key_name(),
        orion_webview_tab_content_set_enable_cookies
    );
}

//endregion



//region WebView callbacks

//all callbacks can be ignored if webkit doesn't exist
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

static void
orion_webview_tab_content_on_uri_initialized_or_changed(OrionWebviewTabContent* self)
{
    // Note that orion_webview_tab_content_get_uri may still return stale data here
    orion::Uri uri = orion::Uri::parse_user_input(webkit_web_view_get_uri(self->content_webview));
    orion::log_verbose("orion_webview_tab_content_on_uri_initialized_or_changed: uri=%s", uri.uri().c_str());
    orion_webview_tab_content_sync_webview_zoom_level(self);

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(self));
    if (!root || !GTK_IS_WINDOW(root)) {
        orion::log_warning("orion_webview_tab_content_on_uri_initialized_or_changed: root is not a window");
        return;
    }
    MainWindow* win = MAIN_WINDOW(root);
    orion::TabId current_tab_id = main_window_get_current_tab_id(win);
    if (self->id == current_tab_id) {
        main_window_on_current_website_change(win);
    }
}

void
orion_webview_tab_content_show_web_inspector(OrionWebviewTabContent* content)
{
    orion::log_debug("orion_webview_tab_content_show_web_inspector");
    //Redundantly set enable, but just to be sure.
    WebKitSettings* settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(content->content_webview));
    g_object_set(G_OBJECT(settings), "enable-developer-extras", TRUE, NULL);
    WebKitWebInspector* inspector = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(content->content_webview));
    webkit_web_inspector_show(WEBKIT_WEB_INSPECTOR(inspector));
}

void
orion_webview_tab_content_toggle_web_inspector(OrionWebviewTabContent* content)
{
    orion::log_verbose(
        "orion_webview_tab_content_toggle_web_inspector: is_visible=%d",
        content->web_inspector_is_visible);

    WebKitSettings* settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(content->content_webview));
    g_object_set(G_OBJECT(settings), "enable-developer-extras", TRUE, NULL); // just to be sure

    WebKitWebInspector* inspector = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(content->content_webview));

    if (content->web_inspector_is_visible) {
        webkit_web_inspector_close(WEBKIT_WEB_INSPECTOR(inspector));
    } else {
        webkit_web_inspector_show(WEBKIT_WEB_INSPECTOR(inspector));
    }
}

//triggers on uri changed callback from webview
static void
on_webview_uri_changed(WebKitWebView* self, GParamSpec* pspec, OrionWebviewTabContent* content)
{
    orion::log_verbose("on_webview_uri_changed: new_uri=%s", webkit_web_view_get_uri(self));
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_URI]);
}

//triggers on title changed callback from webview
static void
on_webview_title_changed(WebKitWebView* self, GParamSpec* pspec, OrionWebviewTabContent* content)
{
    if (content->is_on_custom_widget) {
        return;
    }

    orion::TabId tab_id = content->id;
    orion::gtk::WeakRefWrapper self_weak(self);
    orion::gtk::WeakRefWrapper content_weak(content);

    if(!content->is_suspended) 
    {
        const gchar* title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(self));

        orion_webview_tab_content_set_tab_title_internal(content, title);
        
        orion::UpdateTabQuery tab_query{
            .id = content->id,
            .name = title ? title : "",
            .update_name = true,
        };
        global_core_api->update_tab(tab_query, [](orion::Error err){
            VERIFY_LOG(err);
        });
    }
}

static void
on_webview_favicon_changed(WebKitWebView* self, GParamSpec* pspec, OrionWebviewTabContent* content)
{
    if(!content->is_on_custom_widget)
    {
        GIcon* icon = G_ICON(webkit_web_view_get_favicon(self));
        orion_webview_tab_content_set_tab_favicon(content, icon);
    }
}

static void
on_webview_is_loading_changed(WebKitWebView* self, GParamSpec* pspec, OrionWebviewTabContent* content)
{
    content->is_loading = webkit_web_view_is_loading(self);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_IS_LOADING]);
}

static void
on_webview_is_playing_audio_changed(WebKitWebView* self, GParamSpec* pspec, OrionWebviewTabContent* content)
{
    content->is_playing_audio = webkit_web_view_is_playing_audio(self);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_IS_PLAYING_AUDIO]);
}

static void
on_webview_is_muted_changed(WebKitWebView* self, GParamSpec* pspec, OrionWebviewTabContent* content)
{
    content->is_muted = webkit_web_view_get_is_muted(self);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_IS_MUTED]);
}

static void
on_find_controller_found_text(WebKitFindController* find_controller, guint match_count, gpointer user_data)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(user_data);
    if (content->find_suppress_next_result_callback) {
        content->find_suppress_next_result_callback = FALSE;
        return;
    }
    if (content->find_result_callback) content->find_result_callback(match_count, TRUE);
}

static void
on_find_controller_failed_to_find_text(WebKitFindController* find_controller, gpointer user_data)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(user_data);
    if (content->find_suppress_next_result_callback) {
        content->find_suppress_next_result_callback = FALSE;
        return;
    }
    if (content->find_result_callback) content->find_result_callback(0, FALSE);
}

std::string
js_quote_jsc(const char* text)
{
    JSCContext* ctx = jsc_context_new();
    defer([ctx]{ g_object_unref(ctx); });

    JSCValue* v = jsc_value_new_string(ctx, text);
    defer([v]{ g_object_unref(v); });

    char* escaped = jsc_value_to_json(v, 0);
    defer([escaped]{ g_free(escaped); });

    orion::log_debug("js_quote_jsc: original='%s' escaped=%s", text, escaped);

    return std::string(escaped);
}

static void
on_input_focus_message(
    WebKitUserContentManager* mgr,
    JSCValue* message,
    gpointer user_data
);

static void
fill_username_password_finish(GObject* source_object, GAsyncResult* res, gpointer user_data)
{
    WebKitWebView* webview = WEBKIT_WEB_VIEW(source_object);

    GError* error = NULL;
    JSCValue* value = webkit_web_view_evaluate_javascript_finish(webview, res, &error);
    if (error) {
        defer([error]{ g_error_free(error); });
        orion::log_error("fill_username_password_finish: failed to run javascript: %s", error->message);
        return;
    }

    if (jsc_value_is_string(value)) {
        std::string result_str = orion::jsc::value_to_string(value);
        orion::log_debug("fill_username_password_finish: javascript result: %s", result_str.c_str());
    } else if (jsc_value_is_undefined(value)) {
        orion::log_debug("fill_username_password_finish: javascript result is undefined");
    } else if (jsc_value_is_number(value)) {
        orion::log_debug("fill_username_password_finish: javascript result is number");
    } else if (jsc_value_is_boolean(value)) {
        orion::log_debug("fill_username_password_finish: javascript result is boolean");
    } else if (jsc_value_is_null(value)) {
        orion::log_debug("fill_username_password_finish: javascript result is null");
    } else if (jsc_value_is_array(value)) {
        orion::log_debug("fill_username_password_finish: javascript result is array");
    } else if (jsc_value_is_object(value)) {
        orion::log_debug("fill_username_password_finish: javascript result is object");
    } else {
        orion::log_debug("fill_username_password_finish: javascript return result of unknown type");
    }
}

struct AutofillQuery
{
    WebKitWebView* webview;
    std::string username;
    std::string password;
    bool auto_submit;
    bool skip_username; // if true, then it will not fill the username field
};

static void
fill_username_password(const AutofillQuery& data)
{
    // Build JS that dispatches the CustomEvent consumed by the user script
    gchar* js = g_strdup_printf(R"(
        window.dispatchEvent(new CustomEvent('Autofill:fill', {
            detail: {
                username: %s,
                password: %s,
                autoSubmit: %s,
                skipUsername: %s
            }
        }));
    )", // escape username and password fields properly (they come from untrusted source),
        // g_shell_quote is insufficient
        js_quote_jsc(data.username.c_str()).c_str(),
        js_quote_jsc(data.password.c_str()).c_str(),
        (data.auto_submit ? "true" : "false"),
        (data.skip_username ? "true" : "false")
    );
    defer([js]{ g_free(js); });

    webkit_web_view_evaluate_javascript(
        data.webview, js,
        -1, // NUL-terminated string
        NULL, // source uri
        NULL, // default JS world
        NULL, // cancellable
        fill_username_password_finish, // callback
        NULL  // user_data
    );
}

// user-data struct for the idle callback
struct RepositionAutofillPopoverData
{
    GtkPopover* popover;
    GtkWidget* webview; // parent
    int widget_x; // left edge of input (widget coords)
    int widget_y; // bottom of input (widget coords)
};

// idle callback: measure and reposition popover
static gboolean
reposition_autofill_popover_later(gpointer user_data)
{
    auto ad = static_cast<RepositionAutofillPopoverData*>(user_data);

    GtkPopover* popover = ad->popover;
    GtkWidget* pw = GTK_WIDGET(popover);

    if (!gtk_widget_get_parent(pw)) {
        // the popover was already unparented/destroyed
        return G_SOURCE_REMOVE;
    }

    // Measure the popover's natural width
    int min, nat, min_h, nat_h;
    gtk_widget_measure(
        pw,
        GTK_ORIENTATION_HORIZONTAL,
        -1,
        &min, &nat, &min_h, &nat_h);

    if (nat <= 0) {
        // not ready yet, try again later
        return G_SOURCE_CONTINUE; // try again on next idle iteration
    }

    int popover_w = nat;

    // This is a quick workaround so that the popover is left-aligned with the input field
    const int offset = 0;

    // Compute anchor such that popover_left == widget_x:
    // default behavior: popover_left = anchor_x - popover_w/2
    // so anchor_x = widget_x + popover_w/2

    int anchor_x = (int)(ad->widget_x + (popover_w / 2.0)) + offset;
    GdkRectangle anchor = {
        .x = anchor_x,
        .y = (int) ad->widget_y,
        .width = 1,
        .height = 1
    };

    // Apply anchor and show (popover already shown, but update placement)
    gtk_popover_set_pointing_to(popover, &anchor);
    gtk_popover_set_position(popover, GTK_POS_BOTTOM);
    gtk_widget_queue_draw(GTK_WIDGET(popover));

    return G_SOURCE_REMOVE;
}

static gboolean
unparent_widget_later(gpointer user_data)
{
    GtkWidget* widget = GTK_WIDGET(user_data);
    GtkWidget* parent = gtk_widget_get_parent(widget);
    if (parent) {
        gtk_widget_unparent(widget);
    }
    return G_SOURCE_REMOVE;
}

static void
on_autofill_popover_closed(GtkPopover* popover, gpointer user_data)
{
    OrionWebviewTabContent* tab_content = ORION_WEBVIEW_TAB_CONTENT(user_data);
    if (!tab_content->is_autofill_popover_showing) {
        orion::log_warning("on_autofill_popover_closed: popover closed but is_autofill_popover_showing is already false");
    }
    tab_content->is_autofill_popover_showing = false;
    
    // Note that we cannot unparent the popover here directly, it would break
    // the menu action (it would never fire). Instead, we have to unparent it
    // later using idle callback.
    g_idle_add_full(
        G_PRIORITY_LOW,
        unparent_widget_later,
        g_object_ref(popover), // user_data: keep alive until idle runs
        g_object_unref // destroy notify
    );
}

struct AutofillCbData {
    orion::TabId tab_id;
    MainWindow* win;
    orion::PasswordItemId item_id{}; // optional, used only in on_fill_username_password_from_popover_menu
};

static void
on_fill_username_password_from_popover_menu(GSimpleAction* action, GVariant* param, gpointer user_data)
{
    AutofillCbData* data = static_cast<AutofillCbData*>(user_data);
    defer([data]{ delete data; });

    auto item_id = orion::PasswordItemId(g_variant_get_int64(param));
    
    orion::log_debug(
        "on_fill_username_password_from_popover_menu action activated: tab_id=%lld password_item_id=%lld",
        data->tab_id.value,
        item_id.value
    );

    OrionWebviewTabContent* tab_content = main_window_get_orion_webview_tab_content_by_tab_id(data->win, data->tab_id);
    if (!tab_content) {
        orion::log_warning(
            "on_fill_username_password_from_popover_menu: cannot find tab content for tab_id=%lld",
            data->tab_id.value
        );
        return;
    }
    
    orion::gtk::WeakRefWrapper webview_weak = tab_content->content_webview;
    orion::ProfileId profile_id = main_window_get_current_profile_id(data->win);

    global_core_api->get_password_items({item_id},
    [item_id,webview_weak,profile_id](orion::GetPasswordItemsReply reply){
        VERIFY_LOG(reply.err);

        if (reply.items.empty()) {
            orion::log_warning("on_fill_username_password_from_popover_menu: cannot find password item id %lld",
                item_id.value
            );
            return;
        }
        orion::PasswordItemDto item = reply.items[0];

        global_core_api->get_setting(profile_id, orion::settings::passwords::submit_form_automatically,
        [webview_weak,item](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            
            auto webview_strong = webview_weak.lock();
            if (!webview_strong) return;
            WebKitWebView* webview = WEBKIT_WEB_VIEW(webview_strong.get());

            fill_username_password(AutofillQuery{
                .webview = webview,
                .username = item.username.c_str(),
                .password = item.password.c_str(),
                .auto_submit = reply.value,
                .skip_username = false,
            });
        });
    });
}

struct SuggestNewPasswordData {
    WebKitWebView* webview;
    gboolean copy_to_clipboard;
};

static void
on_suggest_new_password_finish(GObject* source_object, GAsyncResult* res, gpointer user_data)
{
    auto data = static_cast<SuggestNewPasswordData*>(user_data);
    defer([data]{ delete data; });

    WebKitWebView* webview = WEBKIT_WEB_VIEW(source_object);

    GError* error = NULL;
    JSCValue* value = webkit_web_view_evaluate_javascript_finish(webview, res, &error);
    if (error) {
        defer([error]{ g_error_free(error); });
        orion::log_error("on_suggest_new_password_finish: failed to run javascript: %s", error->message);
        return;
    }

    if (!jsc_value_is_string(value)) {
        orion::log_error("on_suggest_new_password_finish: javascript did not return a string");
        return;
    }

    std::string password = orion::jsc::value_to_string(value);

    orion::log_debug("on_suggest_new_password_finish: generated password of length %zu",
        password.size()
    );

    if (data->copy_to_clipboard) {
        GdkDisplay* display = gdk_display_get_default();
        GdkClipboard* clipboard = gdk_display_get_clipboard(display);
        gdk_clipboard_set_text(clipboard, password.c_str());
    }

    fill_username_password(AutofillQuery{
        .webview = webview,
        .username = "",
        .password = password,
        .auto_submit = false,
        .skip_username = true,
    });
}

static void
on_suggest_new_password(GSimpleAction* action, GVariant* param, gpointer user_data)
{
    WebKitWebView* webview = WEBKIT_WEB_VIEW(user_data);
    gboolean copy_to_clipboard = g_variant_get_boolean(param);

    const char* js = R"(
        (function() {
            const charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
            const len = 16;
            let out = "";

            const arr = new Uint8Array(len);
            crypto.getRandomValues(arr);

            for (let i = 0; i < len; i++) {
                out += charset[arr[i] % charset.length];
            }
            return out;   // return directly (string)
        })();
    )";

    SuggestNewPasswordData* data = new SuggestNewPasswordData{
        .webview = webview,
        .copy_to_clipboard = copy_to_clipboard,
    };

    webkit_web_view_evaluate_javascript(
        webview, js,
        -1, // NUL-terminated string
        NULL, // source uri
        NULL, // default JS world
        NULL, // cancellable
        on_suggest_new_password_finish, // callback
        data  // user_data
    );
}

static void
on_open_fill_password_dialog(GSimpleAction* action, GVariant* param, gpointer user_data)
{
    AutofillCbData* data = static_cast<AutofillCbData*>(user_data);
    defer([data]{ delete data; });

    FillPasswordDialogResultCb result_cb = [](std::string username, std::string password, gpointer user_data)->bool{
        AutofillCbData* data = static_cast<AutofillCbData*>(user_data);
        defer([data]{ delete data; });

        orion::log_debug("FillPasswordDialogResultCb: username=%s password=%s",
            username.c_str(),
            password.c_str()
        );
        
        OrionWebviewTabContent* content = main_window_get_orion_webview_tab_content_by_tab_id(data->win, data->tab_id);
        if (!content) {
            orion::log_warning("open_fill_password_dialog: cannot find tab content for tab_id=%lld",
                data->tab_id.value
            );
            return false;
        }

        MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(content)));
        orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);
        orion::gtk::WeakRefWrapper webview_weak(content->content_webview);

        global_core_api->get_setting(profile_id, orion::settings::passwords::submit_form_automatically,
            [webview_weak,username,password](orion::GetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);

                auto webview_strong = webview_weak.lock();
                if (!webview_strong) return;
                WebKitWebView* webview = WEBKIT_WEB_VIEW(webview_strong.get());

                fill_username_password(AutofillQuery{
                    .webview = webview,
                    .username = username,
                    .password = password,
                    .auto_submit = reply.value,
                    .skip_username = false,
                });
            });
        return true;
    };

    AutofillCbData* cb_data = new AutofillCbData{
        .tab_id = data->tab_id,
        .win = data->win,
    };

    FillPasswordDialog* dialog = APP_FILL_PASSWORD_DIALOG(fill_password_dialog_new(result_cb, cb_data));
    adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(data->win));
}

enum class AutofillType
{
    unknown,
    username,
    password,
};

struct AutofillPopoverParams
{
    OrionWebviewTabContent* tab_content;
    int widget_x;
    int widget_y;
    AutofillType type;
};

static void
populate_autofill_menu(GMenu* menu, WebKitWebView* webview, AutofillType type)
{
    const char* uri_string = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(webview));
    orion::Uri uri = orion::Uri::parse_strict(uri_string);

    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(webview)));
    if (!main_window) {
        orion::log_debug("populate_autofill_menu: cannot find main window");
        return;
    }

    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);
    orion::GetPasswordItemIdsQuery query{
        .profile_id = profile_id,
        .authority = uri.authority(),
    };
    orion::TabId tab_id = main_window_get_current_tab_id(main_window);

    orion::gtk::WeakRefWrapper menu_weak(menu);
    orion::gtk::WeakRefWrapper win_weak(main_window);
    orion::gtk::WeakRefWrapper webview_weak(webview);

    global_core_api->get_password_item_ids(query,
    [menu_weak,win_weak,tab_id,type,webview_weak](orion::GetPasswordItemIdsReply reply){
        VERIFY_LOG(reply.err);

        global_core_api->get_password_items(reply.ids,
        [menu_weak,win_weak,tab_id,type,webview_weak](orion::GetPasswordItemsReply reply){
            VERIFY_LOG(reply.err);

            auto menu_strong = menu_weak.lock();
            if (!menu_strong) return;
            GMenu* menu = G_MENU(menu_strong.get());

            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            auto webview_strong = webview_weak.lock();
            if (!webview_strong) return;
            WebKitWebView* webview = WEBKIT_WEB_VIEW(webview_strong.get());

            GSimpleAction* action = g_simple_action_new("open-fill-password-dialog", NULL);
            AutofillCbData* cb_data = new AutofillCbData{
                .tab_id = tab_id,
                .win = win,
            };
            g_signal_connect(action, "activate", G_CALLBACK(on_open_fill_password_dialog), cb_data);

            // the possibly existing action is dropped automatically if it exists with this
            // action name already
            g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(action));

            if (reply.items.empty()) {
                // no saved passwords for this particular website
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
                GMenuItem* menu_item = g_menu_item_new(_("Passwords…"), "win.open-fill-password-dialog");
                defer([menu_item]{ g_object_unref(menu_item); });
                g_menu_append_item(section, menu_item);
            } else {
                // at least one saved password for this website
                {
                    GMenu* section = g_menu_new();
                    defer([section]{ g_object_unref(section); });
                    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));

                    for (const orion::PasswordItemDto& item : reply.items) {
                        std::string action_name = "on-fill-username-password-from-popover-menu";
                        GSimpleAction* action = g_simple_action_new(
                            action_name.c_str(), G_VARIANT_TYPE_INT64
                        );
                        AutofillCbData* cb_data = new AutofillCbData{
                            .tab_id = tab_id,
                            .win = win,
                            .item_id = item.id,
                        };
                        g_signal_connect(action, "activate", G_CALLBACK(on_fill_username_password_from_popover_menu), cb_data);

                        // the possibly existing action is dropped automatically if it exists with this
                        // action_name already
                        g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(action));
                        
                        orion::Uri item_uri = orion::Uri::parse_strict(item.url.c_str());
                        std::string label = item.username + " (" + item_uri.authority() + ")";
                        GMenuItem* menu_item = g_menu_item_new(label.c_str(), NULL);
                        defer([menu_item]{ g_object_unref(menu_item); });
                        g_menu_item_set_action_and_target(
                            menu_item, ("win." + action_name).c_str(),
                            (const gchar*)G_VARIANT_TYPE_INT64, item.id.value
                        );
                        g_menu_append_item(section, menu_item);
                    }
                }
                {
                    GMenu* section = g_menu_new();
                    defer([section]{ g_object_unref(section); });
                    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
                    GMenuItem* menu_item = g_menu_item_new(_("Other Passwords…"), "win.open-fill-password-dialog");
                    defer([menu_item]{ g_object_unref(menu_item); });
                    g_menu_append_item(section, menu_item);
                }
            }

            if (type == AutofillType::password) {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                g_menu_append_section(menu, NULL, G_MENU_MODEL(section));

                std::string action_name = "on-suggest-new-password";
                GSimpleAction* action = g_simple_action_new(action_name.c_str(), G_VARIANT_TYPE_BOOLEAN);
                g_signal_connect(action, "activate", G_CALLBACK(on_suggest_new_password), webview);
                g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(action));

                {
                    GMenuItem* menu_item = g_menu_item_new(_("Suggest New Password"), NULL);
                    defer([menu_item]{ g_object_unref(menu_item); });
                    g_menu_item_set_action_and_target(
                        menu_item, ("win." + action_name).c_str(),
                        (const gchar*)G_VARIANT_TYPE_BOOLEAN, FALSE // do not copy to clipboard
                    );
                    g_menu_append_item(section, menu_item);
                }
                {
                    GMenuItem* menu_item = g_menu_item_new(_("Suggest and Copy New Password"), NULL);
                    defer([menu_item]{ g_object_unref(menu_item); });
                    g_menu_item_set_action_and_target(
                        menu_item, ("win." + action_name).c_str(),
                        (const gchar*)G_VARIANT_TYPE_BOOLEAN, TRUE // copy to clipboard
                    );
                    g_menu_append_item(section, menu_item);
                }
            }
        });
    });
}

static void
show_autofill_popover(AutofillPopoverParams params)
{
    OrionWebviewTabContent* tab_content = params.tab_content;
    GtkWidget* webview = GTK_WIDGET(params.tab_content->content_webview);

    if (tab_content->is_autofill_popover_showing) {
        orion::log_debug("show_autofill_popover: popover is already showing, not showing another");
        return;
    }

    if (!orion::gtk::is_widget_safe_for_popover_parent(GTK_WIDGET(webview))) {
        orion::log_warning("show_autofill_popover: webview is not safe for popover parent, not showing popover");
        return;
    }

    GMenu* menu = g_menu_new();
    defer([menu]{ g_object_unref(menu); });

    populate_autofill_menu(menu, WEBKIT_WEB_VIEW(webview), params.type);

    GtkPopoverMenu* popover = GTK_POPOVER_MENU(gtk_popover_menu_new_from_model(G_MENU_MODEL(menu)));

    gtk_widget_set_parent(GTK_WIDGET(popover), webview);

    // We have to unparent the popover when closed to avoid dangling references
    g_signal_connect(popover, "closed", G_CALLBACK(on_autofill_popover_closed), tab_content);

    // initial anchor (1px at input left bottom)
    GdkRectangle initial = {
        .x = params.widget_x,
        .y = params.widget_y,
        .width = 1,
        .height = 1
    };

    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &initial);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_popover_set_autohide(GTK_POPOVER(popover), TRUE);

    gtk_popover_popup(GTK_POPOVER(popover));
    tab_content->is_autofill_popover_showing = true;

    // schedule adjust in idle so we can measure after layout
    auto data = new RepositionAutofillPopoverData {
        .popover = GTK_POPOVER(popover),
        .webview = webview,
        .widget_x = params.widget_x,
        .widget_y = params.widget_y,
    };

    // run after current events so measurement returns correct natural size
    g_idle_add_full(
        G_PRIORITY_DEFAULT,
        reposition_autofill_popover_later,
        data,
        [](gpointer user_data){
            delete static_cast<RepositionAutofillPopoverData*>(user_data);
        }
    );
}

static void
on_input_focus_message(WebKitUserContentManager* mgr, JSCValue* message, gpointer user_data)
{
    OrionWebviewTabContent* tab_content = ORION_WEBVIEW_TAB_CONTENT(user_data);

    if (!jsc_value_is_object(message)) {
        return;
    }

    JSCValue* type = jsc_value_object_get_property(message, "type");
    defer([type]{ g_object_unref(type); });

    JSCValue* html_type = jsc_value_object_get_property(message, "htmlType");
    defer([html_type]{ g_object_unref(html_type); });

    JSCValue* x_val = jsc_value_object_get_property(message, "x");
    defer([x_val]{ g_object_unref(x_val); });

    JSCValue* y_val = jsc_value_object_get_property(message, "y");
    defer([y_val]{ g_object_unref(y_val); });

    JSCValue* scroll_x_val = jsc_value_object_get_property(message, "scrollX");
    defer([scroll_x_val]{ g_object_unref(scroll_x_val); });

    JSCValue* scroll_y_val = jsc_value_object_get_property(message, "scrollY");
    defer([scroll_y_val]{ g_object_unref(scroll_y_val); });

    JSCValue* width_val = jsc_value_object_get_property(message, "width");
    defer([width_val]{ g_object_unref(width_val); });

    JSCValue* height_val = jsc_value_object_get_property(message, "height");
    defer([height_val]{ g_object_unref(height_val); });

    std::string type_str = orion::jsc::value_to_string(type);
    std::string html_type_str = orion::jsc::value_to_string(html_type);

    int page_x = (int)jsc_value_to_double(x_val);
    int page_y = (int)jsc_value_to_double(y_val);
    int scroll_x = (int)jsc_value_to_double(scroll_x_val);
    int scroll_y = (int)jsc_value_to_double(scroll_y_val);
    int width = (int)jsc_value_to_double(width_val);
    int height = (int)jsc_value_to_double(height_val);

    orion::log_debug("focused input: page_xy=(%d,%d) scroll_xy=(%d,%d) w=%d h=%d htmlType=%s type=%s",
        page_x, page_y, scroll_x, scroll_y, width, height, html_type_str.c_str(), type_str.c_str()
    );

    // Convert to widget (WebView) coordinates
    int widget_x = page_x - scroll_x;
    int widget_y = (page_y - scroll_y) + height; // below the input

    AutofillType autofill_type = AutofillType::unknown;
    if (g_strcmp0(type_str.c_str(), "username") == 0) {
        autofill_type = AutofillType::username;
    } else if (g_strcmp0(type_str.c_str(), "password") == 0) {
        autofill_type = AutofillType::password;
    }

    if (autofill_type == AutofillType::unknown) {
        orion::log_verbose("autofill: focused input is of unknown type, not showing popover");
        return;
    }

    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(tab_content)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);
    orion::gtk::WeakRefWrapper tab_content_weak(tab_content);

    global_core_api->get_setting(
        profile_id, orion::settings::passwords::offer_to_autofill_passwords_from_orion_keychain,
    [tab_content_weak,widget_x,widget_y,autofill_type](orion::GetSettingReply<bool> reply){
        VERIFY_LOG(reply.err);

        if (!reply.value) {
            orion::log_debug("autofill is disabled in settings, not showing popover");
            return;
        }

        auto tab_content_strong = tab_content_weak.lock();
        if (!tab_content_strong) return;
        OrionWebviewTabContent* tab_content = ORION_WEBVIEW_TAB_CONTENT(tab_content_strong.get());
        
        AutofillPopoverParams params {
            .tab_content = tab_content,
            .widget_x = widget_x,
            .widget_y = widget_y,
            .type = autofill_type,
        };
        
        // Show popover
        show_autofill_popover(params);
    });
}

static void
register_autofill_input_focus_handler(OrionWebviewTabContent* tab_content)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(tab_content->content_webview);

    // Register a script message handler named "inputFocus"
    webkit_user_content_manager_register_script_message_handler(
        mgr,
        "inputFocus",
        AUTOFILL_WORLD
    );

    g_signal_connect_object(
        mgr, "script-message-received::inputFocus", G_CALLBACK(on_input_focus_message), tab_content, G_CONNECT_DEFAULT
    );
}

// custom webpage console.log, console.error, console.warn etc. handler
static void
on_console_redirect_message(WebKitUserContentManager* mgr, JSCValue* message, gpointer user_data)
{
    if (!show_console_redirect_messages) {
        return;
    }

    JSCValue* level_val = jsc_value_object_get_property(message, "level");
    defer([level_val]{ g_object_unref(level_val); });

    JSCValue* msg_val = jsc_value_object_get_property(message, "message");
    defer([msg_val]{ g_object_unref(msg_val); });

    orion::log_debug("on_console_redirect_message: level=%s message=%s",
        orion::jsc::value_to_string(level_val).c_str(),
        orion::jsc::value_to_string(msg_val).c_str()
    );
}





static orion::CssClass
reader_shadow_enabled_to_background_color_css(orion::settings::ReaderModeBorderShadowEnabled enabled)
{
    if(enabled == orion::settings::ReaderModeBorderShadowEnabled::on)
    {
        return {
            "background-colors-style-id",
            R"(
                html.paper body.white {
                    background-color: rgb(230, 230, 230);
                }
                html.paper body.sepia {
                    background-color: rgb(224, 216, 200);
                }
                html.paper body.gray {
                    background-color: rgb(50, 50, 51);
                }
                html.paper body.night {
                    background-color: black;
                }
            )"};
    }
    else
    {
        return {
            "background-colors-style-id",
            R"(
                html.paper body.white {
                    background-color: white;
                }
                html.paper body.sepia {
                    background-color: rgb(248, 241, 227);
                }
                html.paper body.gray {
                    background-color: rgb(74, 74, 77);
                }
                html.paper body.night {
                    background-color: rgb(18, 18, 18);
                }
            )"};
    }
}

static orion::CssClass
reader_shadow_enabled_to_paper_shadow_css(orion::settings::ReaderModeBorderShadowEnabled enabled)
{
    switch(enabled)
    {
        case orion::settings::ReaderModeBorderShadowEnabled::not_set:
            return {"reader-shadow-style-id", ""};
        case orion::settings::ReaderModeBorderShadowEnabled::on:
            return {
                "reader-shadow-style-id",
                R"(
                    html.paper body .white #article, html.paper body.sepia #article {
                        box-shadow: 0px 6px 12px 3px var(--paper-shadow-color);
                    }
                    html.paper body.gray #article {
                        box-shadow: 0px 6px 12px 3px rgba(0, 0, 0, 0.24);
                    }
                    html.paper body.night #article {
                        outline: 1px solid #272727;
                    }
                )"};
        case orion::settings::ReaderModeBorderShadowEnabled::off:
            return {"reader-shadow-style-id",""};
    }
    orion::log_error("text_alignment_to_css(): bad enum value");
    return {"reader-shadow-style-id",""};
}

static std::string
text_alignment_to_css(orion::settings::TextAlignment alignment)
{
    switch(alignment)
    {
        case orion::settings::TextAlignment::not_set:
            return "left";
        case orion::settings::TextAlignment::left:
            return "left";
        case orion::settings::TextAlignment::justify:
            return "justify";
    }
    orion::log_error("text_alignment_to_css(): bad enum value");
    return "left";
}

static std::string
background_type_to_css(orion::settings::ReaderModeBackgroundType type)
{
    switch(type)
    {
        case orion::settings::ReaderModeBackgroundType::not_set:
            return "white";
        case orion::settings::ReaderModeBackgroundType::white:
            return "white";
        case orion::settings::ReaderModeBackgroundType::sepia:
            return "sepia";
        case orion::settings::ReaderModeBackgroundType::gray:
            return "gray";
        case orion::settings::ReaderModeBackgroundType::night:
            return "night";
    }
    orion::log_error("text_alignment_to_css(): bad enum value");
    return "left";
}

static gboolean
downwards_animation_tick_callback(GtkWidget* widget, GdkFrameClock* clock, gpointer user_data)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(widget);

    if(content->start_animation_on_next_frame)
    {
        content->start_animation_on_next_frame = false;
        gint64 animation_duration_accounted_for_by_progress = 
            READER_MODE_TRANSITION_DURATION_US * (1.0f - content->reader_mode_upwards_coverage);
        content->animation_start_time = gdk_frame_clock_get_frame_time(clock) - animation_duration_accounted_for_by_progress;
        return G_SOURCE_CONTINUE;
    }

    gint64 current_time = gdk_frame_clock_get_frame_time(clock);

    float upwards_progress = (float)(READER_MODE_TRANSITION_DURATION_US-current_time+content->animation_start_time)
        /READER_MODE_TRANSITION_DURATION_US;
    content->reader_mode_upwards_coverage = std::clamp(upwards_progress, 0.0f, 1.0f);

    if(current_time - content->animation_start_time >= READER_MODE_TRANSITION_DURATION_US)
    {
        content->reader_mode_upwards_coverage = 0.0f;
        gtk_widget_set_visible(GTK_WIDGET(content->reader_mode_webview), false);
        content->anim_cb_id = 0;
        return G_SOURCE_REMOVE;
    }
    gtk_widget_queue_allocate(widget);

    return G_SOURCE_CONTINUE;
}

static void
orion_webview_tab_content_hide_reader_mode_webview(OrionWebviewTabContent* self)
{
    if(self->reader_mode_is_shown)
    {
        gtk_widget_set_visible(GTK_WIDGET(self->reader_mode_webview), true);
        if(self->anim_cb_id)
        {
            gtk_widget_remove_tick_callback(GTK_WIDGET(self), self->anim_cb_id);
        }
        self->anim_cb_id = gtk_widget_add_tick_callback(GTK_WIDGET(self), downwards_animation_tick_callback, NULL, NULL);
        self->start_animation_on_next_frame = true;
        self->reader_mode_is_shown = false;
    }
}

static gboolean
upwards_animation_tick_callback(GtkWidget* widget, GdkFrameClock* clock, gpointer user_data)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(widget);

    if(content->start_animation_on_next_frame)
    {
        content->start_animation_on_next_frame = false;
        gint64 animation_duration_accounted_for_by_progress = READER_MODE_TRANSITION_DURATION_US * content->reader_mode_upwards_coverage;
        content->animation_start_time = gdk_frame_clock_get_frame_time(clock) - animation_duration_accounted_for_by_progress;
        return G_SOURCE_CONTINUE;
    }

    gint64 current_time = gdk_frame_clock_get_frame_time(clock);
    float progress = (float)(current_time-content->animation_start_time)/READER_MODE_TRANSITION_DURATION_US;
    content->reader_mode_upwards_coverage = std::clamp(progress, 0.0f, 1.0f);
    gtk_widget_queue_allocate(widget);
    if(current_time - content->animation_start_time >= READER_MODE_TRANSITION_DURATION_US)
    {
        content->anim_cb_id = 0;
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

static void
orion_webview_tab_content_show_reader_mode_webview(OrionWebviewTabContent* self)
{
    if(!self->reader_mode_is_shown)
    {
        gtk_widget_set_visible(GTK_WIDGET(self->reader_mode_webview), true);
        if(self->anim_cb_id)
        {
            gtk_widget_remove_tick_callback(GTK_WIDGET(self), self->anim_cb_id);
        }
        self->anim_cb_id = gtk_widget_add_tick_callback(GTK_WIDGET(self), upwards_animation_tick_callback, NULL, NULL);
        self->start_animation_on_next_frame = true;
        self->reader_mode_is_shown = true;
    }
    if(!self->reader_mode_enabled)
    {
        self->reader_mode_enabled = true;
        self->webview_is_reader_mode_enabled = true;
        g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_READER_MODE_ENABLED]);
    }
}

static void
reader_mode_init_cb(WebKitUserContentManager* mgr, JSCValue* val, OrionWebviewTabContent* content)
{
    if(!jsc_value_is_string(val))
    {
        orion::log_error("reader_mode_init_cb() got none string value");
        return;
    }

    char* epoch = jsc_value_to_string(val);
    defer([epoch]{ g_free(epoch); });
    if(content->cpp_data.current_reader_epoch == epoch)
    {
        content->last_webview_page_was_readered = true;
        orion_webview_tab_content_show_webview(content);
    }
}

static gboolean
on_reader_mode_webview_decide_policy(OrionWebkitWebview* view, WebKitPolicyDecision* decision, 
    WebKitPolicyDecisionType type, OrionWebviewTabContent* content)
{
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        WebKitNavigationPolicyDecision* nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction* nav_action = webkit_navigation_policy_decision_get_navigation_action(nav_decision);
        WebKitNavigationType nav_type = webkit_navigation_action_get_navigation_type(nav_action);
        
        switch(nav_type)
        {
            case WEBKIT_NAVIGATION_TYPE_OTHER:
                return FALSE;
            case WEBKIT_NAVIGATION_TYPE_LINK_CLICKED:
                {
                    webkit_policy_decision_ignore(decision);
                    WebKitURIRequest* request = webkit_navigation_action_get_request(nav_action);
                    orion_webview_tab_content_load_uri(content, orion::Uri::parse_strict(webkit_uri_request_get_uri(request)));
                    return TRUE;
                }
            default:
                webkit_policy_decision_ignore(decision);
                return TRUE;
        }
    }

    return FALSE;
}

static void
handle_reader_mode_message_received(OrionWebviewTabContent* self, JSCValue* val)
{
    if(!jsc_value_is_string(val))
    {
        return;
    }

    char* val_c_string = jsc_value_to_string(val);
    defer([val_c_string]{ g_free(val_c_string); });

    if(0 == g_strcmp0(val_c_string, "bodyClicked"))
    {
        if(!self->reader_mode_enabled) return;
        
        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
        if(!win) return;
        orion::ProfileId profile_id = main_window_get_current_profile_id(win);
        orion::Uri uri = orion_webview_tab_content_get_uri(self);

        const auto setting = WEBSITE_SETTING(enable_reader_mode, uri.authority());

        orion::SetSettingQuery query{
                .profile_id = profile_id,
                .key = setting.key(),
                .realm = setting.realm(),
                .key_name = setting.key_name(),
                .type = setting.type(),
                .value = orion::settings::marshal_value(orion::settings::ReaderModeWebsiteSetting::off),
                .default_value = setting.default_value_as_string()
        };
        global_core_api->set_setting_raw(query, [](orion::SetSettingReply<std::string> reply){
            VERIFY_LOG(reply.err);
            orion::log_verbose("set reader mode setting to value: %s", reply.new_value.c_str());
        });
    }
    else
    {
        orion::log_error("handle_reader_mode_message_received(): got invalid string \"%s\"", val_c_string);
    }
}

static void
on_reader_template_script_changed(gpointer user_data)
{
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(user_data);

    self->reader_mode_template_script_invalid = true;
}

static void
orion_webview_tab_content_reader_mode_webview_obtained(OrionWebviewTabContent* self, WebViewRef new_webview)
{
    self->reader_mode_webview = WEBKIT_WEB_VIEW(new_webview.release());

    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(self->reader_mode_webview);
    webkit_user_content_manager_register_script_message_handler(
        mgr,
        "readerMode",
        READABILITY_SCRIPT_WORLD
    );
    g_signal_connect_object(
        mgr, "script-message-received::readerMode", G_CALLBACK(handle_reader_mode_message_received), self, G_CONNECT_SWAPPED
    );
    webkit_user_content_manager_add_style_sheet(mgr, orion_app_get_reader_mode_font_style_sheet());

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(win)
    {
        self->prev_reader_template_script = webkit_user_script_ref(main_window_get_reader_template_script(win));
        webkit_user_content_manager_add_script(mgr, self->prev_reader_template_script);
    }
    GtkExpression* root_expression = gtk_property_expression_new(orion_webview_tab_content_get_type(), NULL, "root");
    GtkExpression* script_expression = gtk_property_expression_new(MAIN_WINDOW_TYPE, root_expression, "reader-template-script");
    self->reader_template_script_watch = gtk_expression_watch(script_expression, self, on_reader_template_script_changed, self, NULL);
    g_clear_pointer(&script_expression, gtk_expression_unref);
    webkit_user_content_manager_register_script_message_handler(mgr, "readerInitialised", READABILITY_SCRIPT_WORLD);
    g_signal_connect_object(mgr, "script-message-received::readerInitialised", G_CALLBACK(reader_mode_init_cb), self, G_CONNECT_DEFAULT);


    g_object_bind_property(self, "can-go-back", self->reader_mode_webview, "can-go-back", G_BINDING_SYNC_CREATE);
    g_object_bind_property(self, "can-go-forward", self->reader_mode_webview, "can-go-forward", G_BINDING_SYNC_CREATE);

    g_signal_connect_data(self->reader_mode_webview, "go-back", G_CALLBACK(orion_webview_tab_content_go_back), self, NULL, G_CONNECT_SWAPPED);
    g_signal_connect_data(self->reader_mode_webview, "go-forward", G_CALLBACK(orion_webview_tab_content_go_forward), self, NULL, G_CONNECT_SWAPPED);

    g_signal_connect(self->reader_mode_webview, "decide-policy", G_CALLBACK(on_reader_mode_webview_decide_policy), self);

    gtk_widget_set_parent(GTK_WIDGET(self->reader_mode_webview), GTK_WIDGET(self));
}

static void
orion_webview_tab_content_load_reader_content_html_in_reader_webview(OrionWebviewTabContent* self,
    std::string&& doc, std::string&& origin_uri, std::string&& text_for_initial_offset)
{
    auto handler = [doc = std::move(doc), origin_uri = std::move(origin_uri), text_for_initial_offset = std::move(text_for_initial_offset)](OrionWebviewTabContent* self){
        orion_webview_tab_content_show_reader_mode_webview(self);

        WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(self->reader_mode_webview);
        if(self->prev_reader_init_script)
        {
            webkit_user_content_manager_remove_script(mgr, self->prev_reader_init_script);
            webkit_user_script_unref(self->prev_reader_init_script);
        }

        std::string init_script = std::format(R"(
            window.__epoch = '{}';
            window.textForInitialOffset = '{}';
        )", self->cpp_data.current_reader_epoch, text_for_initial_offset);

        self->prev_reader_init_script = webkit_user_script_new_for_world(init_script.c_str(), WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
            WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, READABILITY_SCRIPT_WORLD, NULL, NULL);

        webkit_user_content_manager_add_script(mgr, self->prev_reader_init_script);
        //webkit_user_script_unref(self->prev_reader_init_script);
        //user content manager only holds a strong reference to the c++ user script class, not the webkigtk wrapper we user here

        if(self->reader_mode_template_script_invalid)
        {
            if(self->prev_reader_template_script)
            {
                webkit_user_content_manager_remove_script(mgr, self->prev_reader_template_script);
                webkit_user_script_unref(self->prev_reader_template_script);
            }

            GValue val = G_VALUE_INIT;
            g_value_init(&val, WEBKIT_TYPE_USER_SCRIPT);
            if(gtk_expression_watch_evaluate(self->reader_template_script_watch, &val))
            {
                //if we return true, that means we got all the way to the window's property, and main window should never give us NULL here
                self->prev_reader_template_script = webkit_user_script_ref(static_cast<WebKitUserScript*>(g_value_get_boxed(&val)));
                webkit_user_content_manager_add_script(mgr, self->prev_reader_template_script);
            }
            g_value_unset(&val);
        }

        webkit_web_view_load_html(self->reader_mode_webview, doc.c_str(), origin_uri.c_str());
    };

    if(self->reader_mode_webview)
    {
        handler(self);
    }
    else
    {
        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
        if(!win)
        {
            orion::log_error("orion_webview_tab_content_run_reader_content_script_in_reader_webview(): tab content not parented");
            return;
        }

        orion::gtk::WeakRefWrapper self_weak(self);
        auto& factory = main_window_get_current_webview_factory(win);
        //slight optimisation to not duplicate webprocesses when possible
        WebViewRef new_webview = factory.web_view_for_content_synchronous(WebViewRef((OpaqueWebView*)g_object_ref(self->content_webview)));
        if(new_webview)
        {
            orion_webview_tab_content_reader_mode_webview_obtained(self, std::move(new_webview));
            handler(self);
        }
        else
        {
            orion::gtk::WeakRefWrapper self_weak(self);
            factory.web_view_for_content([handler = std::move(handler), self_weak](WebViewRef new_webview){
                auto self_strong = self_weak.lock();
                if(!self_strong) return;
                OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(self_strong.get());

                orion_webview_tab_content_reader_mode_webview_obtained(self, std::move(new_webview));
                handler(self);
            });
        }
    }
}

static void
handle_reader_data_received(OrionWebviewTabContent* self, JSCValue* val_with_epoch)
{
    GActionMap* win = G_ACTION_MAP(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    auto reader_check_failed = [self](){
        self->last_webview_page_was_readered = false;
        orion_webview_tab_content_hide_reader_mode_webview(self);
        self->is_readerable = false;
        g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_IS_READERABLE]);
    };

    if (!jsc_value_is_object(val_with_epoch) ||
        !jsc_value_object_has_property(val_with_epoch, "epoch") ||
        !jsc_value_object_has_property(val_with_epoch, "readerData") )
    {
        reader_check_failed();
        return;
    }

    
    {
        JSCValue* epoch_val = jsc_value_object_get_property(val_with_epoch, "epoch");
        if(!jsc_value_is_string(epoch_val))
        {
            reader_check_failed();
            return;
        }
        char* epoch_str = jsc_value_to_string(epoch_val);
        defer([epoch_str]{ g_free(epoch_str); });
        if(self->cpp_data.current_reader_epoch != epoch_str)
        {
            //stale data check, doesn't need to invalidate current reader mode enabled state
            return;
        }
    }
    
    JSCValue* val = jsc_value_object_get_property(val_with_epoch, "readerData");
    defer([val]{ g_object_unref(val); });
    if(!jsc_value_is_object(val))
    {
        reader_check_failed();
        return;
    }



    self->is_readerable = true;
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_IS_READERABLE]);


    std::string title{};
    std::string content{};
    std::string byline{};
    std::string text_for_initial_offset{};
    if(jsc_value_object_has_property(val, "title"))
    {
        JSCValue* title_val = jsc_value_object_get_property(val, "title");
        if(jsc_value_is_string(title_val))
        {
            gchar* title_c_str = jsc_value_to_string(title_val);
            title = title_c_str;
            g_free(title_c_str);
        }
        g_object_unref(title_val);
    }

    if(jsc_value_object_has_property(val, "content"))
    {
        JSCValue* content_val = jsc_value_object_get_property(val, "content");
        if(jsc_value_is_string(content_val))
        {
            gchar* content_c_str = jsc_value_to_string(content_val);
            content = content_c_str;
            g_free(content_c_str);
        }
        g_object_unref(content_val);
    }
    
    if(jsc_value_object_has_property(val, "byline"))
    {
        JSCValue* byline_val = jsc_value_object_get_property(val, "byline");
        if(jsc_value_is_string(byline_val))
        {
            gchar* byline_c_str = jsc_value_to_string(byline_val);
            byline = byline_c_str;
            g_free(byline_c_str);
        }
        g_object_unref(byline_val);
    }
    
    if(jsc_value_object_has_property(val, "textForOffset"))
    {
        JSCValue* text_for_offset_val = jsc_value_object_get_property(val, "textForOffset");
        if(jsc_value_is_string(text_for_offset_val))
        {
            gchar* text_for_offset_c_str = jsc_value_to_string(text_for_offset_val);
            text_for_initial_offset = text_for_offset_c_str;
            g_free(text_for_offset_c_str);
        }
        g_object_unref(text_for_offset_val);
    }

    //this uri should be reliable, definitely changes before this fires, and if a new page is loaded afer, we close reader mode anyway
    const gchar* uri_c_string = webkit_web_view_get_uri(self->content_webview);
    std::string uri = uri_c_string?uri_c_string:"";

    GError* err = NULL;
    GBytes* bytes = g_resources_lookup_data(GRESOURCE_PREFIX "/internalHTML/reader_template.html",
        G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
    if(err)
    {
        orion::log_error("%s", err->message);
        g_error_free(err);
        return;
    }

    defer([bytes]{ g_bytes_unref(bytes); });

    gsize len = 0;
    const char* template_data = static_cast<const char*>(g_bytes_get_data(bytes, &(len)));
    std::string_view template_string(template_data, len);

    auto font = orion::gtk::get_enum_action_state<orion::settings::Font>(win, "reader-mode-font-name");

    auto alignment = orion::gtk::get_enum_action_state<orion::settings::TextAlignment>(win, "reader-mode-text-alignment");

    auto background_type = orion::gtk::get_enum_action_state<orion::settings::ReaderModeBackgroundType>(win, "reader-mode-background-type");

    auto reader_border_shadow_enabled = orion::gtk::get_enum_action_state<orion::settings::ReaderModeBorderShadowEnabled>(win, "reader-mode-border-shadow-enabled");

    orion::str::PlaceholderMap dict = {
        {"READER-URL", uri},
        {"READER-TITLE", title},
        {"READER-CONTENT", content},
        {"READER-BYLINE", byline},
        {"READER-BACKGROUND", background_type_to_css(background_type)},
        {"READER-FONTNAME", orion::font_name_to_css_class_name(font)},
        {"READER-FONTFAMILY", orion::font_name_to_css_font_family(font)},
        {"READER-DYSLECTIC-FONT-BASE64", orion_app_get_dyslexic_font_base64()},
        {"READER-TEXTALIGNMENT", text_alignment_to_css(alignment)},
        {"READER-PLATFORM", "mac"},
        {"READER-PAPER-SHADOW-STYLE", reader_shadow_enabled_to_paper_shadow_css(reader_border_shadow_enabled).full_class()},
        {"READER-BACKGROUND-COLORS-CSS", reader_shadow_enabled_to_background_color_css(reader_border_shadow_enabled).full_class()},
    };



    std::string script = orion::str::parse_string_prepend_append_and_replace_placeholders(template_string, "", "", dict, true);

    orion_webview_tab_content_load_reader_content_html_in_reader_webview(self, std::move(script), std::move(uri), std::move(text_for_initial_offset));

    self->cpp_data.prev_font_name = font; //sadly cannot avoid a race condition but it should be unlikely to trigger and makes for an only mild visual issue
}

static void
handle_reader_epoch_received(OrionWebviewTabContent* self, JSCValue* val)
{
    if(!jsc_value_is_string(val))
    {
        orion::log_error("handle_reader_epoch_received(): got non string jscvalue");
        return;
    }

    char* str = jsc_value_to_string(val);
    defer([str]{ g_free(str); });
    self->cpp_data.current_reader_epoch = str;

    self->is_readerable = true;
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_IS_READERABLE]);
    orion_webview_tab_content_check_reader_mode(self);
}

static void
handle_picker_complete_received(OrionWebviewTabContent* self, JSCValue* val)
{   
    if(jsc_value_is_null(val))
    {
        self->element_picker_running = false;
        g_signal_emit(self, obj_signals[SIG_ELEMENT_PICKER_FAIL], 0);
        return;
    }


    if (!jsc_value_is_object(val) ||
        !jsc_value_object_has_property(val, "selector") ||
        !jsc_value_object_has_property(val, "host") ||
        !jsc_value_object_has_property(val, "erase"))
    {
        return;
    }

    JSCValue* selector_val = jsc_value_object_get_property(val, "selector");
    defer([selector_val]{ g_object_unref(selector_val); });
    JSCValue* host_val = jsc_value_object_get_property(val, "host");
    defer([host_val]{ g_object_unref(host_val); });
    JSCValue* erase_val = jsc_value_object_get_property(val, "erase");
    defer([erase_val]{ g_object_unref(erase_val); });

    if(!jsc_value_is_string(selector_val) || !jsc_value_is_string(host_val) || !jsc_value_is_boolean(erase_val))
    {
        return;
    }

    char* selector =  jsc_value_to_string(selector_val);
    char* host =  jsc_value_to_string(host_val);
    defer([selector]{ g_free(selector); });
    defer([host]{ g_free(host); });
    g_signal_emit(self, obj_signals[SIG_ELEMENT_PICKER_PICK], 0, jsc_value_to_boolean(erase_val), selector, host);
}

// This should add a temporary exception for the current website for the current session (until the browser is
// restarted) - this is the behavior of Mac Orion app
static void
on_handle_certificate_warning_allow_once(VisitUnsecureWebsiteDialog* dialog, gpointer user_data)
{
    orion::log_debug("%s", __func__);
    OrionWebviewTabContent* tab_content = ORION_WEBVIEW_TAB_CONTENT(user_data);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(tab_content));
    if (!win) {
        orion::log_warning("%s: could not get root main window", __func__);
        return;
    }

    // Adapted from main_window_show_certificates_dialog
    GTlsCertificate* certificate = NULL;
    GTlsCertificateFlags tls_errors = static_cast<GTlsCertificateFlags>(0);
    gboolean has_tls_info = webkit_web_view_get_tls_info(
        WEBKIT_WEB_VIEW(tab_content->content_webview), &certificate, &tls_errors);
    if (!has_tls_info) {
        certificate = orion_webkit_webview_get_last_tls_certificate(
            ORION_WEBKIT_WEBVIEW(tab_content->content_webview));
        tls_errors = orion_webkit_webview_get_last_tls_errors(
            ORION_WEBKIT_WEBVIEW(tab_content->content_webview));
    }

    if (!certificate) {
        orion::log_warning("%s: could not get TLS certificate", __func__);
        return;
    }

    WebExtensionWebViewFactory& factory = main_window_get_current_webview_factory(win);
    auto network_session = (WebKitNetworkSession*)factory.network_session();
    std::string website = orion_webkit_webview_get_current_website(ORION_WEBKIT_WEBVIEW(tab_content->content_webview));
    orion::log_debug("%s: temporarily allowing TLS certificate for website %s", __func__, website.c_str());
    webkit_network_session_allow_tls_certificate_for_host(network_session, certificate, website.c_str());
    // Automatically reload the page
    webkit_web_view_reload(WEBKIT_WEB_VIEW(tab_content->content_webview));
}

static void
on_handle_certificate_warning_allow_always(VisitUnsecureWebsiteDialog* dialog, gpointer user_data)
{
    orion::log_debug("%s", __func__);
    OrionWebviewTabContent* tab_content = ORION_WEBVIEW_TAB_CONTENT(user_data);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(tab_content));
    if (!win) {
        orion::log_warning("%s: could not get root main window", __func__);
        return;
    }
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    std::string website = orion_webkit_webview_get_current_website(ORION_WEBKIT_WEBVIEW(tab_content->content_webview));
    orion::gtk::WeakRefWrapper webview_weak(tab_content->content_webview);

    global_core_api->set_setting(
        profile_id,
        WEBSITE_SETTING(ssl_certificate_validation_setting, website),
        orion::settings::SslCertificateValidationWebsiteSetting::ignore,
        [website,webview_weak](orion::SetSettingReply<orion::settings::SslCertificateValidationWebsiteSetting> reply){
            orion::log_debug(
                "set ssl_certificate_validation_setting to ignore for website %s", website.c_str());

            auto webview_strong = webview_weak.lock();
            if (!webview_strong) return;
            OrionWebkitWebview* webview = ORION_WEBKIT_WEBVIEW(webview_strong.get());

            // Automatically reload the page
            webkit_web_view_reload(WEBKIT_WEB_VIEW(webview));
        });
}

static void
handle_certificate_warning_received(OrionWebviewTabContent* self, JSCValue* val)
{    
    if (!jsc_value_is_string(val)) {
        orion::log_warning("handle_certificate_warning_received: received non-string value");
        return;
    }

    char* action_type = jsc_value_to_string(val);
    defer([action_type]{ g_free(action_type); });

    orion::log_debug("handle_certificate_warning_received: action_type=%s", action_type);

    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!win) {
        orion::log_error("handle_certificate_warning_received: could not get root main window");
        return;
    }

    if (0 == g_strcmp0("showCertificate", action_type)) {
        main_window_show_certificates_dialog(win);
    } else if (0 == g_strcmp0("skipCertificateWarning", action_type)) {
        AdwDialog* dialog = visit_unsecure_website_dialog_new();
        g_signal_connect(dialog, "allow-once", G_CALLBACK(on_handle_certificate_warning_allow_once), self);
        g_signal_connect(dialog, "always-allow-for-this-site", G_CALLBACK(on_handle_certificate_warning_allow_always), self);
        adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(win));
    } else {
        orion::log_warning("handle_certificate_warning_received: received unknown action type: %s", action_type);
    }
}

static void
register_console_redirect_handler(OrionWebviewTabContent* tab_content)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(tab_content->content_webview);

    webkit_user_content_manager_register_script_message_handler(
        mgr,
        "console",
        NULL // world name
    );

    g_signal_connect(
        mgr, "script-message-received::console", G_CALLBACK(on_console_redirect_message), NULL
    );
}

static void
register_reader_data_handler(OrionWebviewTabContent* tab_content)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(tab_content->content_webview);


    WebKitUserScript* script = orion_app_get_script(orion::ScriptType::readability_js);
    webkit_user_content_manager_add_script(mgr, script);
    webkit_user_content_manager_register_script_message_handler(
        mgr,
        "readerData",
        READABILITY_SCRIPT_WORLD
    );

    g_signal_connect_object(
        mgr, "script-message-received::readerData", G_CALLBACK(handle_reader_data_received), tab_content, G_CONNECT_SWAPPED
    );
}

static void
register_reader_epoch_handler(OrionWebviewTabContent* tab_content)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(tab_content->content_webview);


    WebKitUserScript* script = orion_app_get_script(orion::ScriptType::readability_js);
    webkit_user_content_manager_add_script(mgr, script);
    webkit_user_content_manager_register_script_message_handler(
        mgr,
        "readerEpoch",
        READABILITY_SCRIPT_WORLD
    );

    g_signal_connect_object(
        mgr, "script-message-received::readerEpoch", G_CALLBACK(handle_reader_epoch_received), tab_content, G_CONNECT_SWAPPED
    );
}

static void
register_element_picker_handler(OrionWebviewTabContent* tab_content)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(tab_content->content_webview);


    WebKitUserScript* script = orion_app_get_script(orion::ScriptType::readability_js);
    webkit_user_content_manager_add_script(mgr, script);
    webkit_user_content_manager_register_script_message_handler(
        mgr,
        "pickerComplete",
        PAGE_TWEAKS_SCRIPT_WORLD
    );

    g_signal_connect_object(
        mgr, "script-message-received::pickerComplete", G_CALLBACK(handle_picker_complete_received), tab_content, G_CONNECT_SWAPPED
    );
}

static void
register_certificate_warning_handler(OrionWebviewTabContent* tab_content)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(tab_content->content_webview);

    WebKitUserScript* script = orion_app_get_script(orion::ScriptType::certificate_warning);
    webkit_user_content_manager_add_script(mgr, script);

    // Connect the signal before registering the script message handler to avoid certain issues
    g_signal_connect_object(
        mgr,
        "script-message-received::certificateWarning",
        G_CALLBACK(handle_certificate_warning_received),
        tab_content,
        G_CONNECT_SWAPPED
    );

    if (!webkit_user_content_manager_register_script_message_handler(
        mgr,
        "certificateWarning",
        NULL // world name - explicitly visible from all pages (same on Mac)
    )) {
        orion::log_warning("register_certificate_warning_handler: failed to register script message handler");
    }
}

static void
unregister_certificate_warning_handler(OrionWebviewTabContent* tab_content)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(tab_content->content_webview);
    webkit_user_content_manager_unregister_script_message_handler(
        mgr,
        "certificateWarning",
        NULL // world name
    );
}

static gboolean
on_web_inspector_attach(WebKitWebInspector* inspector, gpointer user_data)
{
    orion::log_verbose("on_web_inspector_attach called");
    OrionWebviewTabContent* tab_content = ORION_WEBVIEW_TAB_CONTENT(user_data);
    tab_content->web_inspector_is_visible = true;
    return FALSE; // Allow the default behavior to continue (showing the inspector)
}

static void
on_web_inspector_closed(WebKitWebInspector* inspector, gpointer user_data)
{
    orion::log_verbose("on_web_inspector_closed called");
    OrionWebviewTabContent* tab_content = ORION_WEBVIEW_TAB_CONTENT(user_data);
    tab_content->web_inspector_is_visible = false;
}

static void
register_web_inspector_handler(OrionWebviewTabContent* tab_content)
{
    WebKitWebInspector* inspector = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(tab_content->content_webview));
    g_signal_connect(inspector, "attach", G_CALLBACK(on_web_inspector_attach), tab_content);
    g_signal_connect(inspector, "closed", G_CALLBACK(on_web_inspector_closed), tab_content);
}

static void
on_form_submit_message(WebKitUserContentManager* mgr, JSCValue* message, gpointer user_data)
{
    OrionWebviewTabContent* tab_content = ORION_WEBVIEW_TAB_CONTENT(user_data);

    // extract username and password from message
    JSCValue* username_prop = jsc_value_object_get_property(message, "username");
    JSCValue* password_prop = jsc_value_object_get_property(message, "password");

    std::string username = orion::jsc::value_to_string(username_prop);
    std::string password = orion::jsc::value_to_string(password_prop);

    orion::log_debug("on_form_submit_message: form submitted: tab_id=%lld username=%s password=%s",
        tab_content->id, username.c_str(), password.c_str()
    );

    tab_content->is_login_pending = TRUE;

    const char* uri_string = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(tab_content->content_webview));
    tab_content->login_url = std::string(uri_string);
}

static void
register_form_submit_handler(OrionWebviewTabContent* tab_content)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(tab_content->content_webview);

    // Register a script message handler named "formSubmit"
    webkit_user_content_manager_register_script_message_handler(
        mgr,
        "formSubmit",
        AUTOFILL_WORLD
    );

    g_signal_connect_object(
        mgr, "script-message-received::formSubmit", G_CALLBACK(on_form_submit_message), tab_content, G_CONNECT_DEFAULT
    );
}

static void
orion_webview_tab_content_check_reader_mode(OrionWebviewTabContent* content)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(content)));
    if(!win) return;
    const char* uri_c_str = webkit_web_view_get_uri(content->content_webview); 
    if(!uri_c_str) return;
    //this uri can technically instead correspond to a page load that happened after our load, but if that is the case
    //our epoch safeguard in our script will fail, thus not actually causing any harm
    orion::Uri uri = orion::Uri::parse_strict(uri_c_str);
    orion::gtk::WeakRefWrapper content_weak(content);

    guint64 page_id = webkit_web_view_get_page_id(content->content_webview);
    orion::GetSettingForWebsiteOrDefaultQuery query{};
    query.profile_id = main_window_get_current_profile_id(win);
    query.website = uri.authority();
    query.key_name = orion::settings::website::specific::enable_reader_mode.key_name();
    global_core_api->get_setting_for_website_or_default(query, 
        [content_weak, page_id, current_epoch = content->cpp_data.current_reader_epoch](orion::GetSettingForWebsiteOrDefaultReply reply){
            VERIFY_LOG(reply.err);

            auto content_strong = content_weak.lock();
            if(!content_strong) return;
            OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(content_strong.get());

            if(current_epoch != content->cpp_data.current_reader_epoch) return;

            orion::settings::ReaderModeWebsiteSetting setting = orion::settings::ReaderModeWebsiteSetting::not_set;
            orion::settings::unmarshal_value(reply.value, &setting);

            if(setting == orion::settings::ReaderModeWebsiteSetting::on)
            {
                content->reader_mode_enabled = true;
                content->webview_is_reader_mode_enabled = true;
                g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_READER_MODE_ENABLED]);
                std::string& script_string = orion_app_get_script_string(orion::ScriptType::auto_reader);

                orion::str::PlaceholderMap map = {
                    {"READER-EPOCH", current_epoch}
                };

                std::string parsed_string = orion::str::parse_string_prepend_append_and_replace_placeholders(script_string, "", "", map, false);
                //we need to ensure that no page changes have happened in the meantime, future me problem
                webkit_web_view_evaluate_javascript(content->content_webview, parsed_string.c_str(), parsed_string.length(),
                    READABILITY_SCRIPT_WORLD, NULL, NULL, NULL, NULL);
            }
            else
            {
                content->reader_mode_enabled = false;
                g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_READER_MODE_ENABLED]);
                content->last_webview_page_was_readered = false;
                orion_webview_tab_content_hide_reader_mode_webview(content);
            }
        });
}

static void
on_webview_resource_load_started(
    WebKitWebView*, WebKitWebResource*, WebKitURIRequest* request, gpointer user_data)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(user_data);
    const char* uri = webkit_uri_request_get_uri(request);
    if (!uri)
        return;

    std::string domain = orion::registrable_domain_from_uri(uri);
    if (!domain.empty()
        && content->current_page_resource_domains.insert(std::move(domain)).second)
        g_signal_emit(content, obj_signals[SIG_CURRENT_PAGE_RESOURCES_CHANGED], 0);
}

static void
on_webview_load_changed(WebKitWebView* self, WebKitLoadEvent event, gpointer user_data)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(user_data);
    orion::TabId tab_id = content->id;

    orion::log_verbose("on_webview_load_changed: tab_id=%lld event=%d", tab_id.value, event);

    if (event == WEBKIT_LOAD_STARTED) {
        content->current_page_resource_domains.clear();
        g_signal_emit(content, obj_signals[SIG_CURRENT_PAGE_RESOURCES_CHANGED], 0);
    }

    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(content)));
    if (!main_window) {
        orion::log_verbose("on_webview_load_changed: tab content not yet parented to a window, skipping");
        return;
    }
    
    if (event == WEBKIT_LOAD_STARTED)
    {
        // Content blockers cannot be synced in WEBKIT_LOAD_COMMITTED because it is too late for that
        orion_webview_tab_content_sync_content_blockers(content);
        orion_webview_tab_content_sync_intelligent_tracking_prevention(content);
        orion_webview_tab_content_sync_enable_javascript(content);
    }
    else if (event == WEBKIT_LOAD_COMMITTED)
    {
        orion::log_verbose(
            "on_webview_load_changed: load committed: tab_id=%lld url=%s",
            tab_id.value,
            webkit_web_view_get_uri(self)
        );

        if(content->is_initial_load)
        {
            content->is_initial_load = false;
            orion_webkit_webview_ignore_next_download(ORION_WEBKIT_WEBVIEW(content->content_webview));
        }
        if(content->next_load_finished_should_show_webview)
        {
            orion_webview_tab_content_show_webview(content);
            content->next_load_finished_should_show_webview = false;
        }
        std::string new_url = std::string(webkit_web_view_get_uri(self));
        if (content->is_login_pending && content->login_url != new_url) {
            // URL changed, login probably succeeded
            orion::log_debug("on_webview_load_changed: load committed, login succeeded, new url: %s",
                new_url.c_str()
            );
            content->is_login_pending = FALSE;
            // TODO: ask user whether to save password
        }

        // This is probably the best time to synchronize zoom level for the new page (WEBKIT_LOAD_FINISHED is too late,
        // the page would flicker).
        orion_webview_tab_content_on_uri_initialized_or_changed(content);
    }
    else if (event == WEBKIT_LOAD_FINISHED)
    {
        if (content->edit_text_on_page) {
            evaluate_edit_text_on_page_script(content, content->edit_text_on_page);
        }

        if(content->next_load_finished_should_show_webview)
        {
            orion_webview_tab_content_show_webview(content);
            content->next_load_finished_should_show_webview = false;
        }

        const char* uri_string = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(self));
        orion::Uri uri = orion::Uri::parse_strict(uri_string);
        orion::log_verbose("->URI/loaded: %s", uri.to_string().c_str());
        g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_BACK]);
        g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_FORWARD]);

        orion::CreateHistoryEntryQuery history_query;
        history_query.address = uri_string;
        const char* title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(self));
        if(title) history_query.website = title;
        //history_query.profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))));
        history_query.visited_at = orion::UnixTimeUtc64(g_date_time_to_unix(g_date_time_new_now_utc()));
        
        global_core_api->get_tabs({content->id}, [query = std::move(history_query)](orion::GetTabsReply reply) mutable {
            VERIFY_LOG(reply.err);
            if(!reply.tabs.size()) return;
            global_core_api->get_windows({reply.tabs.at(0).window_id}, [query = std::move(query)](orion::GetWindowsReply reply) mutable {
                VERIFY_LOG(reply.err);
                if(!reply.windows.size()) return;
                query.profile_id = reply.windows.at(0).profile_id;
                global_core_api->create_history_entry(query, [](orion::CreateHistoryEntryReply reply){VERIFY_LOG(reply.err);});
            });
        });

        //orion_webview_tab_content_set_tab_favicon(content, content->icon);

        if (content->is_login_pending) {
            orion::log_debug("on_webview_load_changed: load finished, login failed");
            content->is_login_pending = FALSE;
        }
        orion_webview_tab_content_save_state(content);
    }

    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_BACK]);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_FORWARD]);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_URI]);
}

static uint
orion_webview_tab_content_translate_webkit_back_forward_index_to_wrapped(
    OrionWebviewTabContent* self, uint webkit_index)
{
    uint adjustment = 0;
    for(auto& custom_back_forward_list_entry : self->custom_uris_back_forward_list)
    {
        if(custom_back_forward_list_entry.webkit_index<=webkit_index+adjustment)
        {
            adjustment ++;
        }
        else
        {
            break;
        }
    }
    return webkit_index + adjustment;
}

static void
on_webview_back_forward_list_changed(WebKitBackForwardList* self,
    WebKitBackForwardListItem* item_added,
    GList* items_removed,
    gpointer user_data)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(user_data);
    GList_autoptr forward_list = webkit_back_forward_list_get_forward_list(self);
    GList_autoptr back_list = webkit_back_forward_list_get_back_list(self);
    
    uint webkit_index = g_list_length(back_list)+1;

    uint wrapped_index = orion_webview_tab_content_translate_webkit_back_forward_index_to_wrapped(content, webkit_index);

    //if item_added == NULL and items_removed==NULL, we just navigated through bf cache, in that case, move webkit index
    if(!item_added)
    {
        if(!items_removed)
        {
            orion::log_verbose("on_webview_back_forward_list_changed(): likely navigated back/forward");
        }
        else
        {
            //NOTE: not implemented because I do not know of a scenario where this should happen
            orion::log_debug("on_webview_back_forward_list_changed(): called with items_removed but not item_added,");
            orion::log_debug("unexpected, likely will break tab state for id %ld", content->id.value);
        }
    }

    //if item_added and items_removed, we either branched, pruned, or somehow both (TODO: is it possible to both branch and prune?)

    //branches should always happen on current item, so forward list should always be empty
    //but technically, the item count removed from the front should be prev_webview_forward_list_len-current_webview_forward_list_len
    
    //the remaining count of items in items_removed should be items that were dropped from the very back
    if(item_added)
    {
        if(!forward_list)
        {
            if(items_removed)
            {
                uint n_pruned_items = g_list_length(items_removed) - (content->prev_webkit_forward_list_len);
                if(n_pruned_items)
                {
                    orion::log_verbose("on_webview_back_forward_list_changed(): likely pruned %u oldest entries", n_pruned_items);
                    
                    //checks whether custom entryindex should be cut off, if so,
                    //does so and expands the range of indexes we should cut off, else downshifts
                    for(auto it = content->custom_uris_back_forward_list.begin(); it != content->custom_uris_back_forward_list.end();)
                    {
                        if(it->webkit_index <= n_pruned_items)
                        {
                            it = content->custom_uris_back_forward_list.erase(it);
                            content->back_forward_list_len--;
                            n_pruned_items++;
                        }
                        else
                        {
                            it->webkit_index -= n_pruned_items;
                            ++it;
                        }
                    }
                }
                if(content->prev_webkit_forward_list_len)
                {
                    orion::log_verbose("on_webview_back_forward_list_changed(): likely branched, dropping %u entries", content->prev_webkit_forward_list_len);
                    //drops all custom entries that have an index equal or higher to new wrapped index
                    for(auto it = content->custom_uris_back_forward_list.begin(); it != content->custom_uris_back_forward_list.end();)
                    {
                        if(it->webkit_index >= wrapped_index)
                        {
                            it = content->custom_uris_back_forward_list.erase(it);
                            content->back_forward_list_len--;
                        }
                        else
                        {
                            ++it;
                        }
                    }
                }
            }
            else
            {
                orion::log_verbose("on_webview_back_forward_list_changed(): likely loaded uri at the end of back forward list without pruning");
            }
        }
        else
        {
            orion::log_debug("on_webview_back_forward_list_changed(): added new item but forward list exists");
            orion::log_debug("unexpected, likely will break tab state for id %ld", content->id.value);
        }
    }
    
    content->webview_current_back_forward_index = wrapped_index;
    if(!content->is_on_custom_widget)
    {
        content->current_back_forward_index = wrapped_index;
    }
    content->back_forward_list_len = 1 + g_list_length(forward_list) + g_list_length(back_list) + content->custom_uris_back_forward_list.size();
    content->prev_webkit_forward_list_len = g_list_length(forward_list);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_BACK]);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_FORWARD]);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_BACK_LIST]);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_FORWARD_LIST]);
}

#endif

//endregion



//region Getters, setters

//this is called when uri of existing tab is changed to another uri
//loads new uri in webview, maintains back forward list
void
orion_webview_tab_content_load_uri(OrionWebviewTabContent* content, orion::Uri uri)
{
    orion::log_verbose("orion_webview_tab_content_load_uri: uri=%s", uri.uri().c_str());
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(!uri.uri().length())
    {
        return;
    }

    bool widget_obtained = false;
    if(uri.known_scheme() == orion::Uri::Scheme::orion)
    {
        std::string title;
        GIcon* icon = NULL;
        GtkWidget* widget = NULL;
        widget_obtained = orion::custom_uri_get_corresponding_widget(uri.uri(), &widget, &title, &icon);
        if(widget_obtained)
        {
            orion_webview_tab_content_show_widget(content, widget);
            content->current_back_forward_index++;
            content->back_forward_list_len = content->current_back_forward_index;
            content->custom_uris_back_forward_list.push_back(orion::TabBackForwardListEntryDto{
                .tab_id = content->id,
                .webkit_index = content->current_back_forward_index,
                .uri = uri.uri()
            });

            orion_webview_tab_content_set_tab_title_internal(content, title.c_str());
            if(icon)
            {
                defer([&icon]{ g_clear_object(&icon); });
                orion_webview_tab_content_set_tab_favicon(content, icon);
            }
            
            for(auto entry_it = content->custom_uris_back_forward_list.begin(); entry_it != content->custom_uris_back_forward_list.end();)
            {
                if(entry_it->webkit_index > content->back_forward_list_len)
                {
                    entry_it = content->custom_uris_back_forward_list.erase(entry_it);
                }
                else
                {
                    ++entry_it;
                }
            }
        }
    }
    

    //failed to handle as custom uri, proceeding to load in webview
    if(!widget_obtained)
    {
        //if webview is not on the index directly before the current span of custom widgets, force it there
        if(content->is_on_custom_widget && content->content_webview) //means webview current index is desynced from actual current back forward index
        {
            int64_t i = content->current_back_forward_index;
            for(; i >= 1; i--)
            {
                auto it = std::find_if(
                    content->custom_uris_back_forward_list.cbegin(),
                    content->custom_uris_back_forward_list.cend(),
                    [i](const orion::TabBackForwardListEntryDto& a){ return a.webkit_index == i; }
                );
                
                if(it == content->custom_uris_back_forward_list.cend())
                {
                    break;
                }
            }

            //i is the target index we want webview to end up on, or 0 if there is no webview item preceeding the current span of custom uris
            if(i != content->webview_current_back_forward_index)
            {
                if(i > 0)
                {
                    GList* webkit_bf_list = g_list_reverse(get_webkit_fb_glist(content));
                    defer([webkit_bf_list]{ g_list_free(webkit_bf_list); });

                    int64_t target_webkit_bf_list_index = i - std::count_if( // index starting from 1
                        content->custom_uris_back_forward_list.cbegin(),
                        content->custom_uris_back_forward_list.cend(),
                        [i](const orion::TabBackForwardListEntryDto& a){ return a.webkit_index == i; }
                    );

                    if(!(1 <= target_webkit_bf_list_index && target_webkit_bf_list_index <= g_list_length(webkit_bf_list)))
                    {
                        orion::log_debug("orion_webview_tab_content_load_uri():");
                        orion::log_error("calculated invalid webkit bf list index to move webview to for loading uri");
                        return;
                    }
                    
                    WebKitBackForwardListItem* item = WEBKIT_BACK_FORWARD_LIST_ITEM(g_list_nth_data(webkit_bf_list, target_webkit_bf_list_index - 1));
                    
                    orion_webkit_webview_ignore_next_download(ORION_WEBKIT_WEBVIEW(content->content_webview));
                    webkit_web_view_go_to_back_forward_list_item(content->content_webview, item);
                }
                else
                {
                    g_signal_handlers_disconnect_by_data(content->content_webview, content);
                    orion::gtk::widget_unparent_wrapper(content->content_webview);
                    content->content_webview = NULL;
                    for(auto it = content->custom_uris_back_forward_list.begin(); it != content->custom_uris_back_forward_list.end();)
                    {
                        if(it->webkit_index > content->current_back_forward_index)
                        {
                            it = content->custom_uris_back_forward_list.erase(it);
                        }
                        else
                        {
                            ++it;
                        }
                    }
                }
            }
        }

        if(!content->content_webview)
        {
            MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(content)));
            auto& factory = main_window_get_current_webview_factory(win);
            orion::gtk::WeakRefWrapper self_weak(content);

            factory.web_view_for_content([self_weak, uri](WebViewRef web_view){
                auto self_strong = self_weak.lock();
                if (!self_strong) return;
                OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(self_strong.get());
                WebKitWebView* content_webview = WEBKIT_WEB_VIEW(web_view.get());
                self->content_webview = content_webview;

                orion::log_verbose("orion_webview_tab_content_load_uri: loading uri: %s", uri.uri().c_str());
                self->back_forward_list_len = self->current_back_forward_index;
                orion_webview_tab_content_web_view_obtained(self, false);
                webkit_web_view_load_uri(WEBKIT_WEB_VIEW(self->content_webview), uri.uri().c_str());
                if(self->is_on_custom_widget)
                {
                    self->next_load_finished_should_show_webview = true;
                }
            });
        }
        else
        {
            webkit_web_view_load_uri(WEBKIT_WEB_VIEW(content->content_webview), uri.uri().c_str());
            if(content->is_on_custom_widget)
            {
                content->next_load_finished_should_show_webview = true;
            }
        }

        orion::log_verbose("orion_webview_tab_content_load_uri: loading uri: %s", uri.uri().c_str());
    }
#endif
}

orion::TabId
orion_webview_tab_content_get_id(OrionWebviewTabContent* content)
{
    return ORION_WEBVIEW_TAB_CONTENT(content)->id;
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
WebKitWebView*
orion_webview_tab_content_get_webview(OrionWebviewTabContent* content)
{
    return ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
}

const std::unordered_set<std::string>&
orion_webview_tab_content_get_current_page_resource_domains(OrionWebviewTabContent* content)
{
    return ORION_WEBVIEW_TAB_CONTENT(content)->current_page_resource_domains;
}
#endif

void
orion_webview_tab_content_stop_loading(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview = ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
    if (webview) {
        webkit_web_view_stop_loading(webview);
    }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

void
orion_webview_tab_content_reload_bypass_cache(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview = ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
    if (webview) {
        webkit_web_view_reload_bypass_cache(webview);
    }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

void
orion_webview_tab_content_find_search(OrionWebviewTabContent* content, const std::string& text,
    gboolean ignore_case, gboolean wrap_around, gboolean starts_with)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview = ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
    if (webview) {
        WebKitFindController* find_controller = webkit_web_view_get_find_controller(webview);


        WebKitFindOptions options = WEBKIT_FIND_OPTIONS_NONE;
        if (ignore_case) options = static_cast<WebKitFindOptions>(options | WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE);
        if (wrap_around) {
            options = static_cast<WebKitFindOptions>(options | WEBKIT_FIND_OPTIONS_WRAP_AROUND);
            content->cpp_data.find_wrap_around = true;
        } else {
            options = static_cast<WebKitFindOptions>(options | WEBKIT_FIND_OPTIONS_WRAP_AROUND);
            content->cpp_data.find_wrap_around = false;
        }
        if (starts_with) options = static_cast<WebKitFindOptions>(options | WEBKIT_FIND_OPTIONS_AT_WORD_STARTS);

        auto diacritics_stripped_text = orion::str::strip_diacritics(text);
        webkit_find_controller_search(find_controller, diacritics_stripped_text.c_str(), options, G_MAXUINT);
    }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

void
orion_webview_tab_content_find_next(OrionWebviewTabContent* content)
{

    if (content->cpp_data.find_total_matches == 0) {
        return;
    }

    if (content->cpp_data.find_current_match_index >= content->cpp_data.find_total_matches - 1) {
        if (content->cpp_data.find_wrap_around) {
            content->cpp_data.find_current_match_index = 0;
        } else {
            return;
        }
    } else {
        content->cpp_data.find_current_match_index++;
    }

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(content);
    if (self->content_webview) {
        self->find_suppress_next_result_callback = TRUE;
        webkit_find_controller_search_next(webkit_web_view_get_find_controller(self->content_webview));
    }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

void
orion_webview_tab_content_find_previous(OrionWebviewTabContent* content)
{

    if (content->cpp_data.find_total_matches == 0) {
        return;
    }

    if (content->cpp_data.find_current_match_index == 0) {
        if (content->cpp_data.find_wrap_around) {
            content->cpp_data.find_current_match_index = content->cpp_data.find_total_matches - 1;
        } else {
            return;
        }
    } else {
        content->cpp_data.find_current_match_index--;
    }


#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(content);
    if (self->content_webview) {
        self->find_suppress_next_result_callback = TRUE;
        webkit_find_controller_search_previous(webkit_web_view_get_find_controller(self->content_webview));
    }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

void
orion_webview_tab_content_find_finish(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview = ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
    if (webview) {
        webkit_find_controller_search_finish(webkit_web_view_get_find_controller(webview));
    }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

void
orion_webview_tab_content_set_find_result_callback(OrionWebviewTabContent* content, std::function<void(guint, gboolean)> callback)
{

    auto callback_wrapper = [callback,content](guint count, gboolean finished) {

    content->cpp_data.find_total_matches = finished ? count : 0;
    content->cpp_data.find_current_match_index = finished ? 0 : count;
            
    callback(count, finished);
    };

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    ORION_WEBVIEW_TAB_CONTENT(content)->find_result_callback = callback_wrapper;
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

void
orion_webview_tab_content_set_zoom(OrionWebviewTabContent* content, orion::settings::PageZoom zoom)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview = ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
    if (!webview) return;
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(content)));
    if (!win) return;
    int64_t zoom_percent = page_zoom_to_zoom_percent(zoom);
    orion::log_verbose("orion_webview_tab_content_set_zoom: zoom_percent=%d tab_id=%lld webview=%p",
        static_cast<int>(zoom_percent), content->id.value, webview
    );
    webkit_web_view_set_zoom_level(webview, zoom_percent / 100.0);
    
    if (orion_webview_tab_content_get_id(content) == main_window_get_current_tab_id(win)) {
        main_window_update_page_zoom_button(win, zoom_percent);
    }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

orion::Uri
orion_webview_tab_content_get_uri(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(content->is_on_custom_widget)
    {
        auto it = std::find_if(
            content->custom_uris_back_forward_list.cbegin(),
            content->custom_uris_back_forward_list.cend(),
            [index = content->current_back_forward_index](orion::TabBackForwardListEntryDto entry){
                return entry.webkit_index == index;
            });
        if(it == content->custom_uris_back_forward_list.cend())
        {
            return orion::Uri::parse_strict("");
        }

        return orion::Uri::parse_strict(it->uri.c_str());
    }
    else
    {
        if (content->content_webview == nullptr) {
            orion::log_verbose("orion_webview_tab_content_get_uri: content_webview is null");
            return orion::Uri::parse_strict("");
        }
        return orion::Uri::parse_strict(webkit_web_view_get_uri(content->content_webview));
    }
#else
    return orion::Uri::parse_strict("");
#endif
}

static std::string
orion_webview_tab_content_get_title_internal(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    // if(content->is_on_custom_widget)
    // {
    //     auto it = std::find_if(content->custom_uris_back_forward_list.cbegin(), content->custom_uris_back_forward_list.cend(),
    //         [index = content->current_back_forward_index](orion::TabBackForwardListEntryDto entry){return entry.webkit_index == index;});
    //     if(it == content->custom_uris_back_forward_list.cend())
    //     {
    //         return "";
    //     }

    //     return orion::custom_uri_get_corresponding_title(it->uri);
    // }
    // else
    // {
    //     if(content->content_webview)
    //     {
    //         const char* title = webkit_web_view_get_title(content->content_webview);
    //         return title?title:"";
    //     }
    //     else
    //     {
    //         orion::log_debug("orion_webview_tab_content_get_title_internal(): content->is_on_custom_widget == false but webview not constructed yet");
    //         return "";
    //     }
    // }
    return content->title;
#else
    return "Example";
#endif
}

std::string
orion_webview_tab_content_get_title(OrionWebviewTabContent* content)
{
    std::string title = orion_webview_tab_content_get_title_internal(content);
    orion::log_verbose("orion_webview_tab_content_get_title: tab_id=%lld title=%s",
        content->id.value, title.c_str()
    );
    return title;
}

//returns a strong reference to the icon or NULL if fail
GIcon*
orion_webview_tab_content_get_icon(OrionWebviewTabContent* content)
{
    g_return_val_if_fail(ORION_IS_WEBVIEW_TAB_CONTENT(content), NULL);


    if (content->is_suspended) {
        //TODO: FIXME: check memory handling with orion::gtk::convert_icon_to_texture
        GdkTexture* original_texture = orion::gtk::convert_icon_to_texture(content->icon, 64);
        if(original_texture)
        {
            defer([original_texture]{ g_object_unref(original_texture); });
            GdkTexture* modified_favicon = orion::gtk::create_transparent_grayscale_favicon(
                original_texture, orion::gtk::SUSPENDED_FAVICON_ALPHA);
            return G_ICON(modified_favicon);
        }
        else
        {
            return NULL;
        }
    } 
    else 
    {
        if(content->icon)
        {
            return g_object_ref(content->icon);
        }
        else
        {
            return NULL;
        }
    }
}

bool
orion_webview_tab_content_get_is_loading(OrionWebviewTabContent* content)
{
    return ORION_WEBVIEW_TAB_CONTENT(content)->is_loading;
}

gboolean
orion_webview_tab_content_get_is_playing_audio(OrionWebviewTabContent* content)
{
    return ORION_WEBVIEW_TAB_CONTENT(content)->is_playing_audio;
}

gboolean
orion_webview_tab_content_get_is_muted(OrionWebviewTabContent* content)
{
    return ORION_WEBVIEW_TAB_CONTENT(content)->is_muted;
}

void
orion_webview_tab_content_set_is_muted(OrionWebviewTabContent* content, gboolean muted)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview = ORION_WEBVIEW_TAB_CONTENT(content)->content_webview;
    if (webview) {
        webkit_web_view_set_is_muted(webview, muted);
    }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

orion::TlsStatus
orion_webview_tab_content_get_tls_status(OrionWebviewTabContent* content)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(content->is_on_custom_widget)
    {
        return orion::TlsStatus::local;
    }
    else
    {
        if(content->content_webview)
        {
            gint64 val = gint64(orion::TlsStatus::none);
            g_object_get(content->content_webview, "tls-status", &val, NULL);
            return orion::TlsStatus(val);
        }
        else
        {
            return orion::TlsStatus::none;
        }
    }
#else
    return orion::TlsStatus::none;
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

//endregion



//region Constructor, init and context menu
[[maybe_unused]]
static void
orion_webview_tab_content_apply_dto(OrionWebviewTabContent* content, orion::TabDto& dto)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    // g_signal_handlers_block_by_func(content->content_webview, gpointer(on_webview_uri_changed), content);
    // g_signal_handlers_block_by_func(content->content_webview, gpointer(on_webview_title_changed), content);
    // g_signal_handlers_block_by_func(content->content_webview, gpointer(on_webview_favicon_changed), content);
    // g_signal_handlers_block_by_func(content->content_webview, gpointer(on_webview_is_loading_changed), content);
    // g_signal_handlers_block_by_func(content->content_webview, gpointer(on_webview_load_changed), content);
    g_signal_handlers_block_matched(content->content_webview, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, content);
    unsigned long len = dto.session_state.size();
    if(len)
    {
        GBytes* buf = g_bytes_new(dto.session_state.data(), len);
        WebKitWebViewSessionState* state = webkit_web_view_session_state_new(buf);
        webkit_web_view_restore_session_state(content->content_webview, state);
        
        // do not load the page if the tab is suspended
        if (!content->is_suspended) {
            webkit_web_view_reload(content->content_webview);
        }

        auto it = std::find_if(
            content->custom_uris_back_forward_list.cbegin(),
            content->custom_uris_back_forward_list.cend(),
            [index = content->current_back_forward_index](orion::TabBackForwardListEntryDto entry){
                return entry.webkit_index == index;
            });
        if(it == content->custom_uris_back_forward_list.cend())
        {
            content->is_on_custom_widget = false;
            g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_IS_READERABLE]);
            gtk_widget_queue_resize(GTK_WIDGET(content));
        }
        else
        {
            orion::Uri uri = orion::Uri::parse_strict(it->uri.c_str());
            std::string title;
            GtkWidget* widget = NULL;
            GIcon* icon = NULL;
            if(orion::custom_uri_get_corresponding_widget(uri.uri(), &widget, &title, &icon))
            {
                orion_webview_tab_content_show_widget(content, widget);
                orion_webview_tab_content_set_tab_title_internal(content, title.c_str());
                if(icon)
                {
                    defer([&icon]{ g_clear_object(&icon); });
                    orion_webview_tab_content_set_tab_favicon(content, icon);
                }
            }
        }
        WebKitBackForwardList* list = webkit_web_view_get_back_forward_list(content->content_webview);
        content->back_forward_list_len = webkit_back_forward_list_get_length(list) + content->custom_uris_back_forward_list.size();
    }


    else if(dto.uri.size())
    {
        orion::Uri uri = orion::Uri::parse_strict(dto.uri.c_str());
        bool flag = true;
        if(uri.known_scheme() == orion::Uri::Scheme::orion)
        {
            std::string title;
            GtkWidget* widget = NULL;
            GIcon* icon = NULL;
            if(orion::custom_uri_get_corresponding_widget(uri.uri(), &widget, &title, &icon))
            {
                webkit_web_view_stop_loading(content->content_webview);

                content->is_on_custom_widget = true;
                g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_IS_READERABLE]);
                content->current_back_forward_index = 1;
                content->back_forward_list_len = content->current_back_forward_index;
                content->custom_uris_back_forward_list.push_back(orion::TabBackForwardListEntryDto{
                    .tab_id = content->id,
                    .webkit_index = content->current_back_forward_index,
                    .uri = uri.uri()
                });
                flag=false;

                orion_webview_tab_content_show_widget(content, widget);
                orion_webview_tab_content_set_tab_title_internal(content, title.c_str());
                if(icon)
                {
                    defer([&icon]{ g_clear_object(&icon); });
                    orion_webview_tab_content_set_tab_favicon(content, icon);
                }
            }
        }
        if(flag)
        {
            if(content->webview_current_back_forward_index==content->current_back_forward_index+1)
            {
                orion_webkit_webview_ignore_next_download(ORION_WEBKIT_WEBVIEW(content->content_webview));
                webkit_web_view_go_back(content->content_webview);
            }
            orion::log_verbose("orion_webview_tab_content_apply_dto(): loading uri: %s", uri.uri().c_str());
            //orion_webview_tab_content_show_webview(content);
            content->is_on_custom_widget = false;
            g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_IS_READERABLE]);
            gtk_widget_queue_resize(GTK_WIDGET(content));
            webkit_web_view_load_uri(WEBKIT_WEB_VIEW(content->content_webview), uri.uri().c_str());
        }
    }

    //TODO: implement properly
    
    // g_signal_handlers_unblock_by_func(content->content_webview, gpointer(on_webview_uri_changed), content);
    // g_signal_handlers_unblock_by_func(content->content_webview, gpointer(on_webview_title_changed), content);
    // g_signal_handlers_unblock_by_func(content->content_webview, gpointer(on_webview_favicon_changed), content);
    // g_signal_handlers_unblock_by_func(content->content_webview, gpointer(on_webview_is_loading_changed), content);
    // g_signal_handlers_unblock_by_func(content->content_webview, gpointer(on_webview_load_changed), content);
    g_signal_handlers_unblock_matched(content->content_webview, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, content);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_BACK]);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_CAN_GO_FORWARD]);
    orion_webview_tab_content_save_state(content);
    orion_webview_tab_content_on_uri_initialized_or_changed(content);
#else
    (void)content;
    (void)dto;
#endif
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static void
orion_webview_tab_content_web_view_constructed(OrionWebviewTabContent* content)
{
    if(content->uri) {
        auto& uri = content->uri.value();
        if(strlen(uri.uri().c_str())) orion_webview_tab_content_load_uri(content, uri);
    }

    if(content->dto) {
        content->is_initial_load = true;
        auto& dto = content->dto.value();
        orion_webview_tab_content_apply_dto(content, dto);
    }
}
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

OrionWebviewTabContent*
orion_webview_tab_content_new_from_dto(
    orion::TabDto dto, std::vector<orion::TabBackForwardListEntryDto> back_forward_list)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(g_object_new(ORION_WEBVIEW_TAB_CONTENT_TYPE, NULL));
    content->dto.emplace(dto);
    content->custom_uris_back_forward_list = std::move(back_forward_list);
    content->back_forward_list_len = dto.merged_back_forward_list_length;
    content->current_back_forward_index = dto.back_forward_list_index;
    content->webview_current_back_forward_index = dto.webkit_back_forward_list_index;
    orion_webview_tab_content_set_tab_title_internal(content, dto.name.c_str());

    content->id = dto.id;

    return content;
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static void
orion_webview_tab_content_web_view_obtained(OrionWebviewTabContent* self, bool run_constructed)
{
    gtk_widget_set_parent(GTK_WIDGET(self->content_webview), GTK_WIDGET(self));
    orion_webkit_webview_set_page_tweaks_enabled(ORION_WEBKIT_WEBVIEW(self->content_webview), TRUE);

    g_signal_connect_object(
        self->content_webview, "notify::uri", G_CALLBACK(on_webview_uri_changed), self, G_CONNECT_DEFAULT
    );
    g_signal_connect_object(
        self->content_webview, "notify::title", G_CALLBACK(on_webview_title_changed), self, G_CONNECT_DEFAULT
    );
    g_signal_connect_object(
        self->content_webview, "notify::favicon", G_CALLBACK(on_webview_favicon_changed), self, G_CONNECT_DEFAULT
    );
    g_signal_connect_object(
        self->content_webview, "notify::is-loading", G_CALLBACK(on_webview_is_loading_changed), self, G_CONNECT_DEFAULT
    );
    g_signal_connect_object(
        self->content_webview, "notify::is-playing-audio", G_CALLBACK(on_webview_is_playing_audio_changed), self, G_CONNECT_DEFAULT
    );
    g_signal_connect_object(
        self->content_webview, "notify::is-muted", G_CALLBACK(on_webview_is_muted_changed), self, G_CONNECT_DEFAULT
    );
    g_signal_connect_object(
        self->content_webview, "notify::tls-status", G_CALLBACK(notify_tls_status), self, G_CONNECT_SWAPPED
    );
    g_signal_connect_object(
        self->content_webview, "load-changed", G_CALLBACK (on_webview_load_changed), self, G_CONNECT_DEFAULT
    );
    g_signal_connect_object(
        self->content_webview, "resource-load-started",
        G_CALLBACK(on_webview_resource_load_started), self, G_CONNECT_DEFAULT
    );
    g_signal_connect_object(
        webkit_web_view_get_back_forward_list(self->content_webview), "changed",
        G_CALLBACK(on_webview_back_forward_list_changed), self, G_CONNECT_DEFAULT
    );
    g_signal_connect_object(
        webkit_web_view_get_find_controller(self->content_webview), "found-text",
        G_CALLBACK(on_find_controller_found_text), self, G_CONNECT_DEFAULT
    );
    g_signal_connect_object(
        webkit_web_view_get_find_controller(self->content_webview), "failed-to-find-text",
        G_CALLBACK(on_find_controller_failed_to_find_text), self, G_CONNECT_DEFAULT
    );

    //TODO: implement these
    //g_signal_connect(content_webview, "mouse-target-changed", G_CALLBACK(on_webview_mouse_target_changed), self);
    //TODO: use context-menu signal to modify the back 


    register_autofill_input_focus_handler(self);
    register_form_submit_handler(self);
    register_console_redirect_handler(self); // not used at the moment
    register_reader_data_handler(self);
    register_reader_epoch_handler(self);
    register_element_picker_handler(self);
    register_web_inspector_handler(self);
    
    // This is a solution that is similar to Mac Orion, but it is not ideal. It exposes CertificateWarningHandler
    // to all untrusted pages and that can be abused. We could rework this in the future by using Firefox approach
    // that shows certificate chain info in a separate tab page using special URL scheme, and that does not require
    // any native dialog when the user wants to add an exception. So basically using a custom (security) origin
    // is a better approach (but we still want to avoid changing the URL in address entry in this case and we
    // want to avoid back/forward list modification). This declarative approach is probably ideal.
    // Another possible approach is to make the handler registration/deregistration dynamic, so that it is registered
    // only when out error page is loaded and deregistered when the user navigates away from it. But that can still
    // expose the handler to untrusted pages.
    register_certificate_warning_handler(self);

    inject_script(self->content_webview, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END, orion::gtk::get_autofill_script(), AUTOFILL_WORLD);
    inject_script(self->content_webview, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END, orion::gtk::get_input_focus_script(), AUTOFILL_WORLD);
    inject_script(self->content_webview, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END, orion::gtk::get_form_submit_script(), AUTOFILL_WORLD);
    inject_script(self->content_webview, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, orion::gtk::get_console_redirect_script(), CONSOLE_REDIRECT_WORLD);
    inject_script(self->content_webview, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END, orion::gtk::get_reader_epoch_script("readerEpoch"), READABILITY_SCRIPT_WORLD);

    g_object_bind_property(self, "can-go-back", self->content_webview, "can-go-back", G_BINDING_SYNC_CREATE);
    g_object_bind_property(self, "can-go-forward", self->content_webview, "can-go-forward", G_BINDING_SYNC_CREATE);

    g_signal_connect_data(self->content_webview, "go-back", G_CALLBACK(orion_webview_tab_content_go_back), self, NULL, G_CONNECT_SWAPPED);
    g_signal_connect_data(self->content_webview, "go-forward", G_CALLBACK(orion_webview_tab_content_go_forward), self, NULL, G_CONNECT_SWAPPED);
    
    if(run_constructed)
    {
        orion_webview_tab_content_web_view_constructed(self);
    }

    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_CAN_GO_BACK]);
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_CAN_GO_FORWARD]);
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_URI]);

    self->is_playing_audio = webkit_web_view_is_playing_audio(self->content_webview);
    self->is_muted = webkit_web_view_get_is_muted(self->content_webview);
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_IS_PLAYING_AUDIO]);
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_IS_MUTED]);
    notify_tls_status(self);
}
#endif

void
orion_webview_tab_content_add_blocked_popup(OrionWebviewTabContent* content, const std::string& url)
{
    content->blocked_popup_urls.push_back(url);
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_BLOCKED_POPUPS_COUNT]);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(content)));
    if (win && orion_webview_tab_content_get_id(content) == main_window_get_current_tab_id(win))
        main_window_update_popup_blocked_button(win, (guint)content->blocked_popup_urls.size());
}

const std::vector<std::string>&
orion_webview_tab_content_get_blocked_popup_urls(OrionWebviewTabContent* content)
{
    return content->blocked_popup_urls;
}

void
orion_webview_tab_content_clear_blocked_popups(OrionWebviewTabContent* content)
{
    if (content->blocked_popup_urls.empty()) return;
    content->blocked_popup_urls.clear();
    g_object_notify_by_pspec(G_OBJECT(content), obj_properties[PROP_BLOCKED_POPUPS_COUNT]);
}

void
orion_webview_tab_content_set_edit_text_on_page(OrionWebviewTabContent* self, bool edit_text_on_page)
{
    self->edit_text_on_page = edit_text_on_page;
    orion::log_debug("inject edit-text-on-page script: tab=%lld enable=%d", self->id.value, edit_text_on_page);
    evaluate_edit_text_on_page_script(self, edit_text_on_page);
}

bool
orion_webview_tab_content_get_edit_text_on_page(OrionWebviewTabContent* self)
{
    return self->edit_text_on_page;
}

void
orion_webview_tab_content_force_allow_copy_and_paste(OrionWebviewTabContent* self)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    orion_webview_tab_content_evaluate_javascript(
        self,
        orion::scripts::get_allow_copy_and_paste_script().c_str(),
        -1,
        NULL, // world_name - will not work if custom name is used
        NULL, // source_uri
        NULL, // cancellable
        NULL, // callback
        NULL  // data
    );
#endif
}

void
orion_webview_tab_content_start_element_picker(OrionWebviewTabContent* self)
{
    //NOTE: race conditions hopefully avoided, as the element picker element destroys itself upon any events signalling navigation/hiding of that page
    //meaning even if end runs against a newer document than we want, we should be fine
    //there is also only a single EraseElementsBar, meaning that we should never produce a second start_element_picker call before the previous
    //operation's end_element_picker, and the ipc read in a single webprocess is synchronous, 
    //meaning the attempted end of a previous operation should happen prior to a new start,
    //plus we keep track of element picker running state using a local variable here anyway 
    if(self->element_picker_running) return;
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    std::string& script = orion_app_get_script_string(orion::ScriptType::element_picker);
    orion_webview_tab_content_evaluate_javascript(
        self,
        script.c_str(),
        script.length(),
        PAGE_TWEAKS_SCRIPT_WORLD,
        NULL,
        NULL,
        NULL,
        NULL
    );
#endif
    self->element_picker_running = true;
}

void
orion_webview_tab_content_end_element_picker(OrionWebviewTabContent* self)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    const std::string script = R"(
        if(window.__orion_element_picker) {
            window.__orion_element_picker.remove();
            delete window.__orion_element_picker;
        }
    )";
    orion_webview_tab_content_evaluate_javascript(
        self,
        script.c_str(),
        script.length(),
        PAGE_TWEAKS_SCRIPT_WORLD,
        NULL,
        NULL,
        NULL,
        NULL
    );
#endif
}

void
orion_webview_tab_content_set_element_picker_already_erased_selectors(OrionWebviewTabContent* self, std::string_view selectors)
{
    if(!self->element_picker_running)
    {
        orion::log_error("orion_webview_tab_content_set_element_picker_already_erased_selectors(): no element picker is active currently");
        return;
    }

}

void
orion_webview_tab_content_set_preview_erased(OrionWebviewTabContent* self, std::string_view selectors, std::string_view host, gboolean value)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(!self->element_picker_running && value)
    {
        return;
    }
    if(self->content_webview)
    {
        orion_webkit_webview_set_preview_erased(ORION_WEBKIT_WEBVIEW(self->content_webview), selectors, host, value);
    }
#endif
}

void
orion_webview_tab_content_update_preview_erased_selectors(OrionWebviewTabContent* self, std::string_view selectors)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(!self->element_picker_running)
    {
        return;
    }
    if(self->content_webview)
    {
        orion_webkit_webview_update_preview_erased_selectors(ORION_WEBKIT_WEBVIEW(self->content_webview), selectors);
    }
#endif
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
OrionWebviewTabContent*
orion_webview_tab_content_new_from_webview(orion::TabId tab_id, OrionWebkitWebview* source)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(g_object_new(ORION_WEBVIEW_TAB_CONTENT_TYPE, NULL));
    content->id = tab_id;
    content->already_initialized = true;
    content->content_webview = WEBKIT_WEB_VIEW(source);

    WebKitBackForwardList* bf_list = webkit_web_view_get_back_forward_list(content->content_webview);
    GList* back_list = webkit_back_forward_list_get_back_list(bf_list);
    uint current_back_forward_index = g_list_length(back_list);
    g_list_free(back_list);

    content->back_forward_list_len = webkit_back_forward_list_get_length(bf_list);
    content->current_back_forward_index = current_back_forward_index + 1; //index starts from 1
    content->webview_current_back_forward_index = content->current_back_forward_index;
    
    orion_webview_tab_content_web_view_obtained(content);
    orion_webview_tab_content_show_webview(content);
    orion_webview_tab_content_save_state(content);
    return content;
}
#endif

static void
orion_webview_tab_content_dispose(GObject* gobject)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(gobject);
    G_OBJECT_CLASS(orion_webview_tab_content_parent_class)->dispose(gobject);
    if(content->icon)
    {
        g_clear_object(&content->icon);
    }
    g_clear_pointer(&content->current_widget, orion::gtk::widget_unparent_wrapper);
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    g_clear_pointer(&content->content_webview, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&content->reader_mode_webview, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&content->reader_template_script_watch, gtk_expression_watch_unwatch);
#endif
    g_clear_pointer(&content->reader_font_size_index_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&content->reader_font_name_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&content->reader_text_alignment_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&content->reader_background_type_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&content->reader_border_shadow_enabled_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&content->reader_document_width_watch, gtk_expression_watch_unwatch);
}

static void
orion_webview_tab_content_finalize(GObject* gobject)
{
    orion::log_verbose("orion_webview_tab_content_finalize");

    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(gobject);

    global_core_api->single_setting_change().clear_observer(&self->show_favicons_inside_tabs_observer);
    global_core_api->single_setting_change().clear_observer(&self->always_show_website_titles_in_tabs_observer);
    global_core_api->settings_change().clear_observer(&self->settings_observer);


#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    g_clear_pointer(&self->prev_reader_init_script, webkit_user_script_unref);
    g_clear_pointer(&self->prev_reader_template_script, webkit_user_script_unref);
    std::destroy_at(&self->current_page_resource_domains);
#endif

    std::destroy_at(&self->id);
    std::destroy_at(&self->dto);
    std::destroy_at(&self->uri);
    std::destroy_at(&self->custom_uris_back_forward_list);
    std::destroy_at(&self->show_favicons_inside_tabs_observer);
    std::destroy_at(&self->always_show_website_titles_in_tabs_observer);
    std::destroy_at(&self->title);
    std::destroy_at(&self->login_url);
    std::destroy_at(&self->blocked_popup_urls);
    std::destroy_at(&self->settings_observer);

    std::destroy_at(&self->cpp_data);

    G_OBJECT_CLASS(orion_webview_tab_content_parent_class)->finalize(gobject);
}

static void
orion_webview_tab_content_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    orion::log_verbose("orion_webview_tab_content_set_property: property_id=%u", property_id);
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(object);

    switch (property_id)
    {
        case PROP_URI:
            orion_webview_tab_content_load_uri(self, orion::Uri::parse_user_input(g_value_get_string(value)));
            break;
        case PROP_IS_READERABLE:
            self->is_readerable = g_value_get_boolean(value);
            g_object_notify_by_pspec(object, pspec);
            break;
        case PROP_IS_MUTED:
            orion_webview_tab_content_set_is_muted(self, g_value_get_boolean(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
orion_webview_tab_content_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    orion::log_verbose("orion_webview_tab_content_get_property: property_id=%u", property_id);
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(object);

    switch (property_id)
    {
        case PROP_ID:
            g_value_set_int64(value, orion_webview_tab_content_get_id(self).value);
            break;
        case PROP_TAB_TITLE:
            g_value_set_string(value, orion_webview_tab_content_get_title(self).c_str());
            break;
        case PROP_ICON:
            g_value_take_object(value, orion_webview_tab_content_get_icon(self));
            break;
        case PROP_IS_LOADING:
            g_value_set_boolean(value, orion_webview_tab_content_get_is_loading(self));
            break;
        case PROP_URI:
            g_value_set_string(value, orion_webview_tab_content_get_uri(self).uri().c_str());
            break;
        case PROP_CAN_GO_BACK:
            g_value_set_boolean(value, orion_webview_tab_content_can_go_back(self));
            break;
        case PROP_CAN_GO_FORWARD:
            g_value_set_boolean(value, orion_webview_tab_content_can_go_forward(self));
            break;
        case PROP_BLOCKED_POPUPS_COUNT:
            g_value_set_uint(value, (guint)self->blocked_popup_urls.size());
            break;
        case PROP_IS_READERABLE:
            g_value_set_boolean(value, self->is_on_custom_widget?false:self->is_readerable);
            break;
        case PROP_READER_MODE_ENABLED:
            g_value_set_boolean(value, self->reader_mode_enabled);
            break;
        case PROP_IS_PLAYING_AUDIO:
            g_value_set_boolean(value, orion_webview_tab_content_get_is_playing_audio(self));
            break;
        case PROP_IS_MUTED:
            g_value_set_boolean(value, orion_webview_tab_content_get_is_muted(self));
            break;
        case PROP_TLS_STATUS:
            g_value_set_int64(value, gint64(orion_webview_tab_content_get_tls_status(self)));
            break;
        case PROP_BACK_LIST:
            g_value_take_boxed(value, orion_webview_tab_content_get_back_list(self));
            break;
        case PROP_FORWARD_LIST:
            g_value_take_boxed(value, orion_webview_tab_content_get_forward_list(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}




static void
orion_webview_tab_content_unmap(GtkWidget* widget)
{
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(widget);
    if(self->element_picker_running)
    {
        orion_webview_tab_content_end_element_picker(self);
    }
    g_signal_emit(widget, obj_signals[SIG_ELEMENT_PICKER_FAIL], 0);
    GTK_WIDGET_CLASS(orion_webview_tab_content_parent_class)->unmap(widget);
}

static void
orion_webview_tab_content_attached(GtkWidget* widget)
{
    orion::log_verbose("orion_webview_tab_content_attached");

    GTK_WIDGET_CLASS(orion_webview_tab_content_parent_class)->root(widget);
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(widget);

    if(self->already_initialized) return;
    
    WebExtensionWebViewFactory& factory = main_window_get_current_webview_factory(
        MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))));
    orion::gtk::WeakRefWrapper self_weak(self);
    factory.web_view_for_content([self_weak](WebViewRef web_view){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(self_strong.get());

        WebKitWebView* content_webview = WEBKIT_WEB_VIEW(web_view.get());
        self->content_webview = content_webview;

        orion_webview_tab_content_web_view_obtained(self);
    });
    self->already_initialized = true;

    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

static void
orion_webview_tab_content_apply_new_reader_mode_setting(OrionWebviewTabContent* self, orion::settings::ReaderModeWebsiteSetting value)
{
    if(value == orion::settings::ReaderModeWebsiteSetting::on)
    {
        self->reader_mode_enabled = true;
        g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_READER_MODE_ENABLED]);
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        std::string& script_string = orion_app_get_script_string(orion::ScriptType::auto_reader);

        orion::str::PlaceholderMap map = {
            {"READER-EPOCH", self->cpp_data.current_reader_epoch}
        };

        std::string parsed_string = orion::str::parse_string_prepend_append_and_replace_placeholders(script_string, "", "", map, false);
        webkit_web_view_evaluate_javascript(self->content_webview, parsed_string.c_str(), parsed_string.length(),
            READABILITY_SCRIPT_WORLD, NULL, NULL, NULL, NULL);
#endif
    }
    else
    {
        self->reader_mode_enabled = false;
        g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_READER_MODE_ENABLED]);
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        orion_webview_tab_content_hide_reader_mode_webview(self);
#endif
    }
}

static std::string
figures_update_string(const std::string& for_property)
{
    return std::format(R"(
        var figures = document.getElementsByTagName('figure');
        for (var i = 0; i < figures.length; i++) {{
            figures[i].style.{} = document.body.style.{};
        }}
    )", for_property, for_property);
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static void
reader_font_size_index_val_cb(gpointer data)
{
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(data);
    if(!self->reader_mode_webview) return;

    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_VARIANT);
    if(!gtk_expression_watch_evaluate(self->reader_font_size_index_watch, &val)) return;

    GVariant* var = g_value_get_variant(&val);
    int64_t index = std::clamp(g_variant_get_int64(var), 0l, 11l);

    std::string setter_script = std::format(R"(
        reader.setCurrentTextSizeIndex({});
        reader.layOutContent();
    )", index);
    
    webkit_web_view_evaluate_javascript(self->reader_mode_webview, setter_script.c_str(), setter_script.length(), READABILITY_SCRIPT_WORLD, NULL, NULL, NULL, NULL);

    g_value_unset(&val);
}

static void
reader_font_name_val_cb(gpointer data)
{
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(data);
    if(!self->reader_mode_webview) return;

    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_VARIANT);
    if(!gtk_expression_watch_evaluate(self->reader_font_name_watch, &val)) return;

    GVariant* var = g_value_get_variant(&val);
    int64_t raw_value = g_variant_get_int64(var);

    orion::settings::Font new_font_name = orion::settings::Font(raw_value);
    
    std::string script = std::format(R"(
        document.body.classList.remove('{}');
        document.body.classList.add('{}');
        document.body.style.fontFamily = '{}';
        
        {}
    )", orion::font_name_to_css_class_name(self->cpp_data.prev_font_name), orion::font_name_to_css_class_name(new_font_name),
        orion::font_name_to_css_font_family(new_font_name), figures_update_string("fontFamily"));

    webkit_web_view_evaluate_javascript(self->reader_mode_webview, script.c_str(), script.length(), READABILITY_SCRIPT_WORLD, NULL, NULL, NULL, NULL);

    self->cpp_data.prev_font_name = new_font_name; //sadly cannot avoid a race condition but it should be unlikely to trigger and makes for an only mild visual issue

    g_value_unset(&val);
}

static void
reader_text_alignment_val_cb(gpointer data)
{
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(data);
    if(!self->reader_mode_webview) return;

    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_VARIANT);
    if(!gtk_expression_watch_evaluate(self->reader_text_alignment_watch, &val)) return;

    GVariant* var = g_value_get_variant(&val);
    int64_t raw_value = g_variant_get_int64(var);

    std::string text_alignment = text_alignment_to_css(orion::settings::TextAlignment(raw_value));

    std::string script = std::format(R"(
        document.body.style.textAlign = '{}';
        
        {}
    )", text_alignment, figures_update_string("textAlign"));

    webkit_web_view_evaluate_javascript(self->reader_mode_webview, script.c_str(), script.length(), READABILITY_SCRIPT_WORLD, NULL, NULL, NULL, NULL);

    g_value_unset(&val);
}

static void
reader_background_type_val_cb(gpointer data)
{
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(data);
    if(!self->reader_mode_webview) return;

    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_VARIANT);
    if(!gtk_expression_watch_evaluate(self->reader_background_type_watch, &val)) return;

    GVariant* var = g_value_get_variant(&val);
    int64_t raw_value = g_variant_get_int64(var);
    auto new_type = orion::settings::ReaderModeBackgroundType(raw_value);

    std::string script;
    
    for(auto type : enchantum::values<orion::settings::ReaderModeBackgroundType>)
    {
        if(type != new_type)
        {
            script.append(std::format("document.body.classList.remove('{}')\n", background_type_to_css(type)));
        }
    }
    script.append(std::format("document.body.classList.add('{}')\n", background_type_to_css(new_type)));
    script.append(R"(
        document.getElementById('h1-title').style.color = reader.bodyFontColor();
        document.getElementsByClassName('metadata singleline')[0].style.color = reader.bodyFontColor();
        document.querySelectorAll("table").forEach(item => item.style.color = reader.bodyFontColor());
        document.getElementById("article").querySelectorAll(".page, .metadata a[href]").forEach(item => item.style.color = reader.bodyFontColor());
    )");

    webkit_web_view_evaluate_javascript(self->reader_mode_webview, script.c_str(), script.length(), READABILITY_SCRIPT_WORLD, NULL, NULL, NULL, NULL);

    g_value_unset(&val);
}

static void
reader_border_shadow_enabled_val_cb(gpointer data)
{
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(data);
    if(!self->reader_mode_webview) return;

    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_VARIANT);
    if(!gtk_expression_watch_evaluate(self->reader_border_shadow_enabled_watch, &val)) return;

    GVariant* var = g_value_get_variant(&val);
    int64_t raw_value = g_variant_get_int64(var);
    auto enum_value = orion::settings::ReaderModeBorderShadowEnabled(raw_value);
    bool enabled = (enum_value == orion::settings::ReaderModeBorderShadowEnabled::on);

    orion::CssClass paper_shadow_info = reader_shadow_enabled_to_paper_shadow_css(enum_value);
    orion::CssClass background_colors_info = reader_shadow_enabled_to_background_color_css(enum_value);
    
    std::string script = std::format(R"(
        function removeBlock(id) {{
            var style = document.getElementById(id);
            if (style != null) {{
                style.remove();
            }}
        }}
        removeBlock('{}');
        removeBlock('{}');
    
        function addStyle(id, css) {{
            var head = document.head || document.getElementsByTagName('head')[0],
                style = document.createElement('style');

            style.type = 'text/css';
            style.appendChild(document.createTextNode(css));
            style.id = id;
            head.appendChild(style);
        }}
        {}
        addStyle('{}', `{}`);
    )", paper_shadow_info.id(), background_colors_info.id(), (enabled ? std::format("addStyle('{}', `{}`)", paper_shadow_info.id(), paper_shadow_info.css()) : ""),
        background_colors_info.id(), background_colors_info.css());

    webkit_web_view_evaluate_javascript(self->reader_mode_webview, script.c_str(), script.length(), READABILITY_SCRIPT_WORLD, NULL, NULL, NULL, NULL);

    g_value_unset(&val);
}

static void
reader_document_width_val_cb(gpointer data)
{
    OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(data);
    if(!self->reader_mode_webview) return;

    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_VARIANT);
    if(!gtk_expression_watch_evaluate(self->reader_document_width_watch, &val)) return;

    GVariant* var = g_value_get_variant(&val);
    int64_t raw_value = g_variant_get_int64(var);

    std::string script = orion::reader_mode_width_script(raw_value);

    webkit_web_view_evaluate_javascript(self->reader_mode_webview, script.c_str(), script.length(), READABILITY_SCRIPT_WORLD, NULL, NULL, NULL, NULL);

    g_value_unset(&val);
}
#endif

static void
orion_webview_tab_content_register_observers(OrionWebviewTabContent* self)
{
    orion::gtk::WeakRefWrapper self_weak(self);

    if (self->settings_observer.value) {
        orion::log_warning(
            "orion_webview_tab_content_register_observers: settings_observer already has a value, likely a bug");
    }

    // Page zoom per-website setting observer is needed to keep zoom level updated
    // when user changes it from Website Settings dialog
    self->settings_observer = global_core_api->settings_change().add_observer(
    [self_weak](const orion::SettingsChange& change){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        OrionWebviewTabContent* self = ORION_WEBVIEW_TAB_CONTENT(self_strong.get());
        GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(self));
        if (!root || !MAIN_IS_WINDOW(root)) {
            orion::log_warning(
                "orion_webview_tab_content_register_observers: root is null or not a MainWindow");
            return;
        }
        MainWindow* win = MAIN_WINDOW(root);

        orion::Uri current_uri = orion_webview_tab_content_get_uri(self);
        std::string website = current_uri.authority();

        for (const auto& setting : change.updated) {
            if (setting.key == WEBSITE_SETTING(page_zoom, website).key()) {
                orion_webview_tab_content_sync_webview_zoom_level(self);
            } else if (setting.key == orion::settings::websites::page_zoom_default_setting_for_all_websites.key()) {
                orion_webview_tab_content_sync_webview_zoom_level(self);
            } else if (setting.key == WEBSITE_SETTING(enable_content_blockers, website).key()) {
                bool new_value = false;
                VERIFY_LOG(orion::settings::unmarshal_value(setting.new_value, &new_value));
                orion_webview_tab_content_set_enable_content_blockers(self, new_value, true);
            } else if (setting.key == WEBSITE_SETTING(enable_intelligent_tracking_prevention, website).key()) {
                bool new_value = false;
                VERIFY_LOG(orion::settings::unmarshal_value(setting.new_value, &new_value));
                orion_webview_tab_content_set_enable_intelligent_tracking_prevention(self, new_value);
            } else if (setting.key == WEBSITE_SETTING(block_web_fonts, website).key()) {
                bool new_value = false;
                VERIFY_LOG(orion::settings::unmarshal_value(setting.new_value, &new_value));
                orion_webview_tab_content_set_block_web_fonts(self, new_value);
            } else if (setting.key == WEBSITE_SETTING(enable_javascript, website).key()) {
                bool new_value = false;
                VERIFY_LOG(orion::settings::unmarshal_value(setting.new_value, &new_value));
                orion_webview_tab_content_set_enable_javascript(self, new_value);
            } else if (setting.key == WEBSITE_SETTING(enable_cookies, website).key()) {
                bool new_value = false;
                VERIFY_LOG(orion::settings::unmarshal_value(setting.new_value, &new_value));
                // Possible improvement: call this only once per settings change, not for each webview tab content
                orion_webview_tab_content_set_enable_cookies(self, new_value);
            } else if (setting.key == WEBSITE_SETTING(autoplay_default_setting, website).key()) {
                // Per-site autoplay setting for this domain changed -> reload
                orion_webview_tab_content_reload(self);
            } else if (setting.key == orion::settings::websites::autoplay_default_setting_for_all_websites.key()) {
                // Global autoplay default changed -> reload only if this tab uses global default (per-site is not_set)
                orion_webview_tab_reload_on_default_autoplay_setting_change(self, website);
                
            } else if (setting.key == WEBSITE_SETTING(user_agent, website).key()) {
                // Per-site UA for this domain changed → always reload
                orion_webview_tab_content_reload(self);
            } else if (setting.key == orion::settings::websites::user_agent_default_setting_for_all_websites.key()) {
                // Default UA changed → reload only if this tab is using the default (per-site is automatic)
                orion_webview_tab_reload_on_default_user_agent_setting_change(self, website);
            } else if (setting.key == orion::settings::privacy::custom_user_agent.key()) {
                // Custom UA string changed → reload only if this tab is effectively using custom UA
                orion_webview_tab_reload_on_custom_user_agent_setting_change(self, website);
            } else if (setting.key == WEBSITE_SETTING(enable_reader_mode, website).key()) {
                orion::settings::ReaderModeWebsiteSetting new_value = orion::settings::ReaderModeWebsiteSetting::not_set;
                VERIFY_LOG(orion::settings::unmarshal_value(setting.new_value, &new_value));
                orion::log_verbose("reader mode setting value: %s", setting.new_value.c_str());
                orion_webview_tab_content_apply_new_reader_mode_setting(self, new_value);
            } else if (setting.key == orion::settings::websites::enable_reader_mode_default_setting_for_all_websites.key()) {
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
                orion_webview_tab_content_check_reader_mode(self);
#endif
            }
        }
    });

    {
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        GtkExpression* root_expression = gtk_property_expression_new(GTK_TYPE_WIDGET, NULL, "root");

        GtkExpression* font_size_index_action_expression = 
            orion::gtk::action_expression_new<"reader-mode-font-size-index">(gtk_expression_ref(root_expression));
        GtkExpression* font_name_action_expression = 
            orion::gtk::action_expression_new<"reader-mode-font-name">(gtk_expression_ref(root_expression));
        GtkExpression* text_alignment_action_expression = 
            orion::gtk::action_expression_new<"reader-mode-text-alignment">(gtk_expression_ref(root_expression));
        GtkExpression* background_type_action_expression = 
            orion::gtk::action_expression_new<"reader-mode-background-type">(gtk_expression_ref(root_expression));
        GtkExpression* border_shadow_enabled_action_expression = 
            orion::gtk::action_expression_new<"reader-mode-border-shadow-enabled">(gtk_expression_ref(root_expression));
        GtkExpression* document_width_action_expression = 
            orion::gtk::action_expression_new<"reader-mode-document-width">(root_expression);

        GtkExpression* font_size_index_value_expression = gtk_property_expression_new(G_TYPE_ACTION, font_size_index_action_expression, "state");
        GtkExpression* font_name_value_expression = gtk_property_expression_new(G_TYPE_ACTION, font_name_action_expression, "state");
        GtkExpression* text_alignment_value_expression = gtk_property_expression_new(G_TYPE_ACTION, text_alignment_action_expression, "state");
        GtkExpression* background_type_value_expression = gtk_property_expression_new(G_TYPE_ACTION, background_type_action_expression, "state");
        GtkExpression* border_shadow_enabled_value_expression = gtk_property_expression_new(G_TYPE_ACTION, border_shadow_enabled_action_expression, "state");
        GtkExpression* document_width_value_expression = gtk_property_expression_new(G_TYPE_ACTION, document_width_action_expression, "state");

        self->reader_font_size_index_watch = gtk_expression_watch(font_size_index_value_expression, self, reader_font_size_index_val_cb, self, NULL);
        self->reader_font_name_watch = gtk_expression_watch(font_name_value_expression, self, reader_font_name_val_cb, self, NULL);
        self->reader_text_alignment_watch = gtk_expression_watch(text_alignment_value_expression, self, reader_text_alignment_val_cb, self, NULL);
        self->reader_background_type_watch = gtk_expression_watch(background_type_value_expression, self, reader_background_type_val_cb, self, NULL);
        self->reader_border_shadow_enabled_watch = gtk_expression_watch(border_shadow_enabled_value_expression, self, reader_border_shadow_enabled_val_cb, self, NULL);
        self->reader_document_width_watch = gtk_expression_watch(document_width_value_expression, self, reader_document_width_val_cb, self, NULL);

        gtk_expression_unref(font_size_index_value_expression);
        gtk_expression_unref(font_name_value_expression);
        gtk_expression_unref(text_alignment_value_expression);
        gtk_expression_unref(background_type_value_expression);
        gtk_expression_unref(border_shadow_enabled_value_expression);
        gtk_expression_unref(document_width_value_expression);
#endif
    }
}

static void
orion_webview_tab_content_init(OrionWebviewTabContent* self)
{
    orion::log_verbose("orion_webview_tab_content_init");

    std::construct_at(&self->id);
    std::construct_at(&self->dto);
    std::construct_at(&self->uri);
    std::construct_at(&self->custom_uris_back_forward_list);
    std::construct_at(&self->show_favicons_inside_tabs_observer);
    std::construct_at(&self->always_show_website_titles_in_tabs_observer);
    std::construct_at(&self->title);
    std::construct_at(&self->login_url);
    std::construct_at(&self->blocked_popup_urls);
    std::construct_at(&self->settings_observer);

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    std::construct_at(&self->current_page_resource_domains);
#endif

    std::construct_at(&self->cpp_data);

    orion_webview_tab_content_register_observers(self);
}

static void
orion_webview_tab_content_measure(GtkWidget* widget, GtkOrientation orientation, int for_size,
    int* min, int* nat, int* min_baseline, int* nat_baseline)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(widget);
    if(content->is_on_custom_widget)
    {
        gtk_widget_measure(GTK_WIDGET(content->current_widget),
            orientation, for_size, min, nat, min_baseline, nat_baseline);
    }
    else
    {
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        if(content->content_webview)
        {
            gtk_widget_measure(GTK_WIDGET(content->content_webview),
                orientation, for_size, min, nat, min_baseline, nat_baseline);
        }
#endif
    }
}

static void
orion_webview_tab_content_size_allocate(GtkWidget* widget, int width, int height, int baseline)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(widget);
    content->width = width;
    content->height = height;
    if(content->current_widget)
    {
        gtk_widget_allocate(GTK_WIDGET(content->current_widget), width, height, baseline, NULL);
    }
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(content->content_webview)
    {
        gtk_widget_allocate(GTK_WIDGET(content->content_webview), width, height, baseline, NULL);
    }

    if(content->reader_mode_webview)
    {
        float y_offset = height * (1.0f - content->reader_mode_upwards_coverage);
        graphene_point_t offset = {0.0f, y_offset};
        GskTransform* transform = gsk_transform_translate(NULL, &offset);
        gtk_widget_allocate(GTK_WIDGET(content->reader_mode_webview), width, height, baseline, transform);
    }
#endif
}

static void
orion_webview_tab_content_snapshot(GtkWidget* widget, GtkSnapshot* snapshot)
{
    OrionWebviewTabContent* content = ORION_WEBVIEW_TAB_CONTENT(widget);

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(content->reader_mode_upwards_coverage < 1.0f || content->anim_cb_id)
    {
        if(content->is_on_custom_widget && content->current_widget)
        {
            gtk_widget_snapshot_child(widget, GTK_WIDGET(content->current_widget), snapshot);
        }
        else if(content->content_webview)
        {
            gtk_widget_snapshot_child(widget, GTK_WIDGET(content->content_webview), snapshot);
        }
    }
    if(content->reader_mode_webview && gtk_widget_get_visible(GTK_WIDGET(content->reader_mode_webview)))
    {
        if(content->anim_cb_id)
        {
            graphene_rect_t bounds = {{.x = 0, .y = 0}, {.width = (float)content->width, .height = (float)content->height}};
            gtk_snapshot_push_clip(snapshot, &bounds);
            gtk_widget_snapshot_child(widget, GTK_WIDGET(content->reader_mode_webview), snapshot);
            gtk_snapshot_pop(snapshot);
        }
        else
        {
            gtk_widget_snapshot_child(widget, GTK_WIDGET(content->reader_mode_webview), snapshot);
        }
    }
#else
    if(content->is_on_custom_widget && content->current_widget)
    {
        gtk_widget_snapshot_child(widget, GTK_WIDGET(content->current_widget), snapshot);
    }
#endif
}

static void
orion_webview_tab_content_class_init(OrionWebviewTabContentClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = orion_webview_tab_content_finalize;
    object_class->dispose = orion_webview_tab_content_dispose;
    object_class->set_property = orion_webview_tab_content_set_property;
    object_class->get_property = orion_webview_tab_content_get_property;

    obj_properties[PROP_ID] = g_param_spec_int64(
        "id", NULL, NULL, INT64_MIN, INT64_MAX, 0, GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_TAB_TITLE] = g_param_spec_string(
        "tab-title", NULL, NULL, "", GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_ICON] = g_param_spec_object(
        "icon", NULL, NULL, G_TYPE_ICON, GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_IS_LOADING] = g_param_spec_boolean(
        "is-loading", NULL, NULL, false, GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_URI] = g_param_spec_string(
        "uri", NULL, NULL, "", GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_CAN_GO_BACK] = g_param_spec_boolean(
        "can-go-back", NULL, NULL, false, GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_CAN_GO_FORWARD] = g_param_spec_boolean(
        "can-go-forward", NULL, NULL, false, GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_BLOCKED_POPUPS_COUNT] = g_param_spec_uint(
        "blocked-popups-count", NULL, NULL, 0, G_MAXUINT, 0, GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_IS_READERABLE] = g_param_spec_boolean("is-readerable", NULL, NULL, false,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_READER_MODE_ENABLED] = g_param_spec_boolean("reader-mode-enabled", NULL, NULL, false,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_IS_PLAYING_AUDIO] = g_param_spec_boolean("is-playing-audio", NULL, NULL, false,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_IS_MUTED] = g_param_spec_boolean("is-muted", NULL, NULL, false,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_TLS_STATUS] = g_param_spec_int64("tls-status", NULL, NULL, INT64_MIN, INT64_MAX, gint64(orion::TlsStatus::none),
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_BACK_LIST] = g_param_spec_boxed("back-list", NULL, NULL, APP_TYPE_BACK_FORWARD_LIST,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_FORWARD_LIST] = g_param_spec_boxed("forward-list", NULL, NULL, APP_TYPE_BACK_FORWARD_LIST,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_properties(object_class, N_PROPS, obj_properties);

    obj_signals[SIG_ELEMENT_PICKER_PICK] = g_signal_new("element-picker-pick",
        orion_webview_tab_content_get_type(),
        GSignalFlags(0),
        0,
        NULL,
        NULL,
        NULL,
        G_TYPE_NONE,
        3,
        G_TYPE_BOOLEAN, //true -> erase, false -> reinstate
        G_TYPE_STRING,  //selector
        G_TYPE_STRING   //host
    );
    //NOTE: this is also manually emitted upon this widget's unmap, so that
    //we emit this even when we get destroyed before we receive the ipc from webkit to emit
    obj_signals[SIG_ELEMENT_PICKER_FAIL] = g_signal_new("element-picker-fail",
        orion_webview_tab_content_get_type(),
        GSignalFlags(0),
        0,
        NULL,
        NULL,
        NULL,
        G_TYPE_NONE,
        0
    );
    obj_signals[SIG_CURRENT_PAGE_RESOURCES_CHANGED] = g_signal_new(
        "current-page-resources-changed",
        orion_webview_tab_content_get_type(),
        GSignalFlags(0),
        0,
        NULL,
        NULL,
        NULL,
        G_TYPE_NONE,
        0
    );

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->unmap = orion_webview_tab_content_unmap;
    widget_class->root = orion_webview_tab_content_attached;
    widget_class->measure = orion_webview_tab_content_measure;
    widget_class->size_allocate = orion_webview_tab_content_size_allocate;
    widget_class->snapshot = orion_webview_tab_content_snapshot;
}

//endregion
