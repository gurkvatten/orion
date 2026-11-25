#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

// TODO: rename this to WindowListDialog
#define WINDOW_LIST_POPOVER_TYPE (window_list_popover_get_type())

G_DECLARE_FINAL_TYPE(WindowListPopover, window_list_popover, WINDOW_LIST, POPOVER, AdwDialog)

AdwDialog* window_list_popover_new();