#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define SEARCH_POPOVER_TYPE (search_popover_get_type())

G_DECLARE_FINAL_TYPE(SearchPopover, search_popover, SEARCH, POPOVER, GtkPopover)

GtkPopover* search_popover_new();