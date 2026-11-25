#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gmacros.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangofc-fontmap.h>

#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <chrono>

#include <common/src/log.h>
#include <common/src/defer.h>
#include <common/src/crypto_test.h>
#include <common/src/punycode_test.h>
#include <common/src/string_util.h>
#include <common/src/file_system.h>

#include <core/settings.h>
#include <core/downloader.h>
#include <core/platform/portal_secret/portal_secret.h>
#include <core/platform/polkit/verify_password.h>
#include <core/test/core_api_test.h>

#include <webextensions/webview_factory_host.h>
#include <webextensions/profile_host.h>

#include "app.h"
#include "accel/accel.h"
#include "window/main_window/main_window.h"
#include "window/main_window/orion_window_iface.h"
#include "data_item/history_columnview_item.h"

#include "util/profile_manager.h"
#include "util/private_windows.h"
#include "util/kagi_service.h"
#include "util/file_util.h"
#include "util/download_cleanup.h"
#include "util/action_util.h"

#include "platform/libsecret/libsecret.h"
#include "content_block/filter_store.h"
#include "content_block/embedded_filter_lists.h"
#include "window/orion_webview_tabview_widgets/scripts/certificate_warning_scripts.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

static std::shared_ptr<OrionWebviewTabContent> make_shared_content(OrionWebviewTabContent* content)
{
    return std::shared_ptr<OrionWebviewTabContent>(content, [](OrionWebviewTabContent* content){ g_object_unref(content); });
};

// Use anonymous namespace for internal linkage of structs to avoid possible nasty ODR violations
namespace {
    //needs to stay as POD to be usable in _OrionApp struct
    struct Clipboard {
        orion::clipboard_data_type type;
        void* data;
    };

    struct CppWrapper {
        Clipboard clipboard;
        orion::ObserverId use_ctrl_z_to_undo_close_tab_observer;
        orion::ObserverId tabs_change_observer;
        orion::ObserverId theme_change_observer;
        orion::ObserverId profile_observer;
        orion::ObserverId downloads_observer;
        orion::ObserverId programmable_buttons_observer;
        std::vector<std::shared_ptr<OrionWebviewTabContent>> shared_tabs_vector;
        std::map<orion::DownloadId, std::variant<std::monostate, std::shared_ptr<orion::Downloader>, orion::gtk::WeakRefWrapper>> downloads;
        std::vector<orion::DownloadId> downloads_to_open;

        // The application owns profile hosts; each host owns its attachment.
        std::map<orion::ProfileId, std::shared_ptr<orion::bridge::ProfileHost>> profile_hosts;
        // Only activated worlds have native factories. Merely attaching a profile
        // must not create private sessions or start filter/Kagi requests.
        std::map<std::pair<orion::ProfileId, orion::PrivateMode>,
            std::shared_ptr<WebExtensions::WebExtensionWebViewFactory>> web_view_factories;
        std::map<orion::ProfileId, std::shared_ptr<orion::KagiService>> kagi_services;

        // Private Window counterparts
        std::map<orion::ProfileId, std::shared_ptr<orion::KagiService>> private_kagi_services;
    
        // Indicators for quitting profile
        std::map<orion::ProfileId, bool> is_profile_quitting;
        std::map<orion::ProfileId, bool> preserve_profile_windows_on_quit;
        std::map<orion::ScriptType, std::string> script_strings;
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        std::map<orion::ScriptType, WebKitUserScript*> webkit_user_scripts;
#endif
        std::map<orion::ProfileId, std::pair<size_t, UNIQUE_PTR(ProfileDataHolder, g_object_unref)>> profile_data_holders;

        std::vector<QuitCbItem> quit_callbacks;

        std::string dyslexic_font_base64;

        std::map<std::pair<guint, GdkModifierType>, std::string> bubble_accels;
        std::map<std::pair<guint, GdkModifierType>, std::string> capture_accels;
    };
}

static std::string empty_string = "";

struct _OrionApp
{
	AdwApplication parent;
    bool is_running;
    bool is_quitting = false;
    bool preserve_windows_on_quit = false;

    GtkStringList* icon_theme_string_list;
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitUserStyleSheet* reader_mode_font_style_sheet;
    WebKitUserStyleSheet* page_tweaks_content_blockers_hide_style_sheet;
    WebKitUserStyleSheet* page_tweaks_dark_mode_style_sheet;
#endif

    PangoFontMap* reader_mode_font_map;
    GtkStringList* default_fonts_string_list;

    CppWrapper cpp_data;
};

G_DEFINE_TYPE(OrionApp, orion_app, ADW_TYPE_APPLICATION)

static std::map<orion::ProfileId, std::shared_ptr<orion::KagiService>>*
orion_app_get_kagi_services(OrionApp* self, orion::PrivateMode private_mode)
{
    // Note that we cannot use references here (eg., auto& kagi_services = ...) because they would not be 'reseated'
    // after kagi_services = ..., they would be copied which is not what we need. So we use pointers instead.
    auto kagi_services = &self->cpp_data.kagi_services;

    if (private_mode == orion::PrivateMode::yes) {
        kagi_services = &self->cpp_data.private_kagi_services;
    }

    return kagi_services;
}

static std::shared_ptr<orion::bridge::ProfileWorld>
orion_app_get_profile_world(
    OrionApp* self, orion::ProfileId profile_id,
    orion::PrivateMode private_mode)
{
    auto it = self->cpp_data.profile_hosts.find(profile_id);
    if(it == self->cpp_data.profile_hosts.end()) return nullptr;
    return global_core_api->get_profile_world(profile_id, private_mode).lock();
}

PangoFontMap*
orion_app_get_reader_mode_font_map()
{
    OrionApp* self = ORION_APP(g_application_get_default());
    if(!self->reader_mode_font_map)
    {
        PangoFontMap* map = pango_cairo_font_map_new();
        FcConfig* config = FcInitLoadConfigAndFonts();
        pango_fc_font_map_set_config(PANGO_FC_FONT_MAP(map), config);
        FcConfigDestroy(config);
        self->reader_mode_font_map = map;
    }
    return self->reader_mode_font_map;
}

GtkStringList*
orion_app_get_default_fonts_string_list()
{
    OrionApp* self = ORION_APP(g_application_get_default());
    return self->default_fonts_string_list;
}

PangoAttrList*
get_font_attributes_for_string_list_item(GtkListItem* item, const gchar* name)
{
    if(!name) return NULL;

    if(0 == g_strcmp0(name, DEFAULT_FONT_FAMILY_NAME)) return NULL;

    PangoAttrList* list = pango_attr_list_new();

    PangoFontDescription* desc = pango_font_description_new();
    pango_font_description_set_family(desc, name);
    defer([desc]{ pango_font_description_free(desc); });

    PangoAttribute* attribute = pango_attr_font_desc_new(desc);
    pango_attr_list_insert(list, attribute);

    return list;
}

gchar*
get_font_label_for_string_list_item(GtkListItem* item, const gchar* name)
{
    if(!name) return NULL;

    if(0 == g_strcmp0(name, DEFAULT_FONT_FAMILY_NAME)) return g_strdup(DEFAULT_FONT_LABEL);

    return g_strdup(name);
}

