#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define EXTENSION_ITEM_TYPE (extension_item_get_type())

G_DECLARE_FINAL_TYPE(ExtensionItem, extension_item, EXTENSION, ITEM, GObject)

ExtensionItem* extension_item_new_no_desc(orion::ExtensionDto extension);
ExtensionItem* extension_item_new_with_desc(orion::ExtensionDto extension);

GString* extension_item_get_title(ExtensionItem* item);
GString* extension_item_get_description(ExtensionItem* item);
GString* extension_item_get_version(ExtensionItem* item);
GString* extension_item_get_icon_ref(ExtensionItem* item);
orion::ExtensionId* extension_item_get_id(ExtensionItem* item);
bool extension_item_get_is_enabled(ExtensionItem* item);
bool extension_item_get_do_not_update(ExtensionItem* item);
bool extension_item_get_has_options(ExtensionItem* item);
orion::UnixTimeUtc64 extension_item_get_last_updated_at(ExtensionItem* item);
orion::ExtensionType extension_item_get_extension_type(ExtensionItem* item);

int extension_item_compare(gconstpointer a, gconstpointer b, gpointer user_data);