#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define RESET_ORION_DIALOG_TYPE (reset_orion_dialog_get_type ())

G_DECLARE_FINAL_TYPE(ResetOrionDialog, reset_orion_dialog, RESET_ORION, DIALOG, AdwDialog)

AdwDialog* reset_orion_dialog_new();