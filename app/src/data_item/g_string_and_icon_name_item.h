#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

enum class GStringAndIconNameType { folder, entry };

#define G_STRING_AND_ICON_NAME_ITEM_TYPE (g_string_and_icon_name_item_get_type())

G_DECLARE_FINAL_TYPE(GStringAndIconNameItem, g_string_and_icon_name_item, G_STRING_AND_ICON_NAME, ITEM, GObject)

GStringAndIconNameItem* g_string_and_icon_name_item_new(GString* string, const char* icon_name, GStringAndIconNameType type);
void g_string_and_icon_name_item_destroy(GStringAndIconNameItem* item);
GString* g_string_and_icon_name_item_get_string(GStringAndIconNameItem* item);
const char* g_string_and_icon_name_item_get_icon_name(GStringAndIconNameItem* item);
GStringAndIconNameType g_string_and_icon_name_item_get_item_type(GStringAndIconNameItem* item);
void g_string_and_icon_name_item_set_string(GStringAndIconNameItem* item, GString* text);
void g_string_and_icon_name_item_set_icon_name(GStringAndIconNameItem* item, const char* icon_name);
void g_string_and_icon_name_item_set_item_type(GStringAndIconNameItem* item, GStringAndIconNameType type);
