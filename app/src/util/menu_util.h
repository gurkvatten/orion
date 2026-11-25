#pragma once

#include <gtk/gtk.h>
#include <string>
#include <set>
#include <common/src/defer.h>

namespace orion::gtk {

void menu_prepend_item(GMenu* menu, const std::string& label, const std::string& action);
void menu_append_item(GMenu* menu, const std::string& label, const std::string& action);

void menu_append_item_full(
    GMenu* menu, const std::string& label, const std::string& action, const std::string& target, const std::string& role
);

void menu_append_item_full_int64_target(
    GMenu* menu, const std::string& label, const std::string& action, int64_t target, const std::string& role
);

// These functions can be rewritten as templates if needed in the future.
void menu_append_action_with_int64_target(GMenu* menu, const char* label, const char* action, int64_t target);

void menu_append_action_with_string_target(GMenu* menu, const char* label, const char* action, const char* target);
void menu_append_action_with_bool_target(GMenu* menu, const char* label, const char* action, bool target);

void menu_append_action_with_int64_vector_target(
    GMenu* menu, const char* label, const char* action, const std::vector<int64_t>& target
);

void menu_append_action_with_target(GMenu* menu, const char* label, const char* action, GVariant* target);

std::vector<int64_t> get_int64_vector_from_fixed_array_variant(GVariant* variant);

// This function is useful when trying to build a sliding menu (GTK_POPOVER_MENU_SLIDING)
// because GtkStack fails to handle duplicate names in submenus (GTK_POPOVER_MENU_NESTED
// does not have this problem).
// Accumulates unique key names in menu_names with their individual frequencies.
// Use "bookmark" or "folder" as 'type' parameter (just for logging purposes).
std::string transform_to_unique_menu_name(std::string type, std::string name, std::multiset<std::string>* menu_names);

std::string append_invisible_unicode_suffix(const std::string& name, int repeat_count);

gboolean update_menu_item_action(
    GMenuModel* model, const char* label_to_find, const char* new_action, gboolean recursive
);

} // namespace orion::gtk