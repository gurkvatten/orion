#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define APP_TYPE_IMPORT_FROM_URL_DIALOG (import_from_url_dialog_get_type())

// Signals
// finished(ImportFromUrlDialog* self, const gchar* url, gpointer user_data) -- emited on import or cancel clicked, url is NULL on cancel

G_DECLARE_FINAL_TYPE(ImportFromUrlDialog, import_from_url_dialog, APP, IMPORT_FROM_URL_DIALOG, AdwDialog)

AdwDialog* import_from_url_dialog_new();