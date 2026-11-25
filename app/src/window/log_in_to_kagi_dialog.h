#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define LOG_IN_TO_KAGI_DIALOG_TYPE (log_in_to_kagi_dialog_get_type())

G_DECLARE_FINAL_TYPE(LogInToKagiDialog, log_in_to_kagi_dialog, LOG_IN_TO_KAGI, DIALOG, AdwDialog)

AdwDialog* log_in_to_kagi_dialog_new();