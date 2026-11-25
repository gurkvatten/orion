#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define POPULAR_EXTENSION_ITEM_TYPE (popular_extension_item_get_type())

enum class ExtensionInstallState {
    None,
    Installable,
    Fetching,
    Installed,
};

G_DECLARE_FINAL_TYPE(PopularExtensionItem, popular_extension_item, POPULAR_EXTENSION, ITEM, GObject)

PopularExtensionItem* popular_extension_item_new(const WebExtensions::ExtensionInfo& extension);
const WebExtensions::ExtensionInfo& popular_extension_item_get_extension(PopularExtensionItem* item);
ExtensionInstallState popular_extension_item_get_install_state(PopularExtensionItem* item);
void popular_extension_item_set_install_state(PopularExtensionItem* item, ExtensionInstallState new_state);
