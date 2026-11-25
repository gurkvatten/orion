#pragma once

#include <gtk/gtk.h>

#define APP_TYPE_INT64_ITEM (int64_item_get_type())

G_DECLARE_FINAL_TYPE(Int64Item, int64_item, APP, INT64_ITEM, GObject)

Int64Item* int64_item_new(int64_t val);

int64_t int64_item_get_value(Int64Item* item);