std::string&
orion_app_get_script_string(orion::ScriptType type)
{
    OrionApp* self = ORION_APP(g_application_get_default());

    auto& scripts = self->cpp_data.script_strings;
    if(!scripts.contains(type))
    {
        switch(type)
        {
            case orion::ScriptType::readability_js:
                {
                    GError* err = NULL;
                    GBytes* bytes = g_resources_lookup_data(GRESOURCE_PREFIX "/scripts/readability.js",
                        G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
                    if(err)
                    {
                        orion::log_error("%s", err->message);
                        g_error_free(err);
                        return empty_string;
                    }

                    defer([bytes]{ g_bytes_unref(bytes); });

                    gsize len = 0;
                    const char* c_str = static_cast<const char*>(g_bytes_get_data(bytes, &len));
                    scripts.emplace(type, std::string(c_str, len));
                }
                break;
            case orion::ScriptType::auto_reader:
                {
                    GError* err = NULL;
                    GBytes* bytes = g_resources_lookup_data(GRESOURCE_PREFIX "/scripts/auto_reader.js",
                        G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
                    if(err)
                    {
                        orion::log_error("%s", err->message);
                        g_error_free(err);
                        return empty_string;
                    }

                    defer([bytes]{ g_bytes_unref(bytes); });

                    gsize len = 0;
                    const char* c_str = static_cast<const char*>(g_bytes_get_data(bytes, &len));
                    std::string_view script_string_view(c_str, len);

                    orion::str::PlaceholderMap placeholder_map = {
                        {"READABILITY-SCRIPT", std::string(orion_app_get_script_string(orion::ScriptType::readability_js))}
                    };

                    scripts.emplace(type, orion::str::parse_string_prepend_append_and_replace_placeholders(
                            script_string_view, "", "", placeholder_map, false
                        ));
                }
                break;
            case orion::ScriptType::element_picker:
                {
                    GError* err = NULL;
                    GBytes* bytes = g_resources_lookup_data(GRESOURCE_PREFIX "/scripts/element_picker.js",
                        G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
                    if(err)
                    {
                        orion::log_error("%s", err->message);
                        g_error_free(err);
                        return empty_string;
                    }

                    defer([bytes]{ g_bytes_unref(bytes); });

                    gsize len = 0;
                    const char* c_str = static_cast<const char*>(g_bytes_get_data(bytes, &len));

                    scripts.emplace(type, std::string(c_str, len));
                }
                break;
            case orion::ScriptType::certificate_warning:
                {   
                    scripts.emplace(type, orion::gtk::get_certificate_warning_script());
                }
                break;
        }
    }
    if(!scripts.contains(type))
    {
        orion::log_error("orion_app_get_script_string(): got invalid type");
        return empty_string;
    }
    return scripts.at(type);
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
WebKitUserScript*
orion_app_get_script(orion::ScriptType type)
{
    OrionApp* self = ORION_APP(g_application_get_default());

    auto& scripts = self->cpp_data.webkit_user_scripts;
    if(!scripts.contains(type))
    {
        switch(type)
        {
            case orion::ScriptType::readability_js:
                {
                    std::string& script_string = orion_app_get_script_string(type);
                    WebKitUserScript* script = webkit_user_script_new_for_world(script_string.c_str(),
                        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                        READABILITY_SCRIPT_WORLD, NULL, NULL);
                    scripts.emplace(std::make_pair(type, script));
                }
                break;
            case orion::ScriptType::auto_reader:
                {
                    std::string& script_string = orion_app_get_script_string(type);
                    WebKitUserScript* script = webkit_user_script_new_for_world(script_string.c_str(),
                        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
                        READABILITY_SCRIPT_WORLD, NULL, NULL);
                    scripts.emplace(std::make_pair(type, script));
                }
                break;
            case orion::ScriptType::element_picker:
                {
                    std::string& script_string = orion_app_get_script_string(type);
                    WebKitUserScript* script = webkit_user_script_new_for_world(script_string.c_str(),
                        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END,
                        READABILITY_SCRIPT_WORLD, NULL, NULL);
                    scripts.emplace(std::make_pair(type, script));
                }
                break;
            case orion::ScriptType::certificate_warning:
                {
                    std::string& script_string = orion_app_get_script_string(type);
                    // World name must be NULL, ie., explicitly visible from all pages (same on Mac)
                    WebKitUserScript* script = webkit_user_script_new(
                        script_string.c_str(), // source
                        WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, // injected_frames
                        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END, // injection_time
                        NULL, // allow_list
                        NULL); // block_list
                    scripts.emplace(std::make_pair(type, script));
                }
                break;
        }
    }
    if(!scripts.contains(type))
    {
        orion::log_error("orion_app_get_script(): got invalid type");
        return NULL;
    }
    return scripts.at(type);
}

WebKitUserStyleSheet*
orion_app_get_reader_mode_font_style_sheet()
{
    OrionApp* self = ORION_APP(g_application_get_default());
    if(!self->reader_mode_font_style_sheet)
    {
        GError* err = NULL;
        std::string data = "";

        GBytes* bytes = g_resources_lookup_data(GRESOURCE_PREFIX "/css/reader_mode_fonts.css", G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
        if(err)
        {
            orion::log_error("%s", err->message);
            g_error_free(err);
        }
        else
        {
            gsize len = 0;
            const char* bytes_data = static_cast<const char*>(g_bytes_get_data(bytes, &len));
            data = std::string(bytes_data, len);
        }

        //orion::log_debug("%s", data.c_str());
        self->reader_mode_font_style_sheet = webkit_user_style_sheet_new(
            data.c_str(),
            WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
            WEBKIT_USER_STYLE_LEVEL_USER,
            NULL,
            NULL
        );
    }
    return self->reader_mode_font_style_sheet;
}
#endif

std::string
orion_app_get_dyslexic_font_base64()
{
    OrionApp* self = ORION_APP(g_application_get_default());

    if(self->cpp_data.dyslexic_font_base64.length() == 0)
    {
        GError* err = NULL;
        GBytes* dyslexic_font_bytes = g_resources_lookup_data(GRESOURCE_PREFIX "/fonts/OpenDyslexic3-Regular.ttf", G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
        if(err)
        {
            orion::log_error("%s", err->message);
            g_error_free(err);
            return "";
        }
        defer([dyslexic_font_bytes]{ g_bytes_unref(dyslexic_font_bytes); });

        size_t len = 0;
        const guchar* data = static_cast<const guchar*>(g_bytes_get_data(dyslexic_font_bytes, &len));

        gchar* encoded_c_str = g_base64_encode(data, len);
        if (!encoded_c_str) {
            return "";
        }
        defer([encoded_c_str]{ g_free(encoded_c_str); });

        //integer division with this + 2 is the same as calling ceil on floating point math
        size_t base64_len = ((len + 2) / 3) * 4;
        self->cpp_data.dyslexic_font_base64 = std::string(encoded_c_str, base64_len);
    }
    return self->cpp_data.dyslexic_font_base64;
}

std::shared_ptr<orion::bridge::ProfileHost>
orion_app_get_or_create_profile_host(orion::ProfileId profile_id)
{
    OrionApp* self = ORION_APP(g_application_get_default());
    auto [it, inserted] = self->cpp_data.profile_hosts.try_emplace(profile_id);
    if(inserted) {
        it->second = orion::create_app_profile_host(
            *global_core_api, profile_id);
    }
    return it->second;
}

std::weak_ptr<WebExtensions::WebExtensionWebViewFactory>
orion_app_get_web_view_factory(orion::bridge::ProfileWorld& world)
{
    auto* app = ORION_APP(g_application_get_default());
    auto it = app->cpp_data.web_view_factories.find(
        {world.profile_id(), world.private_mode()});
    if(it == app->cpp_data.web_view_factories.end()) return {};
    // A callback from a retired world must not resolve a replacement factory.
    if(it->second->parent_world().lock().get() != &world) return {};
    return it->second;
}

std::weak_ptr<orion::bridge::ProfileWorld>
orion_app_activate_profile_world(
    orion::ProfileId profile_id, orion::PrivateMode private_mode)
{
    orion_app_get_or_create_profile_host(profile_id);
    auto world = global_core_api->get_profile_world(profile_id, private_mode).lock();
    assert(world);
    auto* app = ORION_APP(g_application_get_default());
    auto key = std::make_pair(profile_id, private_mode);
    if(!app->cpp_data.web_view_factories.contains(key)) {
        auto bridge = world->web_view_factory_bridge().lock();
        assert(bridge);
        auto factory = WebExtensions::WebExtensionWebViewFactory::create(*bridge);
        app->cpp_data.web_view_factories.emplace(key, std::move(factory));
    }
    world->initialize();
    return world;
}

std::shared_ptr<orion::KagiService>
orion_app_get_or_create_kagi_service(
    orion::bridge::ProfileWorld& profile_world)
{
    // Service creation is part of world activation, never profile attachment.
    if(!profile_world.runtime() || orion_app_get_web_view_factory(profile_world).expired()) {
        return nullptr;
    }
    OrionApp* self = ORION_APP(g_application_get_default());
    const auto profile_id = profile_world.profile_id();
    const auto private_mode = profile_world.private_mode();

    auto kagi_services = orion_app_get_kagi_services(self, private_mode);

    auto it = kagi_services->find(profile_id);
    if (it != kagi_services->end()) {
        std::shared_ptr<orion::KagiService> kagi_service = it->second;
        if (kagi_service) {
            return kagi_service;
        }
        kagi_services->erase(it);
    }
    auto kagi_service = orion::KagiService::create(profile_world);
    (*kagi_services)[profile_id] = kagi_service;
    return kagi_service;
}

bool
orion_app_is_running(OrionApp* app)
{
    return app->is_running;
}

bool
orion_app_is_quitting(OrionApp* app)
{
    return app->is_quitting;
}

bool
orion_app_preserve_windows_on_quit(OrionApp* app)
{
    return app->preserve_windows_on_quit;
}

void
orion_app_cleanup_profile_resources(
    OrionApp* app, orion::ProfileId profile_id, std::vector<orion::PrivateMode> private_modes)
{
    auto profile_host_it = app->cpp_data.profile_hosts.find(profile_id);
    auto profile_host = profile_host_it != app->cpp_data.profile_hosts.end()
        ? profile_host_it->second
        : nullptr;

    for (orion::PrivateMode private_mode : private_modes) {
        auto kagi_services = orion_app_get_kagi_services(app, private_mode);
        
        if (kagi_services->contains(profile_id)) {
            orion::log_verbose(
                "orion_app_cleanup_profile_resources: removing kagi service for profile_id=%lld private=%d",
                profile_id.value, static_cast<int>(private_mode));
            kagi_services->erase(profile_id);
        }

        if(profile_host) {
            if(auto profile_world = global_core_api
                ->get_profile_world(profile_id, private_mode).lock()) {
                profile_world->cleanup();
            }
            app->cpp_data.web_view_factories.erase({profile_id, private_mode});
        }
    }

    bool profile_host_has_resources = false;
    if(profile_host) {
        for(orion::PrivateMode private_mode
            : {orion::PrivateMode::no, orion::PrivateMode::yes}) {
            auto profile_world = global_core_api
                ->get_profile_world(profile_id, private_mode).lock();
            if(profile_world
                && (profile_world->runtime()
                    || orion_app_get_web_view_factory(*profile_world).lock())) {
                profile_host_has_resources = true;
                break;
            }
        }
    }

    if(profile_host && !profile_host_has_resources
        && app->cpp_data.profile_hosts.erase(profile_id)) {
        orion::log_verbose(
            "orion_app_cleanup_profile_resources: removed profile host for profile_id=%lld",
            profile_id.value);
    }

    // Should these be erased here? Ignoring private mode?
    app->cpp_data.is_profile_quitting.erase(profile_id);
    app->cpp_data.preserve_profile_windows_on_quit.erase(profile_id);
}

void
orion_app_wait_for_profile_async_ops(
    OrionApp* self, orion::ProfileId profile_id, std::vector<orion::PrivateMode> private_modes, orion::Fn ready_cb)
{   
    std::shared_ptr<size_t> pending_ops = std::make_shared<size_t>(0);
    
    for (orion::PrivateMode private_mode : private_modes) {
        auto kagi_services = orion_app_get_kagi_services(self, private_mode);

        // Check kagi service
        if (kagi_services->contains(profile_id)) {
            (*pending_ops)++;
        }
    
        // Check webview factory
        auto profile_world = orion_app_get_profile_world(
            self, profile_id, private_mode);
        if(profile_world && orion_app_get_web_view_factory(*profile_world).lock()) {
            (*pending_ops)++;
        }
    }
    
    if (*pending_ops == 0) {
        ready_cb(); // this should probably be dispatched asynchronously instead to avoid certain issues
        return;
    }
    
    auto complete_cb = [pending_ops, ready_cb]{
        if ((--(*pending_ops)) == 0) {
            ready_cb();
        }
    };

    for (orion::PrivateMode private_mode : private_modes) {
        auto kagi_services = orion_app_get_kagi_services(self, private_mode);

        if (kagi_services->contains(profile_id)) {
            (*kagi_services)[profile_id]->cancel_all_operations(complete_cb);
        }
        
        auto profile_world = orion_app_get_profile_world(
            self, profile_id, private_mode);
        if(auto webview_factory = profile_world
                ? orion_app_get_web_view_factory(*profile_world).lock()
                : nullptr) {
            orion::contentblock::cancel_all_content_filter_store_ops(
                webview_factory.get(),
                complete_cb
            );
        }
    }
}

static void
orion_app_check_finalization()
{
    orion::check_observers(global_core_api->bookmarks_change(), "bookmark");
    orion::check_observers(global_core_api->bookmark_folders_change(), "bookmark folder");
    orion::check_observers(global_core_api->reading_list_entries_change(), "reading list entry");
    orion::check_observers(global_core_api->history_entries_change(), "history entry");
    orion::check_observers(global_core_api->history_entry_days_change(), "history entry day");
    orion::check_observers(global_core_api->profiles_change(), "profile");
    orion::check_observers(global_core_api->windows_change(), "window");
    orion::check_observers(global_core_api->tabs_change(), "tab");
    orion::check_observers(global_core_api->settings_change(), "setting");
    orion::check_observers(global_core_api->single_setting_change(), "single setting");
    orion::check_observers(global_core_api->extensions_change(), "extension");
    orion::check_observers(global_core_api->tab_back_forward_list_entries_change(), "tab back/forward list entry");
    orion::check_observers(global_core_api->ui_windows_change(), "UI window");
    orion::check_observers(global_core_api->recently_closed_tabs_change(), "recently closed tab");
    orion::check_observers(global_core_api->password_items_change(), "password item");
    orion::check_observers(global_core_api->filter_lists_change(), "filter list");
    orion::check_observers(
        orion::contentblock::filter_lists_sync_change(), "filter list sync");

    if (global_history_columnview_item_count > 0) {
        orion::log_warning("warning: remaining history columnview items (never finalized): %d",
            global_history_columnview_item_count
        );
    }

    if (!global_core_api->get_registered_ui_window_ids().empty()) {
        orion::log_warning("warning: remaining registered UI window ids: %zu",
            global_core_api->get_registered_ui_window_ids().size()
        );
    }

    if (global_core_api->get_ui_window_ref_count() != 0) {
        orion::log_warning("warning: UI window ref count is not zero: %zu",
            global_core_api->get_ui_window_ref_count()
        );
    }
}

static void
orion_app_clear_observers(OrionApp* self)
{
    global_core_api->single_setting_change().clear_observer(&self->cpp_data.use_ctrl_z_to_undo_close_tab_observer);
    global_core_api->tabs_change().clear_observer(&self->cpp_data.tabs_change_observer);
    global_core_api->single_setting_change().clear_observer(&self->cpp_data.theme_change_observer);
    global_core_api->profiles_change().clear_observer(&self->cpp_data.profile_observer);
    global_core_api->downloads_change().clear_observer(&self->cpp_data.downloads_observer);
    global_core_api->programmable_buttons_change().clear_observer(&self->cpp_data.programmable_buttons_observer);
}

static void
orion_app_quit_internal(OrionApp* self)
{
    orion::log_verbose("orion_app_quit_internal");

    orion_app_clear_observers(self);

    orion::profile::close_all();

    self->cpp_data.kagi_services.clear();

    self->cpp_data.private_kagi_services.clear();
    self->cpp_data.web_view_factories.clear();
    self->cpp_data.profile_hosts.clear();

    self->is_running = false;

    for(auto& s : self->cpp_data.shared_tabs_vector)
    {
        OrionWebviewTabContent* content = s.get();
        uint ref_count = G_OBJECT(s.get())->ref_count;
        if(ref_count > 1)
        {
            orion::log_error("tab with id %ld not getting destroyed together with app object, remaining ref count %u",
                orion_webview_tab_content_get_id(content).value, ref_count-1);
        }
    }
    self->cpp_data.shared_tabs_vector.clear();

    // We have to store the iteration callback and user data before quitting the run loop,
    // because the run loop will be destroyed and the callback will no longer be valid.
    auto iteration_cb = global_core_api->get_iteration();
    auto iteration_user_data = global_core_api->get_iteration_user_data();

    VERIFY_LOG(global_core_api->request_stop_run_loop([self, iteration_cb, iteration_user_data] {
        // Do not call global_core_api here, its iternal run loop is already stopped
        // and destroyed.
        orion_app_check_finalization();
        orion::log_verbose("orion_app_quit() called, quitting application");
        g_application_quit(G_APPLICATION(self));
        if (iteration_cb) {
            iteration_cb(iteration_user_data); // call the iteration callback one last time
        }
    }));
}

static void
orion_app_wait_for_kagi_service_to_quit(orion::ErrorFn ready_cb)
{
    OrionApp* app = ORION_APP(g_application_get_default());

    std::shared_ptr<int32_t> pending_services = std::make_shared<int>(
        app->cpp_data.kagi_services.size() + app->cpp_data.private_kagi_services.size());

    if (*pending_services == 0) {
        // No kagi services, we can quit immediately
        ready_cb(orion::Error::ok());
        return;
    }

    orion::log_verbose(
        "orion_app_wait_for_kagi_service_to_quit: waiting for kagi services to be ready to quit: kagi_services=%d",
        *pending_services
    );

    for (orion::PrivateMode private_mode : {orion::PrivateMode::no, orion::PrivateMode::yes}) {
        auto kagi_services = orion_app_get_kagi_services(app, private_mode);

        for (auto [profile_id, kagi_service] : *kagi_services) {
            // Send request to all remaining kagi services to cancel their pending
            // operations if any
            kagi_service->cancel_all_operations([pending_services,ready_cb=ready_cb]{
                (*pending_services)--;
                if (*pending_services <= 0) {
                    // All kagi services have signaled that they are ready to quit, we can call
                    // the internal quit function now. It is guaranteed that we are not called synchronously
                    // so we avoid issues with runtime lifetime
                    ready_cb(orion::Error::ok());
                }
            });
        }
    }
}

static void
orion_app_wait_for_factories_to_quit(orion::ErrorFn ready_cb)
{
    OrionApp* app = ORION_APP(g_application_get_default());

    std::shared_ptr<int32_t> pending_factories = std::make_shared<int>(0);
    for(const auto& profile_host_entry : app->cpp_data.profile_hosts) {
        const auto profile_id = profile_host_entry.first;
        for(orion::PrivateMode private_mode
            : {orion::PrivateMode::no, orion::PrivateMode::yes}) {
            auto world = global_core_api
                ->get_profile_world(profile_id, private_mode).lock();
            if(!world || !orion_app_get_web_view_factory(*world).lock()) continue;
            ++*pending_factories;
        }
    }

    if (*pending_factories == 0) {
        // No webview factories, we can quit immediately
        ready_cb(orion::Error::ok());
        return;
    }

    orion::log_verbose(
        "orion_app_wait_for_factories_to_quit: waiting for content filter stores to be ready to quit: webview_factories=%d",
        *pending_factories
    );

    for (orion::PrivateMode private_mode : {orion::PrivateMode::no, orion::PrivateMode::yes}) {
        for(const auto& profile_host_entry : app->cpp_data.profile_hosts) {
            const auto profile_id = profile_host_entry.first;
            auto world = global_core_api
                ->get_profile_world(profile_id, private_mode).lock();
            if(!world) continue;
            auto webview_factory = orion_app_get_web_view_factory(*world).lock();
            if(!webview_factory) continue;
            // Send request to all remaining content filter stores to cancel their pending
            // operations (save, load etc.) if any
            orion::contentblock::cancel_all_content_filter_store_ops(webview_factory.get(),
            [pending_factories,ready_cb=ready_cb]{
                (*pending_factories)--;
                
                orion::log_verbose(
                    "orion_app_wait_for_factories_to_quit: factory cancelled, pending factories: %d",
                    *pending_factories
                );

                if (*pending_factories <= 0) {
                    // All content filter stores have signaled that they are ready to quit, we can call
                    // the internal quit function now. It is guaranteed that we are not called synchronously
                    // so we avoid issues with runtime lifetime
                    ready_cb(orion::Error::ok());
                }
            });
        }
    }
}

static bool
app_opens_with_restored_session(orion::settings::AppOpensWith app_opens_with)
{
    switch (app_opens_with) {
        case orion::settings::AppOpensWith::all_windows_from_last_session:
        case orion::settings::AppOpensWith::all_non_private_windows_from_last_session:
            return true;
        case orion::settings::AppOpensWith::new_window:
        case orion::settings::AppOpensWith::new_private_window:
        case orion::settings::AppOpensWith::not_set:
        default:
            return false;
    }
}

// Asynchronously perform the given functions for each profile id, and call done_cb when all functions have completed
static void
orion_app_for_each_profile(
    const std::vector<std::function<void(orion::ProfileId, orion::ErrorFn)>>& foreach_fns, orion::ErrorFn done_cb)
{
    global_core_api->get_profile_ids(
    [foreach_fns=std::move(foreach_fns),done_cb=std::move(done_cb)](orion::GetProfileIdsReply reply){
        VERIFY_LOG_OR_DONE_CB(reply.err, done_cb);

        std::vector<orion::Cb<orion::Cb<orion::Error>>> fns;

        for (orion::ProfileId id : reply.ids) {
            for (auto& foreach_fn : foreach_fns) {
                fns.push_back([id,foreach_fn=std::move(foreach_fn)](orion::Cb<orion::Error> cb){
                    foreach_fn(id, [id,cb=std::move(cb)](orion::Error err){
                        cb(err);
                    });
                });
            }
        }

        global_core_api->when_all(fns, done_cb);
    });
}

// Called when the application is quitting and starting
static std::vector<std::function<void(orion::ProfileId, orion::ErrorFn)>>
orion_app_get_profile_fns_for_quit_and_start(bool is_starting)
{
    std::vector<std::function<void(orion::ProfileId, orion::ErrorFn)>> profile_fns{
        // Add more async operations that should be performed for each profile if needed
        [is_starting](orion::ProfileId profile_id, orion::ErrorFn done_cb){
            auto trigger = is_starting
                ? orion::CleanupDownloadsTrigger::on_startup
                : orion::CleanupDownloadsTrigger::on_quit;
            orion::cleanup_downloads(profile_id, trigger, std::move(done_cb));
        },
        // Delete all backend private data of each profile. This is a safety measure that should
        // only be needed when the app was not closed gracefully.
        [](orion::ProfileId profile_id, orion::ErrorFn done_cb){
            orion::profile::cleanup_private_data(profile_id, std::move(done_cb));
        },
    };

    return profile_fns;
}

// This is called when Quit Orion action is triggered, or when the application is closed by closing the last main window
void
orion_app_quit()
{
    OrionApp* app = ORION_APP(g_application_get_default());

    if (app->is_quitting) {
        orion::log_info("orion_app_quit: called while already quitting, ignoring another request");
        return;
    }

    app->is_quitting = true;

    auto continue_quit = [app](bool preserve_windows_on_quit){
        app->preserve_windows_on_quit = preserve_windows_on_quit;

        // Close all remaining main windows one by one
        std::vector<orion::UiWindowId> ids = global_core_api->get_registered_ui_window_ids();
        orion::log_verbose("registered main windows: %zu", ids.size());
        for (orion::UiWindowId id : ids) {
            gtk_window_close(GTK_WINDOW(id.value));
            // Do not unregister the UI window here, it is done automatically in main_window_finalize
        }

        // orion_app_quit_internal must be called only after all global_core_api reply callbacks
        // had been called because they might never be called otherwise
        auto api_callbacks_finished_cb = [app]{
            // Add more async operation managers here if needed
            orion_app_register_quit_callback(
                app, {"orion_app_wait_for_factories_to_quit", orion_app_wait_for_factories_to_quit}
            );
            orion_app_register_quit_callback(
                app, {"orion_app_wait_for_kagi_service_to_quit", orion_app_wait_for_kagi_service_to_quit}
            );

            std::shared_ptr<size_t> pending_funcs = std::make_shared<size_t>(app->cpp_data.quit_callbacks.size());

            orion::log_verbose(
                "orion_app_quit: waiting for %zu quit callbacks to finish",
                *pending_funcs
            );

            for (auto& item : app->cpp_data.quit_callbacks) {
                item.cb([pending_funcs,item](orion::Error err){
                    --(*pending_funcs);
                    orion::log_verbose(
                        "orion_app_quit: quit callback '%s' finished, remaining pending callbacks: %zu",
                        item.name.c_str(), *pending_funcs
                    );
                    if (*pending_funcs == 0) {
                        global_core_api->async([]{
                            orion_app_quit_internal(ORION_APP(g_application_get_default()));
                        });
                    }
                });
            }
        };

        if (preserve_windows_on_quit) {
            // In restore-session modes, keep backend windows and tab groups so startup can reopen them.
            api_callbacks_finished_cb();
            return;
        }

        // Delete all remaining tab groups in the backend that are not associated with any main window.
        global_core_api->get_profile_ids([=](orion::GetProfileIdsReply reply){
            VERIFY_LOG(reply.err);

            if (reply.ids.empty()) {
                api_callbacks_finished_cb();
                return;
            }

            std::shared_ptr<size_t> remaining_profiles = std::make_shared<size_t>(reply.ids.size());

            for (orion::ProfileId profile_id : reply.ids) {
                global_core_api->get_window_ids({.profile_id=profile_id}, [=](orion::GetWindowIdsReply reply){
                    VERIFY_LOG(reply.err);

                    if (reply.ids.empty()) {
                        if ((--(*remaining_profiles)) <= 0) {
                            api_callbacks_finished_cb();
                        }
                        return;
                    }

                    global_core_api->get_windows(reply.ids, [=](orion::GetWindowsReply reply){
                        VERIFY_LOG(reply.err);
                        std::vector<orion::WindowId> ids_to_delete;
                        for (const orion::WindowDto& window : reply.windows) {
                            if (window.is_tab_group) {
                                ids_to_delete.push_back(window.id);
                            }
                        }
                        if (!ids_to_delete.empty()) {
                            orion::log_debug("orion_app_quit: deleting remaining tab groups in the backend: %zu", ids_to_delete.size());
                            global_core_api->delete_windows(ids_to_delete, [=](orion::Error err){
                                VERIFY_LOG(err);
                                if ((--(*remaining_profiles)) <= 0) {
                                    api_callbacks_finished_cb();
                                }
                            });
                        } else {
                            if ((--(*remaining_profiles)) <= 0) {
                                api_callbacks_finished_cb();
                            }
                        }
                    });
                });
            }
        });
    };

    orion_app_for_each_profile(orion_app_get_profile_fns_for_quit_and_start(false),
    [continue_quit=std::move(continue_quit)](orion::Error err){
        VERIFY_LOG(err);

        global_core_api->get_default_profile_id([continue_quit](orion::GetDefaultProfileIdReply profile_reply){
            VERIFY_LOG(profile_reply.err);
            if (!profile_reply.profile_id.value) {
                continue_quit(false);
                return;
            }

            global_core_api->get_setting(profile_reply.profile_id, orion::settings::general::app_opens_with,
                [continue_quit](orion::GetSettingReply<orion::settings::AppOpensWith> setting_reply){
                    VERIFY_LOG(setting_reply.err);
                    continue_quit(app_opens_with_restored_session(setting_reply.value));
                }
            );
        });
    });
}

void
orion_app_register_quit_callback(OrionApp* self, QuitCbItem item)
{
    self->cpp_data.quit_callbacks.push_back(item);

    orion::log_verbose(
        "orion_app_register_quit_callback: registered quit callback '%s' (total now: %zu)",
        item.name.c_str(), self->cpp_data.quit_callbacks.size()
    );
}

orion::clipboard_data_type
orion_app_get_clipboard_data_type(OrionApp* app)
{
    g_return_val_if_fail(ORION_IS_APP(app), orion::clipboard_data_type::no_data);
    return app->cpp_data.clipboard.type;
}

const void*
orion_app_get_clipboard_data(OrionApp* app)
{
    g_return_val_if_fail(ORION_IS_APP(app), NULL);
    return app->cpp_data.clipboard.data;
}

void*
orion_app_take_clipboard_data(OrionApp* app)
{
    g_return_val_if_fail(ORION_IS_APP(app), NULL);
    app->cpp_data.clipboard.type = orion::clipboard_data_type::no_data;
    return g_steal_pointer(&(app->cpp_data.clipboard.data));
}

static void
orion_app_clear_clipboard(OrionApp* app)
{
    if(app->cpp_data.clipboard.data)
    {
        switch(app->cpp_data.clipboard.type)
        {
            case orion::clipboard_data_type::no_data: 
                orion::log_debug("programming error: app->cpp_data.clipboard.data was set while type was orion::clipboard_data_type::no_data");
                app->cpp_data.clipboard.data = NULL;
                break;
            case orion::clipboard_data_type::bookmarks_copy_query:
                delete static_cast<orion::CopyBookmarksQuery*>(app->cpp_data.clipboard.data);
                app->cpp_data.clipboard.data = NULL;
                break;
        }
    }
}

void
orion_app_set_clipboard_data(OrionApp* app, void* data, orion::clipboard_data_type type)
{
    g_return_if_fail(ORION_IS_APP(app));
    orion_app_clear_clipboard(app);
    app->cpp_data.clipboard.data = data;
    app->cpp_data.clipboard.type = type;

}

OrionWebviewTabContent*
orion_app_get_shared_tab(OrionApp* app, orion::TabId tab_id)
{
    auto it = std::find_if(app->cpp_data.shared_tabs_vector.begin(), app->cpp_data.shared_tabs_vector.end(),
        [tab_id](std::shared_ptr<OrionWebviewTabContent> ptr){ return tab_id == orion_webview_tab_content_get_id(ORION_WEBVIEW_TAB_CONTENT(ptr.get())); });
    if(it != app->cpp_data.shared_tabs_vector.end())
    {
        return ORION_WEBVIEW_TAB_CONTENT(it->get());
    }
    else
    {
        //orion::log_debug("id not found: %ld", tab_id.value);
        return NULL;
    }
}

void
orion_app_add_shared_pinned_tab(OrionApp* app, OrionWebviewTabContent* content)
{
    //orion::log_debug("adding id: %ld", orion_webview_tab_content_get_id(content).value);
    auto it = std::find_if(app->cpp_data.shared_tabs_vector.cbegin(), app->cpp_data.shared_tabs_vector.cend(),
        [tab_id = orion_webview_tab_content_get_id(content)](std::shared_ptr<OrionWebviewTabContent> ptr){ return tab_id == orion_webview_tab_content_get_id(ORION_WEBVIEW_TAB_CONTENT(ptr.get())); });
    if(it == app->cpp_data.shared_tabs_vector.cend())
    {
        //orion::log_debug("added id: %ld", orion_webview_tab_content_get_id(content).value);
        app->cpp_data.shared_tabs_vector.push_back(make_shared_content(g_object_ref(content)));
    }
    else
    {
        orion::log_error("attempted adding the same tab multiple times to global store");
    }
}

std::variant<std::monostate, std::shared_ptr<orion::Downloader>, orion::gtk::WeakRefWrapper>
orion_app_get_downloader_for_id(OrionApp* app, orion::DownloadId id)
{
    if(app->cpp_data.downloads.contains(id))
    {
        return app->cpp_data.downloads[id];
    }
    else
    {
        return std::monostate{};
    }
}

void
orion_app_add_download_with_id(OrionApp* app, std::variant<std::monostate, std::shared_ptr<orion::Downloader>, orion::gtk::WeakRefWrapper> downloader, orion::DownloadId id)
{
    app->cpp_data.downloads[id]=downloader;
}

bool
orion_app_remove_download_for_id(OrionApp* app, orion::DownloadId id)
{
    return app->cpp_data.downloads.erase(id);
}

void
orion_app_update_theme(OrionApp* app, orion::ProfileId profile_id)
{
    auto set_theme = [](orion::settings::Theme theme){
        AdwStyleManager* style_manager = adw_style_manager_get_default();
        switch (theme) {
            case orion::settings::Theme::light:
                adw_style_manager_set_color_scheme(style_manager, ADW_COLOR_SCHEME_PREFER_LIGHT);
                break;
            case orion::settings::Theme::dark:
                adw_style_manager_set_color_scheme(style_manager, ADW_COLOR_SCHEME_PREFER_DARK);
                break;
            default:
                adw_style_manager_set_color_scheme(style_manager, ADW_COLOR_SCHEME_DEFAULT);
        }
    };

    global_core_api->get_setting(profile_id, orion::settings::appearance::theme,
        [set_theme](orion::GetSettingReply<orion::settings::Theme> reply){
            VERIFY_LOG(reply.err);
            set_theme(reply.value);
        }
    );

    if (!app->cpp_data.theme_change_observer.value) {
        app->cpp_data.theme_change_observer = global_core_api->add_single_setting_change_observer(
            orion::settings::appearance::theme,
            [set_theme](orion::SingleSettingChange<orion::settings::Theme> change){
                set_theme(change.new_value);
            }
        );
    }
}

static void
on_icon_theme_changed(GtkIconTheme* theme, GtkStringList* list)
{
    char** names = gtk_icon_theme_get_icon_names(theme);
    defer([names]{ g_strfreev(names); });
    uint n_items = g_list_model_get_n_items(G_LIST_MODEL(list));
    gtk_string_list_splice(list, 0, n_items, names);
}

GtkStringList*
orion_app_get_icon_theme_string_list(OrionApp* app)
{
    if(!app->icon_theme_string_list)
    {
        GtkIconTheme* theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
        gchar** names = gtk_icon_theme_get_icon_names(theme);

        defer([names]{ g_strfreev(names); });

        app->icon_theme_string_list = gtk_string_list_new(names);

        g_signal_connect_object(theme, "changed", G_CALLBACK(on_icon_theme_changed), app->icon_theme_string_list, G_CONNECT_DEFAULT);
    }
    return app->icon_theme_string_list;
}

ProfileDataHolder*
orion_app_ref_profile_data_holder(orion::ProfileId id)
{
    orion::log_verbose("orion_app_ref_profile_data_holder: id=%lld", id.value);
    OrionApp* self = ORION_APP(g_application_get_default());
    if(!self->cpp_data.profile_data_holders.contains(id))
    {
        self->cpp_data.profile_data_holders.try_emplace(id, 0, profile_data_holder_new(id));
    }
    self->cpp_data.profile_data_holders.at(id).first++;
    return self->cpp_data.profile_data_holders.at(id).second.get();
}

void
orion_app_unref_profile_data_holder(orion::ProfileId id)
{
    orion::log_verbose("orion_app_unref_profile_data_holder: id=%lld", id.value);
    OrionApp* self = ORION_APP(g_application_get_default());
    if(!self->cpp_data.profile_data_holders.contains(id))
    {
        orion::log_error("orion_app_unref_profile_data_holder(): attempted to unref for profile id not in store");
        return;
    }

    self->cpp_data.profile_data_holders.at(id).first--;
    if(self->cpp_data.profile_data_holders.at(id).first <= 0)
    {
        orion::log_verbose(
            "orion_app_unref_profile_data_holder: ref_count=%d",
            G_OBJECT(self->cpp_data.profile_data_holders.at(id).second.get())->ref_count);
        self->cpp_data.profile_data_holders.erase(id);
    }
}


void
orion_app_set_accels_for_action(OrionApp* app, const std::string& action_name, const std::vector<std::string>& accels, GtkPropagationPhase phase)
{
    assert(phase == GTK_PHASE_BUBBLE || phase == GTK_PHASE_CAPTURE);
    auto& map = (phase == GTK_PHASE_BUBBLE) ? (app->cpp_data.bubble_accels) : (app->cpp_data.capture_accels);
    std::vector<std::pair<guint, GdkModifierType>> parsed_accels;
    for(const std::string& accel : accels)
    {
        guint keyval = 0;
        GdkModifierType mods = GDK_NO_MODIFIER_MASK;
        if(gtk_accelerator_parse(accel.c_str(), &keyval, &mods))
        {
            parsed_accels.push_back({gdk_keyval_to_lower(keyval), GdkModifierType(gtk_accelerator_get_default_mod_mask() & mods)});
        }
    }

    std::erase_if(map, [&action_name](const auto& pair) {
        return pair.second == action_name;
    });

    for(auto& accel : parsed_accels)
    {
        map[accel] = action_name;
    }
}

static gboolean
window_key_pressed(GtkEventController* controller, guint keyval, guint keycode, GdkModifierType state, std::map<std::pair<guint, GdkModifierType>, std::string>* accel_map)
{
    std::pair<guint, GdkModifierType> key = {gdk_keyval_to_lower(keyval), GdkModifierType(gtk_accelerator_get_default_mod_mask() & state)};
    if(accel_map->contains(key))
    {
        gtk_widget_activate_action(gtk_event_controller_get_widget(controller), accel_map->at(key).c_str(), NULL);
        return TRUE;
    }
    return FALSE;
}

static void
orion_app_window_added(GtkApplication* app, GtkWindow* window)
{
    OrionApp* self = ORION_APP(app);

    GtkEventController* bubble_controller = gtk_event_controller_key_new();
    GtkEventController* capture_controller = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(bubble_controller, GTK_PHASE_BUBBLE);
    gtk_event_controller_set_propagation_phase(capture_controller, GTK_PHASE_CAPTURE);
    //this user data usage is safe, app object outlives windows
    g_signal_connect(bubble_controller, "key-pressed", G_CALLBACK(window_key_pressed), &self->cpp_data.bubble_accels);
    g_signal_connect(capture_controller, "key-pressed", G_CALLBACK(window_key_pressed), &self->cpp_data.capture_accels);
    gtk_widget_add_controller(GTK_WIDGET(window), bubble_controller);
    gtk_widget_add_controller(GTK_WIDGET(window), capture_controller);

    GTK_APPLICATION_CLASS(orion_app_parent_class)->window_added(app, window);
}

static void
orion_app_setup_profile_observer(OrionApp* self)
{
    orion::gtk::WeakRefWrapper self_weak(self);

    orion::ObserverId observer_id = global_core_api->profiles_change().add_observer(
    [self_weak](const orion::ProfilesChange& change){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        OrionApp* self = ORION_APP(self_strong.get());

        for(orion::ProfileId deleted_profile_id : change.deleted) {
            orion_app_wait_for_profile_async_ops(
                self, deleted_profile_id,
                {orion::PrivateMode::no, orion::PrivateMode::yes},
                [deleted_profile_id, self_weak] {
                    auto self_strong = self_weak.lock();
                    if(!self_strong) return;
                    OrionApp* self = ORION_APP(self_strong.get());
                    self->cpp_data.kagi_services.erase(deleted_profile_id);
                    self->cpp_data.private_kagi_services.erase(deleted_profile_id);
                });
        }
    });

    self->cpp_data.profile_observer = observer_id;
}

static void
setup_accels(OrionApp* self)
{
	orion::register_accels(self);
    orion::gtk::WeakRefWrapper app_weak(self);

    // note: this setting is not profile-specific (nor main window-specific), always use default profile
    global_core_api->get_default_profile_id([app_weak](orion::GetDefaultProfileIdReply reply){
        VERIFY_LOG(reply.err);
        orion::ProfileId default_profile_id = reply.profile_id;

        auto app_strong = app_weak.lock();
        if (!app_strong) return;
        OrionApp* app = ORION_APP(app_strong.get());

        global_core_api->get_setting(
            default_profile_id, orion::settings::tabs::use_ctrl_z_to_undo_close_tab,
            [default_profile_id,app_weak](orion::GetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);

                auto app_strong = app_weak.lock();
                if (!app_strong) return;
                OrionApp* self = ORION_APP(app_strong.get());

                bool is_enabled = reply.value;
                // note: this setting is not profile-specific (nor main window-specific)
                orion::undo_tab_close_accel_set_enabled(
                    self,
                    is_enabled
                );
            });
            
        // always use default profile when changing use_ctrl_z_to_undo_close_tab setting
        app->cpp_data.use_ctrl_z_to_undo_close_tab_observer = global_core_api->add_single_setting_change_observer(
            orion::settings::tabs::use_ctrl_z_to_undo_close_tab,
            [app_weak,default_profile_id](orion::SingleSettingChange<bool> change){
                auto app_strong = app_weak.lock();
                if (!app_strong) return;
                OrionApp* self = ORION_APP(app_strong.get());

                bool is_enabled = change.new_value;
                orion::undo_tab_close_accel_set_enabled(
                    self,
                    is_enabled
                );
            }
        );
    });

    orion::register_ctrl_num_accels(self);
}

void
orion_app_schedule_download_for_opening(OrionApp* app, orion::DownloadId id)
{
    auto existing = std::find(app->cpp_data.downloads_to_open.cbegin(), app->cpp_data.downloads_to_open.cend(), id);
    if(existing == app->cpp_data.downloads_to_open.cend())
    {
        orion::log_verbose("scheduled for opening: %li", id.value);
        app->cpp_data.downloads_to_open.push_back(id);
    }
}

static void
orion_app_handle_downloads_change(OrionApp* app, const orion::DownloadsChange& change)
{
    for(auto& updated : change.updated)
    {
        auto download = std::find_if(app->cpp_data.downloads_to_open.cbegin(), app->cpp_data.downloads_to_open.cend(), [id = updated.id](orion::DownloadId download_to_open){ return download_to_open == id; });
        if(download != app->cpp_data.downloads_to_open.cend())
        {
            switch(updated.operation_state)
            {
                case orion::DownloadOperationState::downloading:
                    break;
                case orion::DownloadOperationState::paused:
                    break;
                case orion::DownloadOperationState::cancelled:
                    app->cpp_data.downloads_to_open.erase(download);
                    break;
                case orion::DownloadOperationState::cancelling:
                    app->cpp_data.downloads_to_open.erase(download);
                    break;
                case orion::DownloadOperationState::failed:
                    app->cpp_data.downloads_to_open.erase(download);
                    break;
                case orion::DownloadOperationState::finished:
                    app->cpp_data.downloads_to_open.erase(download);
                    {
                        orion::log_verbose("opening download");
                        std::filesystem::path path = std::filesystem::path(updated.path)/updated.name;
                        GFile* file = g_file_new_for_path(path.c_str());
                        defer([file]{ g_object_unref(file); });
                        GtkFileLauncher* launcher = gtk_file_launcher_new(file);
                        defer([launcher]{ g_object_unref(launcher); });
                        gtk_file_launcher_launch(launcher, NULL, NULL, NULL, NULL);
                    }
                    break;
            }
        }
    }
    for(auto& deleted : change.deleted)
    {
        auto download = std::find_if(app->cpp_data.downloads_to_open.cbegin(), app->cpp_data.downloads_to_open.cend(), [id = deleted](orion::DownloadId download_to_open){ return download_to_open == id; });
        if(download != app->cpp_data.downloads_to_open.cend())
        {
            app->cpp_data.downloads_to_open.erase(download);
        }
    }
}


static void
orion_app_handle_tabs_change(OrionApp* app, const orion::TabsChange& change)
{
    auto& shared_pinned_tabs = app->cpp_data.shared_tabs_vector;
    for(auto& tab : change.deleted)
    {
        auto it = std::find_if(shared_pinned_tabs.begin(), shared_pinned_tabs.end(),
            [id = tab](std::shared_ptr<OrionWebviewTabContent> content){ return id == orion_webview_tab_content_get_id(content.get()); });
        if(it!=shared_pinned_tabs.end())
        {
            shared_pinned_tabs.erase(it);
        }
    }
}

static void
font_file_replaced_cb(GObject* source, GAsyncResult* res, gpointer data)
{
    GFile* file = G_FILE(source);
    GError* err = NULL;
    if(!orion::write_file_if_needed_finish(file, res, &err))
    {
        orion::log_error("Failed to load custom font: %s", err->message);
        g_error_free(err);
        return;
    }
    
    char* path = g_file_get_path(G_FILE(source));
    if(!path) return;

    defer([path]{ g_free(path); });

    PangoFcFontMap* map = PANGO_FC_FONT_MAP(orion_app_get_reader_mode_font_map());
    FcConfig* config = pango_fc_font_map_get_config(map);
    FcConfigAppFontAddFile(config, (const FcChar8*)path);
    pango_fc_font_map_cache_clear(map);
    //NOTE: the following comment is a more modern method, but one which is unavailable on even relatively recent linux distributions
    //it also introduces a crash when a GtkFontDialogButton attempts to select a font imported in this manner
    // if (!pango_font_map_add_font_file(font_map, path, &err)) {
    //     orion::log_error("Failed to load custom font: %s", err->message);
    //     g_error_free(err);
    //     return;
    // }
}

static inline void
install_font(std::filesystem::path resource_path)
{
    GError* err = NULL;
    GBytes* bytes = g_resources_lookup_data((GRESOURCE_PREFIX / resource_path).c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
    if(err)
    {
        orion::log_error("font_dir_created_cb(): %s", err->message);
        g_error_free(err);
        return;
    }
    defer([bytes]{ g_bytes_unref(bytes); });

    auto path = orion::get_cache_directory_path(APPLICATION_COMMAND, {resource_path});
    GFile* file = g_file_new_for_path(path.c_str());
    defer([file]{ g_object_unref(file); });

    orion::write_file_if_needed_async(file, bytes, NULL, font_file_replaced_cb, NULL);
}

static void
font_dir_created_cb(GObject* source, GAsyncResult* res, gpointer data)
{
    GError* err = NULL;
    if(!orion::make_directory_with_parents_finish(G_FILE(source), res, &err))
    {
        orion::log_error("Failed to create font directory: %s", err->message);
        g_error_free(err);
        return;
    }

    install_font("fonts/OpenDyslexic3-Regular.ttf");
    install_font("fonts/Charter-Bold-Italic.otf");
    install_font("fonts/Charter-Bold.otf");
    install_font("fonts/Charter-Italic.otf");
    install_font("fonts/Charter-Regular.otf");
    install_font("fonts/Gelasio-Italic-VariableFont_wght.ttf");
    install_font("fonts/Gelasio-VariableFont_wght.ttf");
    install_font("fonts/LiberationSerif-BoldItalic.ttf");
    install_font("fonts/LiberationSerif-Bold.ttf");
    install_font("fonts/LiberationSerif-Italic.ttf");
    install_font("fonts/LiberationSerif-Regular.ttf");
    install_font("fonts/Merriweather-Italic-VariableFont_opsz,wdth,wght.ttf");
    install_font("fonts/Merriweather-VariableFont_opsz,wdth,wght.ttf");
    install_font("fonts/texgyrepagella-bolditalic.otf");
    install_font("fonts/texgyrepagella-bold.otf");
    install_font("fonts/texgyrepagella-italic.otf");
    install_font("fonts/texgyrepagella-regular.otf");
    install_font("fonts/Vollkorn-Italic-VariableFont_wght.ttf");
    install_font("fonts/Vollkorn-VariableFont_wght.ttf");
}

static void
orion_app_init_default_fonts_string_list(OrionApp* self)
{
    PangoFontMap* map = pango_cairo_font_map_get_default();
    int n_families = 0;
    PangoFontFamily** font_families_list = NULL;
    pango_font_map_list_families(map, &font_families_list, &n_families);

    defer([font_families_list]{ g_free(font_families_list); });

    std::vector<const char*> font_family_names;
    font_family_names.reserve(n_families + 2);
    font_family_names.push_back(DEFAULT_FONT_FAMILY_NAME);
    for(int i = 0; i < n_families; i++)
    {
        const char* name = pango_font_family_get_name(font_families_list[i]);
        font_family_names.push_back(name);
    }

    std::sort(font_family_names.begin()+1, font_family_names.end(), [](const char* a, const char* b){
        return g_utf8_collate(a, b) < 0;
    });

    font_family_names.push_back(NULL);

    self->default_fonts_string_list = gtk_string_list_new(font_family_names.data());
}

static void
orion_app_init_fonts(OrionApp* app)
{
    auto path = std::filesystem::path(g_get_user_cache_dir()) / APPLICATION_COMMAND / "fonts";
    GFile* file = g_file_new_for_path(path.c_str());
    defer([file]{ g_object_unref(file); });
    orion::make_directory_with_parents_async(file, NULL, font_dir_created_cb, NULL);

    orion_app_init_default_fonts_string_list(app);
}

void
orion_app_startup(OrionApp* self)
{
    setup_accels(self);

    orion_app_setup_profile_observer(self);

    global_core_api->set_filter_list_auto_update_handler(
        [self](std::vector<orion::FilterListDto> filter_lists) {
            if (!self->is_running || filter_lists.empty()) return;

            const orion::ProfileId profile_id = filter_lists.front().profile_id;
            auto profile_world = orion_app_get_profile_world(self, profile_id, orion::PrivateMode::no);
            if (auto webview_factory = profile_world
                    ? orion_app_get_web_view_factory(*profile_world).lock()
                    : nullptr) {
                orion::contentblock::setup_filter_lists_from_urls(
                    webview_factory.get(), filter_lists, true);    
            }
        });

    self->is_running = true;

    // Open the default profile
    global_core_api->get_default_profile_id([](orion::GetDefaultProfileIdReply reply) {
        VERIFY_LOG(reply.err);
        orion::ProfileId default_profile_id = reply.profile_id;

        // Run core API tests
        
        // Increase this value if the successful test regularly takes longer than this.
        auto test_timeout = 10s;
        
        auto started_at = std::chrono::steady_clock::now();
        orion::test_core_api(global_core_api.get(), test_timeout, [started_at](orion::Error err){
            if (err) {
                orion::log_error("core api test FAILED: %s", err.message().c_str());
            } else {
                auto took_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - started_at
                ).count();
                orion::log_verbose("core api test OK (took %lld ms)", took_ms);
            }
        });
        orion::crypto::crypto_test();
        orion::libsecret::test_libsecret(APPLICATION_ID);
        orion::punycode::punycode_test();
        
        //orion::libsecret::log_password_items(APPLICATION_ID);
        //orion::polkit::verify_password();
        //orion::portal_secret::test_portal_secret();
        //orion::libportal::test_libportal();

        orion_app_for_each_profile(orion_app_get_profile_fns_for_quit_and_start(true),
        [default_profile_id](orion::Error err){
            VERIFY_LOG(err);
            orion::profile::open(default_profile_id);
        });
    });

    self->cpp_data.tabs_change_observer = global_core_api->tabs_change().add_observer(
        [self](const orion::TabsChange& change){
            orion_app_handle_tabs_change(self, change);
        });
    self->cpp_data.downloads_observer = global_core_api->downloads_change().add_observer(
        [self](const orion::DownloadsChange& change){
            if(!(self->cpp_data.downloads_to_open.size()))
            {
                return;
            }
            orion_app_handle_downloads_change(self, change);
        });
    self->cpp_data.programmable_buttons_observer = global_core_api->programmable_buttons_change().add_observer(
        [](const orion::ProgrammableButtonsChange& change){
            for(auto& created: change.created)
            {
                if(created.title.empty())
                {
                    orion::UpdateProgrammableButtonQuery query{};
                    query.id = created.id.value();
                    query.update_title = true;
                    query.title = _("Programmable Button");

                    global_core_api->update_programmable_button(query, [](orion::Error err){ VERIFY_LOG(err); });
                }
            }
        }
    );
    orion_app_init_fonts(self);
}

static void
on_app_activate(GApplication* app)
{
    //OrionApp* self = ORION_APP(app);
}

static gboolean
orion_app_dbus_register(GApplication* app, GDBusConnection* conn, const char* object_path, GError** error)
{
    orion::log_verbose("orion_app_dbus_register");
    return G_APPLICATION_CLASS(orion_app_parent_class)->dbus_register(app, conn, object_path, error);
}

// note: do not use this, it will never be called, see main.cpp for details
static void
orion_app_dbus_unregister(GApplication* app, GDBusConnection* conn, const char* object_path)
{
    orion::log_verbose("orion_app_dbus_unregister");
}

static void
on_remote_activate(GAction* action, GVariant* parameter, OrionApp* self)
{
    //parameter should always be argv
    OrionWindow* win = ORION_WINDOW(gtk_application_get_active_window(GTK_APPLICATION(self)));
    if(!win) return;

    orion::profile::FocusMode focus_mode = orion::profile::FocusMode::no;
    if(G_IS_ACTION_MAP(win))
    {
        bool val = orion::gtk::get_boolean_action_state(G_ACTION_MAP(win), "toggle-focus-mode");

        if(val) focus_mode = orion::profile::FocusMode::yes;
    }

    orion::profile::open_new_window(orion_window_get_profile_id(win), focus_mode, orion_window_get_private_mode(win));
}

static void
orion_app_init(OrionApp* self)
{
    orion::log_verbose("orion_app_init");

    std::construct_at(&self->cpp_data);

    GSimpleAction* remote_activate_action = g_simple_action_new("remote-activate", G_VARIANT_TYPE("as"));
    g_signal_connect(remote_activate_action, "activate", G_CALLBACK(on_remote_activate), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(remote_activate_action));
    g_object_unref(remote_activate_action);
}

// this is workaround for proper finalize call, see main.cpp for details
void
orion_app_cleanup(OrionApp* self)
{
    orion::log_verbose("orion_app_cleanup");

    global_core_api->set_filter_list_auto_update_handler({});

    for(auto& pair : self->cpp_data.downloads)
    {
        auto& download = pair.second;
        if(const std::shared_ptr<orion::Downloader>* downloader = std::get_if<std::shared_ptr<orion::Downloader>>(&download))
        {
            (*downloader)->cancel();
        }
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        else if(const orion::gtk::WeakRefWrapper* downloader = std::get_if<orion::gtk::WeakRefWrapper>(&download))
        {
            auto downloader_strong = (*downloader).lock();
            if(downloader_strong)
            {
                webkit_download_cancel(WEBKIT_DOWNLOAD(downloader_strong.get()));
            }
        }
#endif
    }

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    for(auto& pair : self->cpp_data.webkit_user_scripts)
    {
        webkit_user_script_unref(pair.second);
    }
    g_clear_pointer(&self->reader_mode_font_style_sheet, webkit_user_style_sheet_unref);
    g_clear_pointer(&self->page_tweaks_content_blockers_hide_style_sheet, webkit_user_style_sheet_unref);
    g_clear_pointer(&self->page_tweaks_dark_mode_style_sheet, webkit_user_style_sheet_unref);
#endif
    orion_app_clear_clipboard(self);

    std::destroy_at(&self->cpp_data);
    g_clear_object(&self->icon_theme_string_list);
    g_clear_object(&self->default_fonts_string_list);

    GApplication* app = G_APPLICATION(self);
    G_APPLICATION_CLASS(orion_app_parent_class)->dbus_unregister(app, g_application_get_dbus_connection(app), g_application_get_dbus_object_path(app));
}

// note: do not use this, it will never be called, see main.cpp for details
static void
orion_app_dispose(GObject* object)
{
    G_OBJECT_CLASS(orion_app_parent_class)->dispose(object);
}

// note: do not use this, it will never be called, see main.cpp for details
static void
orion_app_finalize(GObject* object)
{
    //OrionApp* self = ORION_APP(object);
    G_OBJECT_CLASS(orion_app_parent_class)->finalize(object);
}

OrionApp*
orion_app_new()
{
	GApplicationFlags flags{};

	// gresource prefix: /com/kagi/Orion
    OrionApp* app = ORION_APP(g_object_new(ORION_APP_TYPE,
        "application-id", APPLICATION_ID,
        flags
    ));
    
	g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);

	return app;
}

static void
orion_app_class_init(OrionAppClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = orion_app_finalize;
    G_OBJECT_CLASS(klass)->dispose = orion_app_dispose;
    G_APPLICATION_CLASS(klass)->dbus_register = orion_app_dbus_register;
    G_APPLICATION_CLASS(klass)->dbus_unregister = orion_app_dbus_unregister;
    GTK_APPLICATION_CLASS(klass)->window_added = orion_app_window_added;
}
