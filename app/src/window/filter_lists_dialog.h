#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define FILTER_LISTS_DIALOG_TYPE (filter_lists_dialog_get_type ())

G_DECLARE_FINAL_TYPE(FilterListsDialog, filter_lists_dialog, FILTER_LISTS, DIALOG, AdwDialog)

AdwDialog* filter_lists_dialog_new();