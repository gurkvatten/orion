#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define SEARCH_COMPLETION_POPOVER_TYPE (search_completion_popover_get_type ())

G_DECLARE_FINAL_TYPE(SearchCompletionPopover, search_completion_popover, SEARCH_COMPLETION, POPOVER, GtkPopover)

GtkPopover* search_completion_popover_new();