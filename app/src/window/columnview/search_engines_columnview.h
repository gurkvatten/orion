#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <vector>
#include "app.h"

void search_engines_column_view_realize(GtkWidget* self, gpointer user_data);
void search_engines_column_view_map(GtkWidget* self, gpointer user_data);
void search_engines_column_view_unmap(GtkWidget* self, gpointer user_data);
void search_engines_column_view_get_selected_search_engine_ids(
    GtkColumnView* column_view, std::vector<orion::SearchEngineId>* out_selected_ids
);