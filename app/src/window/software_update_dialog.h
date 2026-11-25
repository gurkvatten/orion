#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define SOFTWARE_UPDATE_DIALOG_TYPE (software_update_dialog_get_type ())

G_DECLARE_FINAL_TYPE(SoftwareUpdateDialog, software_update_dialog, SOFTWARE_UPDATE, DIALOG, AdwDialog)

AdwDialog* software_update_dialog_new();