#pragma once

#include <gtk/gtk.h>

#include <core/dto/overflow_menu_item_dto.h>

#define OVERFLOW_MENU_ITEM_TYPE (overflow_menu_item_get_type())

G_DECLARE_FINAL_TYPE(OverflowMenuItem, overflow_menu_item, APP, OVERFLOW_MENU_ITEM, GObject)

OverflowMenuItem* overflow_menu_item_new_from_dto(const orion::OverflowMenuItemDto& dto);
orion::OverflowMenuItemId overflow_menu_item_get_id(OverflowMenuItem* self);
int64_t overflow_menu_item_get_idx(OverflowMenuItem* self);