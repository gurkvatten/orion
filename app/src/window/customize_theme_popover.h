#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define CUSTOMIZE_THEME_POPOVER_TYPE (customize_theme_popover_get_type ())

// TODO: rename this to CustomizeThemeDialog
G_DECLARE_FINAL_TYPE(CustomizeThemePopover, customize_theme_popover, CUSTOMIZE_THEME, POPOVER, AdwDialog)

AdwDialog* customize_theme_popover_new();