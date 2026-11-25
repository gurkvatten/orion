#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <unordered_set>
#include "app.h"
#include "orion_webview_tab_view.h"
#include "orion_webkit_webview.h"
#include "window/main_window/orion_window_iface.h"

#define ORION_WEBVIEW_TAB_CONTENT_TYPE (orion_webview_tab_content_get_type ())

G_DECLARE_FINAL_TYPE(OrionWebviewTabContent, orion_webview_tab_content, ORION, WEBVIEW_TAB_CONTENT, AdwBin)

enum class UpdateBackend { no, yes };

//should only ever be used from OrionWebviewTabView
OrionWebviewTabContent* orion_webview_tab_content_new_from_dto(orion::TabDto dto, std::vector<orion::TabBackForwardListEntryDto> back_forward_list);
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
OrionWebviewTabContent* orion_webview_tab_content_new_from_webview(orion::TabId id, OrionWebkitWebview* source);
#endif

gboolean orion_webview_tab_content_can_go_back(OrionWebviewTabContent* content);
gboolean orion_webview_tab_content_can_go_forward(OrionWebviewTabContent* content);
void orion_webview_tab_content_navigate_to_relative_bf_list_index(OrionWebviewTabContent* content, int64_t relative_index);
void orion_webview_tab_content_go_back(OrionWebviewTabContent* content);
void orion_webview_tab_content_go_forward(OrionWebviewTabContent* content);
void orion_webview_tab_content_reload(OrionWebviewTabContent* content);
void orion_webview_tab_content_show_web_inspector(OrionWebviewTabContent* content);
void orion_webview_tab_content_toggle_web_inspector(OrionWebviewTabContent* content);
void orion_webview_tab_content_close(OrionWebviewTabContent* content, std::function<void(orion::Error)> done_cb = nullptr);

void orion_webview_tab_content_load_uri(OrionWebviewTabContent* content, orion::Uri uri);

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
WebKitWebView* orion_webview_tab_content_get_webview(OrionWebviewTabContent* content);
const std::unordered_set<std::string>& orion_webview_tab_content_get_current_page_resource_domains(OrionWebviewTabContent* content);
#endif

GIcon* orion_webview_tab_content_get_icon(OrionWebviewTabContent* content);
orion::TabId orion_webview_tab_content_get_id(OrionWebviewTabContent* content);
orion::Uri orion_webview_tab_content_get_uri(OrionWebviewTabContent* content);
std::string orion_webview_tab_content_get_title(OrionWebviewTabContent* content);
OrionWebviewTabView* orion_webview_tab_content_get_tabview(OrionWebviewTabContent* content);
bool orion_webview_tab_content_get_is_loading(OrionWebviewTabContent* content);
gboolean orion_webview_tab_content_get_is_suspended(OrionWebviewTabContent* content);
gboolean orion_webview_tab_content_get_is_readerable(OrionWebviewTabContent* content);
gboolean orion_webview_tab_content_get_reader_mode_enabled(OrionWebviewTabContent* content);
gboolean orion_webview_tab_content_get_is_playing_audio(OrionWebviewTabContent* content);
gboolean orion_webview_tab_content_get_is_muted(OrionWebviewTabContent* content);
orion::TlsStatus orion_webview_tab_content_get_tls_status(OrionWebviewTabContent* content);
orion::BackForwardList* orion_webview_tab_content_get_back_list(OrionWebviewTabContent* content);
orion::BackForwardList* orion_webview_tab_content_get_forward_list(OrionWebviewTabContent* content);
void orion_webview_tab_content_set_is_muted(OrionWebviewTabContent* content, gboolean muted);
void orion_webview_tab_content_stop_loading(OrionWebviewTabContent* content);
void orion_webview_tab_content_reload_bypass_cache(OrionWebviewTabContent* content);
void orion_webview_tab_content_set_zoom(OrionWebviewTabContent* content, orion::settings::PageZoom zoom);

void orion_webview_tab_content_find_search(OrionWebviewTabContent* content,
    const std::string& text,gboolean ignore_case, gboolean wrap_around, gboolean starts_with);
void orion_webview_tab_content_find_next(OrionWebviewTabContent* content);
void orion_webview_tab_content_find_previous(OrionWebviewTabContent* content);
void orion_webview_tab_content_find_finish(OrionWebviewTabContent* content);
void orion_webview_tab_content_set_find_result_callback(OrionWebviewTabContent* content, std::function<void(guint, gboolean)> callback);
void orion_webview_tab_content_get_selected_text(OrionWebviewTabContent* content, std::function<void(std::string)> done_cb);
void orion_webview_tab_content_scroll_to_selection(OrionWebviewTabContent* content);
void orion_webview_tab_content_change_zoom(OrionWebviewTabContent* self, int64_t delta, bool reset);
void orion_webview_tab_content_sync_webview_zoom_level(OrionWebviewTabContent* self);
void orion_webview_tab_content_evaluate_javascript(OrionWebviewTabContent* self, const char* script, gssize length,
    const char* world_name, const char* source_uri, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer data);


//TODO: migrate these things over to the tab container
//this automatically notifies backend of the change
//to be exclusively used in OrionWebviewTabView

//to be exclusively used in OrionTabSelectionModel
//returns whether value was modified or not
void orion_webview_tab_content_set_is_suspended(OrionWebviewTabContent* content, bool is_suspended);

void orion_webview_tab_content_suspend_webview(OrionWebviewTabContent* content);

void orion_webview_tab_content_add_blocked_popup(OrionWebviewTabContent* content, const std::string& url);
const std::vector<std::string>& orion_webview_tab_content_get_blocked_popup_urls(OrionWebviewTabContent* content);
void orion_webview_tab_content_clear_blocked_popups(OrionWebviewTabContent* content);
void orion_webview_tab_content_set_edit_text_on_page(OrionWebviewTabContent* self, bool edit_text_on_page);
bool orion_webview_tab_content_get_edit_text_on_page(OrionWebviewTabContent* self);
void orion_webview_tab_content_force_allow_copy_and_paste(OrionWebviewTabContent* self);
void orion_webview_tab_content_start_element_picker(OrionWebviewTabContent* self);
void orion_webview_tab_content_end_element_picker(OrionWebviewTabContent* self);
void orion_webview_tab_content_set_preview_erased(OrionWebviewTabContent* self, std::string_view selectors, std::string_view host, gboolean value);
void orion_webview_tab_content_update_preview_erased_selectors(OrionWebviewTabContent* self, std::string_view selectors);
