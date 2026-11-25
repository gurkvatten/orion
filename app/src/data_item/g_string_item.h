#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define G_STRING_ITEM_TYPE (g_string_item_get_type())

G_DECLARE_FINAL_TYPE(GStringItem, g_string_item, G_STRING, ITEM, GObject)

GStringItem* g_string_item_new(GString* string);
void g_string_item_destroy(GStringItem* item);
GString* g_string_item_get_string(GStringItem* item);
void g_string_item_set_string(GStringItem* item, GString* text);
