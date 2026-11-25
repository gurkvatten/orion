#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

void sidebar_bookmarks_column_view_map(GtkColumnView* bookmarks_columnview, gpointer user_data);
void sidebar_bookmarks_column_view_map_flat_query(GtkColumnView* bookmarks_columnview, gpointer user_data, std::string query);

void sidebar_bookmarks_column_view_realize(GtkColumnView* bookmarks_columnview, gpointer user_data);

void on_sidebar_bookmarks_column_view_activate(GtkColumnView* view, guint pos, gpointer user_data);