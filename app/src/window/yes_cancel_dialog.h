#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define YES_CANCEL_DIALOG_TYPE (yes_cancel_dialog_get_type ())

G_DECLARE_FINAL_TYPE(YesCancelDialog, yes_cancel_dialog, YES_CANCEL, DIALOG, AdwDialog)

AdwDialog* yes_cancel_dialog_new();