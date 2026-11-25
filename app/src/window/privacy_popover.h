#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

typedef struct _MainWindow MainWindow;

#define PRIVACY_POPOVER_TYPE (privacy_popover_get_type ())

G_DECLARE_FINAL_TYPE(PrivacyPopover, privacy_popover, PRIVACY, POPOVER, GtkPopover)

GtkPopover* privacy_popover_new();
void privacy_popover_refresh(PrivacyPopover* self, MainWindow* main_window);
