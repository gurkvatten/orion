#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

void reading_list_column_view_map(GtkColumnView* reading_list_columnview, BookmarksDialog* win);
void reading_list_column_view_realize(GtkColumnView* reading_list_columnview, BookmarksDialog* win);