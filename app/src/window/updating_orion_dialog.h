#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define UPDATING_ORION_DIALOG_TYPE (updating_orion_dialog_get_type ())

G_DECLARE_FINAL_TYPE(UpdatingOrionDialog, updating_orion_dialog, UPDATING_ORION, DIALOG, AdwDialog)

AdwDialog* updating_orion_dialog_new();