#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define IMPORT_FROM_BROWSER_DIALOG_TYPE (import_from_browser_dialog_get_type())

G_DECLARE_FINAL_TYPE(ImportFromBrowserDialog, import_from_browser_dialog, IMPORT_FROM_BROWSER, DIALOG, AdwDialog)

AdwDialog* import_from_browser_dialog_new();