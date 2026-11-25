#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define SAVE_WINDOW_POPOVER_TYPE (save_window_popover_get_type())

G_DECLARE_FINAL_TYPE(SaveWindowPopover, save_window_popover, SAVE_WINDOW, POPOVER, GtkPopover)

GtkPopover* save_window_popover_new();
void save_window_popover_set_name(SaveWindowPopover* self, const char* name);
void save_window_popover_select_name(SaveWindowPopover* self);
void save_window_popover_set_icon_name(SaveWindowPopover* self, const char* icon_name);
void save_window_popover_set_keyboard_shortcut(SaveWindowPopover* self, const char* shortcut);
void save_window_popover_set_window_id(SaveWindowPopover* self, orion::WindowId window_id);
void save_window_popover_set_delete_window_on_cancel(SaveWindowPopover* self, bool delete_window_on_cancel);