#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define APP_TYPE_READER_MODE_SETTINGS_POPOVER (reader_mode_settings_popover_get_type ())

G_DECLARE_FINAL_TYPE(ReaderModeSettingsPopover, reader_mode_settings_popover, APP, READER_MODE_SETTINGS_POPOVER, GtkPopover)

GtkPopover* reader_mode_settings_popover_new(orion::ProgrammableButtonId id);