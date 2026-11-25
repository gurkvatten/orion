#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define DEFAULT_FONT_DIALOG_TYPE (default_font_dialog_get_type())

G_DECLARE_FINAL_TYPE(DefaultFontDialog, import_from_extension_dialog, DEFAULT_FONT, DIALOG, AdwDialog)

AdwDialog* default_font_dialog_new();