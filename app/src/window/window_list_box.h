#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include "window/main_window/main_window.h"

#define WINDOW_LIST_BOX_TYPE (window_list_box_get_type())

G_DECLARE_FINAL_TYPE(WindowListBox, window_list_box, WINDOW_LIST, BOX, GtkBox)

GtkBox* window_list_box_new();
void create_window_context_menu_async(orion::WindowId window_id, MainWindow* main_window, std::function<void(GMenu*)> callback);
void on_windows_list_view_row_secondary_released(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data);
