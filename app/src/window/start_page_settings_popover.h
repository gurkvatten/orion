#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define START_PAGE_SETTINGS_POPOVER_TYPE (start_page_settings_popover_get_type ())

G_DECLARE_FINAL_TYPE(StartPageSettingsPopover, start_page_settings_popover, START_PAGE_SETTINGS, POPOVER, GtkPopover)

GtkPopover* start_page_settings_popover_new();
void start_page_settings_popover_set_name(StartPageSettingsPopover* popover, const char* name);
void start_page_settings_popover_select_name(StartPageSettingsPopover* popover);