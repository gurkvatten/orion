#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define APP_TYPE_WRAPPING_ITEM (wrapping_item_get_type())

G_DECLARE_FINAL_TYPE(WrappingItem, wrapping_item, APP, WRAPPING_ITEM, GObject)

WrappingItem* wrapping_item_new(GObject* wrapped_data, GObject* wrapper_data);

GObject* wrapping_item_get_wrapper_data(WrappingItem* self);
GObject* wrapping_item_get_wrapped_item(WrappingItem* self);
