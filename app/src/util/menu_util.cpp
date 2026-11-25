#include "menu_util.h"
#include <common/src/log.h>
#include <common/src/string_util.h>
#include "common/src/ptr_util.h"

namespace orion::gtk {

void
menu_prepend_item(GMenu* menu, const std::string& label, const std::string& action)
{
    GMenuItem* menu_item = g_menu_item_new(label.c_str(), action.c_str());
    defer([menu_item]{ g_object_unref(menu_item); });
    g_menu_prepend_item(menu, menu_item);
}

void
menu_append_item(GMenu* menu, const std::string& label, const std::string& action)
{
    GMenuItem* menu_item = g_menu_item_new(label.c_str(), action.empty() ? NULL : action.c_str());
    defer([menu_item]{ g_object_unref(menu_item); });
    g_menu_append_item(menu, menu_item);
}

void
menu_append_item_full(
    GMenu* menu, const std::string& label, const std::string& action, const std::string& target,
    const std::string& role)
{
    GMenuItem* menu_item = g_menu_item_new(label.c_str(), NULL);
    defer([menu_item]{ g_object_unref(menu_item); });
    if (!target.empty()) {
        g_menu_item_set_action_and_target(menu_item, action.c_str(), "s", target.c_str());
    }
    if (!role.empty()) {
        g_menu_item_set_attribute(menu_item, "role", "s", role.c_str());
    }
    g_menu_append_item(menu, menu_item);
}

void
menu_append_item_full_int64_target(
    GMenu* menu, const std::string& label, const std::string& action, int64_t target,
    const std::string& role)
{
    GMenuItem* menu_item = g_menu_item_new(label.c_str(), NULL);
    defer([menu_item]{ g_object_unref(menu_item); });
    
    g_menu_item_set_action_and_target(menu_item, action.c_str(), "x", target);
    if (!role.empty()) {
        g_menu_item_set_attribute(menu_item, "role", "s", role.c_str());
    }
    g_menu_append_item(menu, menu_item);
}

void
menu_append_action_with_int64_target(GMenu* menu, const char* label, const char* action, int64_t target)
{
    GMenuItem* menu_item = g_menu_item_new(label, NULL);
    defer([menu_item]{ g_object_unref(menu_item); });
    g_menu_item_set_action_and_target(menu_item, action, "x", target);
    g_menu_append_item(menu, menu_item);
}

void
menu_append_action_with_string_target(GMenu* menu, const char* label, const char* action, const char* target)
{
    GMenuItem* menu_item = g_menu_item_new(label, NULL);
    defer([menu_item]{ g_object_unref(menu_item); });
    g_menu_item_set_action_and_target(menu_item, action, "s", target);
    g_menu_append_item(menu, menu_item);
}

void
menu_append_action_with_bool_target(GMenu* menu, const char* label, const char* action, bool target)
{
    GMenuItem* menu_item = g_menu_item_new(label, NULL);
    defer([menu_item]{ g_object_unref(menu_item); });
    g_menu_item_set_action_and_target(menu_item, action, "b", target);
    g_menu_append_item(menu, menu_item);
}

void
menu_append_action_with_pointer_target(GMenu* menu, const char* label, const char* action, gpointer target)
{
    GMenuItem* menu_item = g_menu_item_new(label, NULL);
    defer([menu_item]{ g_object_unref(menu_item); });
    g_menu_item_set_action_and_target(menu_item, action, "s", target);
    g_menu_append_item(menu, menu_item);
}

void
menu_append_action_with_int64_vector_target(GMenu* menu, const char* label, const char* action, const std::vector<int64_t>& target)
{
    GVariant* variant = g_variant_new_fixed_array(G_VARIANT_TYPE_INT64, target.data(), target.size(), sizeof(int64_t));
    GMenuItem* item = g_menu_item_new(label, NULL);
    defer([item]{ g_object_unref(item); });
    g_menu_item_set_action_and_target_value(item, action, variant);
    g_menu_append_item(menu, item);
}

void
menu_append_action_with_target(GMenu* menu, const char* label, const char* action, GVariant* target)
{
    GMenuItem* item = g_menu_item_new(label, NULL);
    defer([item]{ g_object_unref(item); });
    g_menu_item_set_action_and_target_value(item, action, target);
    g_menu_append_item(menu, item);
}

std::vector<int64_t>
get_int64_vector_from_fixed_array_variant(GVariant* variant)
{
    gsize n_elements = 0;
    const int64_t* data = static_cast<const int64_t*>(
        g_variant_get_fixed_array(variant, &n_elements, sizeof(int64_t))
    );
    return std::vector<int64_t>(data, data + n_elements);
}

// We need to use some non-character code point to avoid collisions with real names.
// Cannot use U+FDD0 because it is rendered in GTK menus as garbage.
// Use U+200B (ZERO WIDTH SPACE (ZWSP)) instead, this is a word-breaking hint and it
// should be invisible in menus.
const char8_t* non_char = u8"\u200B";

std::string
transform_to_unique_menu_name(std::string type, std::string name, std::multiset<std::string>* menu_names)
{
    std::string key = name;
    std::string result = name;
    if (menu_names->count(key) > 0) {
        // the cast might be potentially dangerous
        std::string non_char_str = std::string(reinterpret_cast<const char*>(non_char));
        std::string suffix = orion::str::repeat(non_char_str, menu_names->count(key));
        result = name + suffix;
    }
    orion::log_verbose("transform_to_unique_menu_name: insert item: type=%s key='%s' name='%s'",
        type.c_str(), key.c_str(), result.c_str()
    );
    menu_names->insert(key);
    return result;
}

std::string
append_invisible_unicode_suffix(const std::string& name, int repeat_count)
{
    if (repeat_count <= 0) return name;
    // the cast might be potentially dangerous
    std::string non_char_str = std::string(reinterpret_cast<const char*>(non_char));
    std::string suffix = orion::str::repeat(non_char_str, repeat_count);
    return name + suffix;
}

gboolean
update_menu_item_action(
    GMenuModel* model, const char* label_to_find, const char* new_action, gboolean recursive)
{
    int n = g_menu_model_get_n_items(model);

    for (int i = 0; i < n; i++) {
        // Check label
        GVariant* label_v = g_menu_model_get_item_attribute_value(
            model, i, "label", G_VARIANT_TYPE_STRING
        );

        if (label_v) {
            defer([label_v]{ g_variant_unref(label_v); });
            const char* label = g_variant_get_string(label_v, NULL);

            if (g_strcmp0(label, label_to_find) == 0) {
                if (G_IS_MENU(model)) {
                    // Replace item
                    g_menu_remove(G_MENU(model), i);
                    GMenuItem* new_item = g_menu_item_new (label, new_action);
                    defer([new_item]{ g_object_unref(new_item); });
                    g_menu_insert_item (G_MENU (model), i, new_item);
                    return TRUE; // stop after first replacement
                }
            }
        }

        if (recursive) {
            // Recurse into section
            GMenuModel* link = g_menu_model_get_item_link(model, i, G_MENU_LINK_SECTION);
            if (link) {
                defer([link]{ g_object_unref(link); });
                gboolean found = update_menu_item_action(link, label_to_find, new_action, TRUE);
                if (found) {
                    return TRUE;
                }
            }

            // Recurse into submenu
            link = g_menu_model_get_item_link(model, i, G_MENU_LINK_SUBMENU);
            if (link) {
                defer([link]{ g_object_unref(link); });
                gboolean found = update_menu_item_action(link, label_to_find, new_action, TRUE);
                if (found) {
                    return TRUE;
                }
            }
        }
    }

    return FALSE; // not found here
}

} // namespace orion::gtk