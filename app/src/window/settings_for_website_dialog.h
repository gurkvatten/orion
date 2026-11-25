#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <string>
#include "app.h"

// TODO: rename this to SettingsForWebsiteDialog
#define SETTINGS_FOR_WEBSITE_POPOVER_TYPE (settings_for_website_popover_get_type())

G_DECLARE_FINAL_TYPE(SettingsForWebsitePopover, settings_for_website_popover, SETTINGS_FOR_WEBSITE, POPOVER, AdwDialog)

AdwDialog* settings_for_website_popover_new(const std::string& website);