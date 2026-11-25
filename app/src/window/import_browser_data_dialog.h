#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define IMPORT_BROWSER_DATA_DIALOG_TYPE (import_browser_data_dialog_get_type())

G_DECLARE_FINAL_TYPE(ImportBrowserDataDialog, import_browser_data_dialog, IMPORT_BROWSER_DATA, DIALOG, AdwDialog)

AdwDialog* import_browser_data_dialog_new();