#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <core/webextensions/runtime.h>
#include <webextensions/webview_factory_host.h>

#include "app.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_view.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_container.h"
#include "window/save_window_popover.h"
#include "window/main_window/header/header_bar.h"
#include "window/main_window/sidebar/sidebar.h"
#include "window/main_window/header/tab_bar.h"
#include "window/main_window/header/erase_elements_bar.h"
#include "window/profiles_icon_selection_box.h"
#include "window/main_window/header/bookmarks_bar.h"
#include "window/main_window/header/find_bar.h"
#include "util/profile_manager.h"

#define MAIN_WINDOW_TYPE (main_window_get_type ())

G_DECLARE_FINAL_TYPE(MainWindow, main_window, MAIN, WINDOW, AdwApplicationWindow)

struct ContextMenuData {
    gpointer user_data{};
    std::vector<orion::gtk::GObjectPtr> selected_items{}; //used in bookmarks columnviews (in Sidebar and Bookmarks dialog) at the moment
    
    ContextMenuData() = default;

    ContextMenuData(const ContextMenuData&) = delete;
    ContextMenuData& operator=(const ContextMenuData&) = delete;

    ContextMenuData(ContextMenuData&&) = default;
    ContextMenuData& operator=(ContextMenuData&&) = default;
};

using MainWindowNewFromBackendCb = std::function<void(MainWindow*)>;

namespace orion::gtk {

// Returns NULL if the widget is not inside a MainWindow
MainWindow* get_root_main_window(GtkWidget* widget);

} // namespace orion::gtk

MainWindow* main_window_new_from_backend(
    GApplication* app, orion::ProfileId profile_id, orion::WindowId window_id,
    orion::PrivateMode private_mode, MainWindowNewFromBackendCb done_cb = nullptr);

MainWindow* main_window_new(
    GApplication* app, orion::ProfileId profile_id, orion::WindowId window_id,
    orion::PrivateMode private_mode);

SaveWindowPopover* main_window_on_save_window(MainWindow* win);
OrionWebviewTabView* main_window_get_webview_tab_view(MainWindow* win);
Sidebar* main_window_get_sidebar(MainWindow* win);
AdwOverlaySplitView* main_window_get_js_sidebar_split_view(MainWindow* win);
TabBar* main_window_get_tab_bar(MainWindow* win);
GtkPopover* main_window_get_window_list_popover(MainWindow* win);
//currently only set by right button_released_handler
ContextMenuData* main_window_get_context_menu_data(MainWindow* win);
//currently only set by right button_released_handler
GtkWidget* main_window_get_last_right_clicked_widget(MainWindow* win);
double main_window_get_mouse_x(MainWindow* win);
double main_window_get_mouse_y(MainWindow* win);
void main_window_set_last_right_clicked_widget(MainWindow* win, GtkWidget* widget);
void main_window_set_context_menu_data(MainWindow* win, ContextMenuData data);
void main_window_set_test_popover(MainWindow* win, GtkPopover* popover);
void main_window_on_edit_programmable_button(MainWindow* win, orion::ProgrammableButtonId id);
void main_window_activate_programmable_button_action(MainWindow* win, orion::ProgrammableButtonId id);
void main_window_switch_to_sidebar_page(MainWindow* win, const char* page_name);
void main_window_popdown_all_popovers(MainWindow* win);
void main_window_popup_settings_for_website(MainWindow* self);
orion::ProfileId main_window_get_current_profile_id(MainWindow* win);
WebExtensionsRuntime& main_window_get_current_extensions_runtime(MainWindow* win);
WebExtensionWebViewFactory& main_window_get_current_webview_factory(MainWindow* win);
orion::WindowId main_window_get_window_id(MainWindow* win);
GtkPopover* main_window_get_reader_unavailable_popover(MainWindow* win);
// Call this when JavaScript requested fullscreen (eg., YouTube video)
void main_window_set_is_webkit_fullscreen(MainWindow* window, gboolean is_fullscreened);
// When called from JavaScript for fullscreen (eg., YouTube video), set from_webkit to TRUE
void main_window_set_is_borderless(MainWindow* window, gboolean is_borderless, gboolean from_webkit);
void main_window_show_link_overlay(MainWindow* window, const char* url);
void main_window_hide_link_overlay(MainWindow* window);
OrionWebviewTabContent* main_window_get_current_tab_content(MainWindow* self);
void main_window_show_find_bar(MainWindow* self);
void main_window_show_find_bar_with_text(MainWindow* self, const std::string& text);
void main_window_hide_find_bar(MainWindow* self);
void main_window_refresh_find_search(MainWindow* self);
gboolean main_window_get_find_bar_visible(MainWindow* self);
void main_window_load_bookmark(MainWindow* win, std::string bookmark_address);
void main_window_set_show_sidebar_setting(MainWindow* self, gboolean is_visible);
void main_window_toggle_show_sidebar_setting(MainWindow* self);
void main_window_close_all_windows(MainWindow* self);
void on_new_tab_button_clicked(MainWindow* win);
orion::TabId main_window_get_current_tab_id(MainWindow* win);
void main_window_switch_to_window_id_in_place(MainWindow* win, orion::WindowId window_id);
void main_window_close(MainWindow* win);
void main_window_switch_to_tab(MainWindow* win, orion::TabDto tab);
void main_window_switch_to_previous_tab(MainWindow* win);
void main_window_switch_to_next_tab(MainWindow* win);
void main_window_show_previous_tab_accel(MainWindow* win);
void main_window_show_next_tab_accel(MainWindow* win);
void main_window_show_tab_switcher(MainWindow* win);
OrionWebviewTabView* main_window_set_current_tab_view_by_window_id(
    MainWindow* self, orion::WindowId window_id, bool load_from_backend
);
void main_window_autoselect_next_created_tab_with_id(MainWindow* win, orion::TabId id);
void main_window_reload_tab_by_tab_id(MainWindow* win, orion::TabId id);
void main_window_reload_current_tab_bypass_cache(MainWindow* self);
void main_window_reload_all_tabs(MainWindow* self);
void main_window_set_main_menu_model(MainWindow* self, GMenuModel* model);
GMenuModel* main_window_get_main_menu_model(MainWindow* self);
void main_window_set_recently_bookmarked_menu(MainWindow* self, GMenu* menu);
GMenu* main_window_get_recently_bookmarked_menu(MainWindow* self);
void main_window_set_recently_bookmarked_change_observer(MainWindow* self, orion::ObserverId id);
orion::ObserverId main_window_get_recently_bookmarked_change_observer(MainWindow* self);
void main_window_set_all_folders_change_observer(MainWindow* self, orion::ObserverId id);
orion::ObserverId main_window_get_all_folders_change_observer(MainWindow* self);
void main_window_set_all_bookmarked_change_observer(MainWindow* self, orion::ObserverId id);
orion::ObserverId main_window_get_all_bookmarked_change_observer(MainWindow* self);
void main_window_set_all_bookmarks_section(MainWindow* self, GMenu* section);
GMenu* main_window_get_all_bookmarks_section(MainWindow* self);
HeaderBar* main_window_get_header_bar(MainWindow* self);
EraseElementsBar* main_window_get_erase_elements_bar(MainWindow* self);

