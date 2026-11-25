#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define MANAGE_SEARCH_ENGINES_DIALOG_TYPE (manage_search_engines_dialog_get_type ())

G_DECLARE_FINAL_TYPE(ManageSearchEnginesDialog, manage_search_engines_dialog, MANAGE_SEARCH_ENGINES, DIALOG, AdwDialog)

AdwDialog* manage_search_engines_dialog_new();
void manage_search_engines_dialog_set_popover_menu(ManageSearchEnginesDialog* self, GtkPopoverMenu* popover_menu);