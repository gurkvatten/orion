#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

void history_column_view_realize(GtkColumnView* historyColumnView, BookmarksDialog* win);
void history_column_view_map(GtkColumnView* historyColumnView, BookmarksDialog* win);
void history_column_view_unmap(GtkColumnView* historyColumnView, gpointer user_data);
