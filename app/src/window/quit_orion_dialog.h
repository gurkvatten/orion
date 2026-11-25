#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define QUIT_ORION_DIALOG_TYPE (quit_orion_dialog_get_type())

G_DECLARE_FINAL_TYPE(QuitOrionDialog, quit_orion_dialog, QUIT_ORION, DIALOG, AdwDialog)

AdwDialog* quit_orion_dialog_new();