#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include "data_item/reading_list_columnview_item.h"

void on_reading_list_list_view_map(GtkListView* view, gpointer user_data);

void on_reading_list_list_view_row_secondary_released(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
void on_reading_list_list_view_secondary_released(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);


void load_image_from_reading_list_item(GtkImage* image, gpointer user_data);
void load_description_label_from_reading_list_item(GtkLabel* label, gpointer user_data);

char* get_reading_list_title_from_item(GtkListItem* listitem);
char* get_reading_list_address_from_item(GtkListItem* listitem);
char* get_reading_list_description_from_item(GtkListItem* listitem);

void reading_list_list_view_refetch_data(GtkListView* view, std::string query, bool show_unread);

