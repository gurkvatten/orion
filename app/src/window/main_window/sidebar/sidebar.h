#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "util/click_util.h"
#include <string>
#include "window/orion_webview_tabview_widgets/orion_webview_tab_view.h"

#define APP_SIDEBAR_TYPE (sidebar_get_type())

G_DECLARE_FINAL_TYPE(Sidebar, sidebar, APP, SIDEBAR, GtkBox)

typedef void(*SidebarButtonClickedCallback)(gpointer user_data);

void sidebar_set_sidebar_button_clicked_callback(Sidebar* self, SidebarButtonClickedCallback cb, gpointer user_data);
void sidebar_setup_tabs_lists(Sidebar* self, OrionWebviewTabView* tab_view);
void sidebar_switch_to_page(Sidebar* self, const char* page_name);
void sidebar_popdown_all_popovers(Sidebar* self);
GtkPopoverMenu* sidebar_get_bookmarks_folder_popover_menu(Sidebar* self);
void sidebar_set_bookmarks_folder_popover_menu(Sidebar* self, GtkPopoverMenu* popover_menu);
GtkPopoverMenu* sidebar_get_bookmarks_internal_folder_popover_menu(Sidebar* self);
void sidebar_set_bookmarks_internal_folder_popover_menu(Sidebar* self, GtkPopoverMenu* popover_menu);
GtkPopoverMenu* sidebar_get_bookmarks_bookmark_popover_menu(Sidebar* self);
void sidebar_set_bookmarks_bookmark_popover_menu(Sidebar* self, GtkPopoverMenu* popover_menu);
GtkColumnView* sidebar_get_bookmarks_column_view(Sidebar* self);
GtkPopoverMenu* sidebar_get_bookmarks_column_view_popover_menu(Sidebar* self);
const char* sidebar_get_visible_page(Sidebar* self);
bool get_sidebar_readinglist_show_read(Sidebar* self);
std::string get_sidebar_readinglist_query(Sidebar* self);
GtkPopoverMenu* sidebar_get_tabs_list_view_popover_menu(Sidebar* self);
GtkPopoverMenu* sidebar_get_new_tab_button_popover_menu(Sidebar* self);
GtkListView* sidebar_get_tabs_list_view(Sidebar* self);
GtkGridView* sidebar_get_pinned_tabs_grid_view(Sidebar* self);
GtkSelectionModel* sidebar_get_pinned_tabs_selection(Sidebar* self);
void sidebar_set_pinned_tabs_selection(Sidebar* self, GtkSelectionModel* selection);
GtkListView* sidebar_get_unpinned_tabs_list_view(Sidebar* self);
GtkMenuButton* sidebar_get_current_window_menu_button(Sidebar* self);
GtkLabel* sidebar_get_tabs_label(Sidebar* self);
void sidebar_set_unpinned_tabs_selection(Sidebar* self, GtkSelectionModel* selection);
GtkSelectionModel* sidebar_get_unpinned_tabs_selection(Sidebar* self);
bool sidebar_get_show_favicons_inside_tabs(Sidebar* self);
void sidebar_set_mini_toolbar_enabled(Sidebar* self, bool enabled);
void sidebar_deactivate_other_page_toggles(Sidebar* self, const char* action_name);
orion::settings::SidebarPage sidebar_page_name_to_page_type(std::string page_name);
std::string sidebar_page_type_to_toggle_action(orion::settings::SidebarPage page);