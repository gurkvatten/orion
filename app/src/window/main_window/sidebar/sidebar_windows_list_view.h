#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

void window_item_factory_bind(GtkSignalListItemFactory* factory, GtkListItem* list_item, gpointer user_data);
void window_item_factory_setup(GtkSignalListItemFactory* factory, GtkListItem* list_item, gpointer user_data);
