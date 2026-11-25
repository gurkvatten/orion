#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

void profiles_column_view_map(GtkColumnView* profiles_list_columnview, BookmarksDialog* win);
void profiles_column_view_realize(GtkColumnView* profiles_list_columnview, BookmarksDialog* win);