ProfileDataHolder* main_window_get_profile_data_holder(MainWindow* win);

void main_window_focus(MainWindow* win, std::function<void()> cb = nullptr);
void on_profiles_button_icon_selection_box_map(ProfilesIconSelectionBox* self, gpointer user_data);
gboolean on_tab_bar_drop_target_drop(
    GtkDropTarget* self, const GValue* value, gdouble x, gdouble y, gpointer user_data
);
BookmarksBar* main_window_get_bookmarks_bar(MainWindow* self);
OrionWebviewTabContainer* main_window_get_orion_webview_tab_container_by_tab_id(MainWindow* win, orion::TabId tab_id);
OrionWebviewTabContent* main_window_get_orion_webview_tab_content_by_tab_id(MainWindow* self, orion::TabId tab_id);

//aggregate is-playing-audio/is-muted state of every tab in the window except current_tab_id
struct OtherTabsAudioState
{
    bool any_playing = false;
    bool any_playing_unmuted = false;
};
OtherTabsAudioState main_window_get_other_tabs_audio_state(MainWindow* win, orion::TabId current_tab_id);
void main_window_refresh_bookmark_icon(MainWindow* self);
gboolean main_window_get_is_borderless(MainWindow* self);
gboolean main_window_get_is_webkit_fullscreen(MainWindow* self);
void main_window_show_tab_overview(MainWindow* self);
void main_window_hide_tab_overview(MainWindow* self);
void main_window_set_settings_dialog_open(MainWindow* self, gboolean open);
void main_window_stop_loading_current_tab(MainWindow* self);
void main_window_zoom_in(MainWindow* self);
void main_window_zoom_out(MainWindow* self);
void main_window_reset_zoom(MainWindow* self);
void main_window_update_page_zoom_button(MainWindow* self, int64_t zoom_percent);
void main_window_update_popup_blocked_button(MainWindow* self, guint count);
orion::PrivateMode main_window_get_private_mode(MainWindow* self);
void main_window_show_privacy_popover(MainWindow* self);
void main_window_toggle_reader(MainWindow* self);
void main_window_exit_reader_mode(MainWindow* self);
// Returns empty string if current tab content cannot be determined
std::string main_window_get_current_website(MainWindow* self);
// Called when the website in current tab changes (eg., when navigating to a new page or when switching to another tab)
void main_window_on_current_website_change(MainWindow* self);
void main_window_update_enable_compatibility_mode_action_state(MainWindow* self, std::string website);
void main_window_show_certificates_dialog(MainWindow* self);
void main_window_show_popup_blocked_message(MainWindow* self);

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
WebKitUserScript* main_window_get_reader_template_script(MainWindow* self);
#endif