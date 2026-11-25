#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define INVALID_SETUP_KEY_DIALOG_TYPE (invalid_setup_key_dialog_get_type ())

G_DECLARE_FINAL_TYPE(InvalidSetupKeyDialog, invalid_setup_key_dialog, INVALID_SETUP_KEY, DIALOG, AdwDialog)

AdwDialog* invalid_setup_key_dialog_new();