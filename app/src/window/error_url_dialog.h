#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define ERROR_URL_DIALOG_TYPE (error_url_dialog_get_type ())

G_DECLARE_FINAL_TYPE(ErrorUrlDialog, error_url_dialog, ERROR_URL, DIALOG, AdwDialog)

AdwDialog* error_url_dialog_new();