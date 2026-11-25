#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define POPULAR_EXTENSIONS_DIALOG_TYPE (popular_extensions_dialog_get_type ())

G_DECLARE_FINAL_TYPE(PopularExtensionsDialog, popular_extensions_dialog, POPULAR_EXTENSIONS, DIALOG, AdwDialog)

AdwDialog* popular_extensions_dialog_new();