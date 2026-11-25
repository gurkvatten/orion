#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#include "window/main_window/header/tab_bar.h"

#define APP_HEADER_BAR_TYPE (header_bar_get_type ())

// Use GtkBox instead of AdwBin just because of an issue with
// disposing the template by calling gtk_widget_dispose_template
G_DECLARE_FINAL_TYPE(HeaderBar, header_bar, APP, HEADER_BAR, GtkBox)

typedef void (*ToggleSidebarCallback)(gpointer user_data, gboolean active);

GtkButton* header_bar_get_window_list_button(HeaderBar* self);
GtkButton* header_bar_get_sidebar_button(HeaderBar* self);
GtkButton* header_bar_get_back_button(HeaderBar* self);
GtkButton* header_bar_get_forward_button(HeaderBar* self);
GtkMenuButton* header_bar_get_menu_button(HeaderBar* self);
GtkButton* header_bar_get_focus_mode_button(HeaderBar* self);
GtkButton* header_bar_get_share_button(HeaderBar* self);
GtkButton* header_bar_get_tab_overview_button(HeaderBar* self);
GtkButton* header_bar_get_downloads_button(HeaderBar* self);
GtkButton* header_bar_get_privacy_button(HeaderBar* self);
GtkButton* header_bar_get_page_tweaks_button(HeaderBar* self);
GtkButton* header_bar_get_website_settings_button(HeaderBar* self);
GtkButton* header_bar_get_profiles_button(HeaderBar* self);
GtkBox* header_bar_get_profiles_button_box(HeaderBar* self);
GtkButton* header_bar_get_address_focus_mode_button(HeaderBar* self);
GtkButton* header_bar_get_reader_mode_button(HeaderBar* self);
GtkButton* header_bar_get_rss_feeds_button(HeaderBar* self);
GtkButton* header_bar_get_certificates_button(HeaderBar* self);
GtkEntry* header_bar_get_address_entry(HeaderBar* self);
GtkButton* header_bar_get_reload_page_button(HeaderBar* self);
GtkButton* header_bar_get_add_page_to_button(HeaderBar* self);
GtkButton* header_bar_get_new_tab_button(HeaderBar* self);
GtkPopoverMenu* header_bar_get_new_tab_button_popover_menu(HeaderBar* self);
GtkButton* header_bar_get_history_button(HeaderBar* self);
TabBar* header_bar_get_compact_page_content_tabbar(HeaderBar* self);
void header_bar_set_sidebar_button_toggled_callback(HeaderBar* self, ToggleSidebarCallback cb, gpointer user_data);
void header_bar_set_sidebar_button_visible(HeaderBar* self, gboolean visible);
void header_bar_popdown_all_popovers(HeaderBar* self);
GtkButton* header_bar_get_page_zoom_button(HeaderBar* self);
GtkButton* header_bar_get_popup_blocked_button(HeaderBar* self);
void header_bar_show_popup_blocked_message(HeaderBar* self);
