#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define CHECK_FOR_UPDATES_DIALOG_TYPE (check_for_updates_dialog_get_type ())

G_DECLARE_FINAL_TYPE(CheckForUpdatesDialog, check_for_updates_dialog, CHECK_FOR_UPDATES, DIALOG, AdwDialog)

AdwDialog* check_for_updates_dialog_new();