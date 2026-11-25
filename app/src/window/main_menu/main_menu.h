#pragma once

#include "window/main_window/main_window.h"

#include <gtk/gtk.h>

void main_window_setup_main_menu_button(MainWindow* win);

GListModel* create_main_menu_list_model();

// Workaround to avoid submenu name collisions in sliding GMenu
extern int global_window_counter;