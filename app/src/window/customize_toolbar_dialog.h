#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define CUSTOMIZE_TOOLBAR_DIALOG_TYPE (customize_toolbar_dialog_get_type())

G_DECLARE_FINAL_TYPE(CustomizeToolbarDialog, customize_toolbar_dialog, CUSTOMIZE_TOOLBAR, DIALOG, AdwDialog)

AdwDialog* customize_toolbar_dialog_new(bool is_compact_tab_layout);