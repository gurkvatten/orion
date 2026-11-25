#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define UP_TO_DATE_DIALOG_TYPE (up_to_date_dialog_get_type ())

G_DECLARE_FINAL_TYPE(UpToDateDialog, up_to_date_dialog, UP_TO_DATE, DIALOG, AdwDialog)

AdwDialog* up_to_date_dialog_new();