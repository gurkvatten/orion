#pragma once

#include <gtk/gtk.h>

void on_overflow_menu_listview_map(GtkListView* self, gpointer user_data);
void overflow_menu_listview_init(GtkListView* self);
void overflow_menu_listview_finalize(GtkListView* self);