#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define GET_ORION_PLUS_DIALOG_TYPE (get_orion_plus_dialog_get_type ())

G_DECLARE_FINAL_TYPE(GetOrionPlusDialog, get_orion_plus_dialog, GET_ORION_PLUS, DIALOG, AdwDialog)

AdwDialog* get_orion_plus_dialog_new();