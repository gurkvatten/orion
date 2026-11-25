#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define EXTENSIONS_KEYBOARD_SHORTCUTS_DIALOG_TYPE (extensions_keyboard_shortcuts_dialog_get_type())

G_DECLARE_FINAL_TYPE(ExtensionsKeyboardShortcutsDialog, extensions_keyboard_shortcuts_dialog, EXTENSIONS_KEYBOARD_SHORTCUTS, DIALOG, AdwPreferencesDialog)

AdwDialog* extensions_keyboard_shortcuts_dialog_new(orion::ExtensionId id);