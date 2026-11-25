#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define IMPORT_FROM_EXTENSION_DIALOG_TYPE (import_from_extension_dialog_get_type())

G_DECLARE_FINAL_TYPE(ImportFromExtensionDialog, import_from_extension_dialog, IMPORT_FROM_EXTENSION, DIALOG, AdwDialog)

AdwDialog* import_from_extension_dialog_new();