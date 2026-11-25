#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define EXTENSION_POPOVER_TYPE (extension_popover_get_type())

G_DECLARE_FINAL_TYPE(ExtensionPopover, extension_popover, EXTENSION, POPOVER, GtkPopover)

GtkPopover* extension_popover_new(orion::ExtensionId id);