#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <string>

#include "app.h"

char* get_extension_title_label(GtkListItem* listitem);
char* get_extension_source_label(GtkListItem* listitem);
char* get_extension_icon_ref(GtkListItem* listitem);
bool get_extension_keyboard_shortcuts_button_visible(GtkListItem* listitem);

void on_extension_keyboard_shortcuts_button_clicked(GtkButton* self, gpointer user_data);
void on_extension_uninstall_button_clicked(GtkButton* self, gpointer user_data);
bool on_extension_enable_switch_state_set(GtkSwitch* self, gboolean state, gpointer user_data);
void on_extension_enable_switch_map(GtkSwitch* self, gpointer user_data);
void on_extension_menu_button_map(GtkMenuButton* self, gpointer user_data);
void on_extensions_list_view_map(GtkListView* view, gpointer user_data);