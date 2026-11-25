#pragma once

#ifdef NDEBUG
#define G_DISABLE_ASSERT
#endif

#include <gtk/gtk.h>
#include <adwaita.h>
#include <glib/gi18n.h>

#include <core/core_api.h>
#include <global_core_api.h>
#include <common/src/memory.h>
#include <common/src/coroutine.h>

#include "app_profile_data_holder.h"

#include "webextensions/webview_factory_host.h"

#include "util/kagi_service.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

#define USER_INPUT_DEBOUNCE_DURATION_US 400000
#define MAX_CONTEXT_MENU_USER_STRING_LENGTH 50
#define READABILITY_SCRIPT_WORLD "readability"
#define PAGE_TWEAKS_SCRIPT_WORLD "pageTweaks"
#define LEAVE_FULLSCREEN_SCRIPT_WORLD "leaveFullscreen"
#define PROGRAMMABLE_BUTTON_SCRIPT_WORLD "programmableButton"
#define DEFAULT_FONT_FAMILY_NAME "(default)"
#define DEFAULT_FONT_LABEL _("Default")
#define AWAIT(function, ...) co_await async(invoke(global_core_api,&orion::CoreApi::function),##__VA_ARGS__)

// #if !defined(_)
// #   define _(x) x
// #endif

#include "util/ref_util.h"

namespace orion {
//refers to the different queries 
enum class clipboard_data_type {
    no_data,
    bookmarks_copy_query, //orion::CopyBookmarksQuery
};

enum class ScriptType {
    readability_js,
    auto_reader,
    element_picker,
    certificate_warning,
};
}

#define ORION_APP_TYPE (orion_app_get_type())

G_DECLARE_FINAL_TYPE(OrionApp, orion_app, ORION, APP, AdwApplication)

OrionApp* orion_app_new();
void orion_app_quit();
void orion_app_startup(OrionApp* self);
bool orion_app_is_running(OrionApp* app);
bool orion_app_is_quitting(OrionApp* app);
bool orion_app_preserve_windows_on_quit(OrionApp* app);
void orion_app_cleanup(OrionApp* self);

// Profile management helpers
void orion_app_cleanup_profile_resources(
    OrionApp* app, orion::ProfileId profile_id, std::vector<orion::PrivateMode> private_modes
);

void orion_app_wait_for_profile_async_ops(
    OrionApp* self, orion::ProfileId profile_id, std::vector<orion::PrivateMode> private_modes, orion::Fn ready_cb
);

orion::clipboard_data_type orion_app_get_clipboard_data_type(OrionApp* app);

void* orion_app_take_clipboard_data(OrionApp* app);
//owned by the instance, caller should make a copy
const void* orion_app_get_clipboard_data(OrionApp* app);
void orion_app_set_clipboard_data(OrionApp* app, void* data, orion::clipboard_data_type type);

typedef struct _OrionWebviewTabContent OrionWebviewTabContent;

OrionWebviewTabContent* orion_app_get_shared_tab(OrionApp* app, orion::TabId tab_id);
void orion_app_add_shared_pinned_tab(OrionApp* app, OrionWebviewTabContent* content);
void orion_app_update_theme(OrionApp* app, orion::ProfileId profile_id);


std::variant<std::monostate, std::shared_ptr<orion::Downloader>, orion::gtk::WeakRefWrapper> orion_app_get_downloader_for_id(
    OrionApp* app, orion::DownloadId id);

void orion_app_add_download_with_id(
    OrionApp* app, std::variant<std::monostate, std::shared_ptr<orion::Downloader>,
    orion::gtk::WeakRefWrapper> download, orion::DownloadId id);

bool orion_app_remove_download_for_id(OrionApp* app, orion::DownloadId id);

void orion_app_schedule_download_for_opening(OrionApp* app, orion::DownloadId id);

GtkStringList* orion_app_get_icon_theme_string_list(OrionApp* app);

//weird naming scheme to indicate that app reference counts these objects to know when to drop its own strong reference
ProfileDataHolder* orion_app_ref_profile_data_holder(orion::ProfileId id);
void orion_app_unref_profile_data_holder(orion::ProfileId id);

//will clear previously set accels, only handles GTK_PHASE_BUBBLE and GTK_PHASE_CAPTURE
void orion_app_set_accels_for_action(OrionApp* app, const std::string& action_name, const std::vector<std::string>& accels, GtkPropagationPhase phase);

template <typename T>
using Cb = std::function<void(T)>;

PangoFontMap* orion_app_get_reader_mode_font_map();
//also includes a font labeled based on DEFAULT_FONT_FAMILY_NAME at the start of this list, rest is a list sorted based on g_utf8_collate
GtkStringList* orion_app_get_default_fonts_string_list();
//to be used in widget factories for font list widgets:
PangoAttrList* get_font_attributes_for_string_list_item(GtkListItem* item, const gchar* name);
gchar* get_font_label_for_string_list_item(GtkListItem* item, const gchar* name);

//lifetime of these references is guaranteed until orion_app_cleanup is run
std::string& orion_app_get_script_string(orion::ScriptType type);
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
WebKitUserScript* orion_app_get_script(orion::ScriptType type);
WebKitUserStyleSheet* orion_app_get_reader_mode_font_style_sheet();
WebKitUserStyleSheet* orion_app_get_page_tweaks_dark_mode_style_sheet();
#endif
std::string orion_app_get_dyslexic_font_base64();
std::shared_ptr<orion::bridge::ProfileHost> orion_app_get_or_create_profile_host(
    orion::ProfileId profile_id);

// Activate only the world requested by a newly opened window.
std::weak_ptr<orion::bridge::ProfileWorld> orion_app_activate_profile_world(
    orion::ProfileId profile_id, orion::PrivateMode private_mode);
// Lookup never creates or activates a native factory.
std::weak_ptr<WebExtensions::WebExtensionWebViewFactory> orion_app_get_web_view_factory(
    orion::bridge::ProfileWorld& world);

std::shared_ptr<orion::KagiService> orion_app_get_or_create_kagi_service(
    orion::bridge::ProfileWorld& profile_world);

struct QuitCbItem {
    std::string name;
    std::function<void(std::function<void(orion::Error)>)> cb;
};

void orion_app_register_quit_callback(OrionApp* self, QuitCbItem item);


template<size_t N>
struct StaticString {
    char data[N];
    constexpr StaticString(const char(&str)[N]) {
        for(size_t i = 0; i<N; ++i) data[i] = str[i];
    }
    constexpr std::string_view view() const { 
        return {data, N - 1}; 
    }
};
