#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <functional>

#include <common/src/uri.h>

#include "app.h"

#define ORION_WEBVIEW_TAB_VIEW_TYPE (orion_webview_tab_view_get_type ())

// We derive from AdwBin because AdwTabView is final and cannot be derived from.
G_DECLARE_FINAL_TYPE(OrionWebviewTabView, orion_webview_tab_view, ORION, WEBVIEW_TAB_VIEW, AdwBin)

OrionWebviewTabView* orion_webview_tab_view_new();

AdwTabPage* orion_webview_tab_view_get_selected_page(OrionWebviewTabView* view);
AdwTabPage* orion_webview_tab_view_get_page(OrionWebviewTabView* view, GtkWidget* content);
int orion_webview_tab_view_get_n_pages(OrionWebviewTabView* view);
AdwTabView* orion_webview_tab_view_get_actual_view(OrionWebviewTabView* view);

void orion_webview_tab_view_set_back_button(OrionWebviewTabView* view, GtkButton* button);
void orion_webview_tab_view_set_forward_button(OrionWebviewTabView* view, GtkButton* button);
void orion_webview_tab_view_set_reload_button(OrionWebviewTabView* view, GtkButton* button);
void orion_webview_tab_view_set_transfer_next_tab_with_children(OrionWebviewTabView* view, bool val);

void orion_webview_tab_view_show_web_inspector(OrionWebviewTabView* view);
void orion_webview_tab_view_toggle_web_inspector(OrionWebviewTabView* view);
void orion_webview_tab_view_toggle_reader_mode(OrionWebviewTabView* view);
void orion_webview_tab_view_exit_reader_mode(OrionWebviewTabView* view);

const char* orion_webview_tab_view_get_title(OrionWebviewTabView* view);

void orion_webview_tab_view_set_selected_page(OrionWebviewTabView* view, AdwTabPage* page);

// Returns NULL if page with the given tab_id is not found
AdwTabPage* orion_webview_tab_view_get_page_with_tab_id(OrionWebviewTabView* view, orion::TabId tab_id);

void orion_webview_tab_view_navigation_notify(OrionWebviewTabView* view);
gboolean orion_webview_tab_view_can_go_back(OrionWebviewTabView* view);
gboolean orion_webview_tab_view_can_go_forward(OrionWebviewTabView* view);
void orion_webview_tab_view_go_back(OrionWebviewTabView* view);
void orion_webview_tab_view_go_forward(OrionWebviewTabView* view);
void orion_webview_tab_view_go_back_forward_relative_index(OrionWebviewTabView* view, int64_t i);

//expects pos to be the correct pos the page is being transferred to when group_with_existing is false
//otherwise expects pos to be the position of the parent page we are dropping into the group of
void orion_webview_tab_view_transfer_page(
    OrionWebviewTabView* view, AdwTabPage* page, int pos, AdwTabView* original_view,
    bool transfer_with_children, bool group_with_existing
);
void orion_webview_tab_view_dispose_of_pinned(OrionWebviewTabView* view);
orion::Uri orion_webview_tab_view_get_uri(OrionWebviewTabView* view);
//masks the orion://new_tab/ address if this tab is 
orion::Uri orion_webview_tab_view_get_visual_uri(OrionWebviewTabView* view);
void orion_webview_tab_view_load_uri(OrionWebviewTabView* view, orion::Uri uri);

using DoneCallback = std::function<void(void* user_data)>;

void orion_webview_tab_view_load_from_backend(
    OrionWebviewTabView* view, orion::WindowId window_id, orion::ProfileId profile_id,
    DoneCallback done_cb = nullptr, void* user_data = nullptr
);

void orion_webview_tab_view_new_tab(OrionWebviewTabView* view);

struct OrionWebviewTabViewNewTabOptions {
    bool focus_address_entry = false;
    bool autoselect = false;
    // If true, then ignore "When opening a link in a new tab, make it active" setting
    bool ignore_when_opening_setting = false;
    // If true, then ignore "Open new tab next to current tab" setting and always open the tab next to the current one
    bool force_open_next_to_current = false;
};

void orion_webview_tab_view_new_tab_with_uri(
    OrionWebviewTabView* view, const char* uri, const OrionWebviewTabViewNewTabOptions& options = {});

void orion_webview_tab_view_load_bookmark_folder(OrionWebviewTabView* view, orion::BookmarkFolderId id);
void orion_webview_tab_view_load_bookmark(OrionWebviewTabView* view, orion::BookmarkId id);
OrionWebviewTabContent* orion_webview_tab_view_get_current_tab_content(OrionWebviewTabView* self);
void orion_webview_tab_view_select_next_created_tab_with_id(OrionWebviewTabView* view, orion::TabId id);
