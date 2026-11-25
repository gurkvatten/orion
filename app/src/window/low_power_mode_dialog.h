#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define LOW_POWER_MODE_DIALOG_TYPE (low_power_mode_dialog_get_type ())

G_DECLARE_FINAL_TYPE(LowPowerModeDialog, low_power_mode_dialog, LOW_POWER_MODE, DIALOG, AdwDialog)

AdwDialog* low_power_mode_dialog_new();