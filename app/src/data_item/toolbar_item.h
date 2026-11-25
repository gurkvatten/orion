#pragma once

#include <gtk/gtk.h>

#include <core/dto/toolbar_item_dto.h>

#include "util/variant_util.h"

#define TOOLBAR_ITEM_TYPE (toolbar_item_get_type())

G_DECLARE_FINAL_TYPE(ToolbarItem, toolbar_item, APP, TOOLBAR_ITEM, GObject)

// Useful for drag and drop operations
enum class ToolbarItemSource : int64_t {
    not_set = 0,
    current_toolbar_box = 1,
    available_toolbar_box = 2,
    overflow_menu_listview = 3,
};

ToolbarItem* toolbar_item_new_from_dto(const orion::ToolbarItemDto& dto);

orion::ToolbarItemId toolbar_item_get_id(ToolbarItem* self);
orion::ToolbarItemType toolbar_item_get_item_type(ToolbarItem* self);
int64_t toolbar_item_get_position(ToolbarItem* self);
bool toolbar_item_get_in_overflow_menu(ToolbarItem* self);

struct ToolbarItemDetail {
    std::string translated_name;
    std::string action = "";
    bool can_be_in_overflow_menu = true;
    bool can_be_in_overflow_menu_multiple_times = false;
    bool can_be_in_toolbar_multiple_times = false;
    orion::gtk::VariantWrapper target = {};
    std::string icon_name = "";
    bool use_label_instead_of_icon = false;
};

ToolbarItemDetail toolbar_item_get_toolbar_item_details(ToolbarItem* item);
std::string toolbar_item_get_translated_name(ToolbarItem* self);
std::string toolbar_item_get_action(ToolbarItem* self);
bool toolbar_item_get_can_be_in_overflow_menu(ToolbarItem* self);
bool toolbar_item_get_can_be_in_overflow_menu_multiple_times(ToolbarItem* self);
bool toolbar_item_get_can_be_in_toolbar_multiple_times(ToolbarItem* self);
ToolbarItemSource toolbar_item_get_source(ToolbarItem* self);
void toolbar_item_set_source(ToolbarItem* self, ToolbarItemSource source);

orion::ProfileId toolbar_item_get_profile_id(ToolbarItem* self);