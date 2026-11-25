#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include "window/main_window/main_window.h"

#define SETTINGS_DIALOG_TYPE (settings_dialog_get_type ())

G_DECLARE_FINAL_TYPE(SettingsDialog, settings_dialog, SETTINGS, DIALOG, AdwDialog)

void settings_dialog_present_from_last_selected(MainWindow* parent_window);
void settings_dialog_present_with_page_selected(MainWindow* parent_window, int page);
// Centralized helper: pass page < 0 to use last selected page
void settings_dialog_present(MainWindow* parent_window, int page);
void settings_dialog_select_page(SettingsDialog* self, int page_number);