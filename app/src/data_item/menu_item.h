#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define APP_TYPE_MENU_ITEM (menu_item_get_type())

G_DECLARE_FINAL_TYPE(AppMenuItem, menu_item, APP, MENU_ITEM, GObject)

bool menu_item_matches_action_string(AppMenuItem* item, std::string& string);
gboolean menu_item_filter_func(AppMenuItem* item, std::vector<std::string>* terms);
std::string menu_item_get_detailed_action_name(AppMenuItem* item);

