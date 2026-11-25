#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define ABOUT_DIALOG_TYPE (about_dialog_get_type ())

G_DECLARE_FINAL_TYPE(AboutDialog, about_dialog, ABOUT, DIALOG, AdwDialog)

AdwDialog* about_dialog_new_adw();