#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define OVERFLOW_MENU_POPOVER_TYPE (overflow_menu_popover_get_type())

G_DECLARE_FINAL_TYPE(OverflowMenuPopover, overflow_menu_popover, WINDOW_LIST, POPOVER, GtkPopover)

GtkPopover* overflow_menu_popover_new();