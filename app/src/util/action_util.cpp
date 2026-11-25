#include "action_util.h"

#include <common/src/defer.h>

namespace orion::gtk {

#pragma region Action map utils

void
register_g_action(GtkWidget* win, const char* action_name, GCallback handler)
{
	GSimpleAction* action = g_simple_action_new(action_name, NULL);
	g_signal_connect(action, "activate", handler, win);
	g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(action));
    g_object_unref(action);
}

void
register_g_action_stateful(GtkWidget* win, const char* action_name, GCallback handler, gboolean initial_state)
{
	GSimpleAction* action = g_simple_action_new_stateful(
        action_name,
        NULL,
        g_variant_new_boolean(initial_state)
    );
    g_signal_connect(action, "activate", handler, win);
	g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(action));
    g_object_unref(action);
}

void
register_g_action_stateful_radio(
    GtkWidget* win, const char* action_name, GCallback handler, GVariant* initial_state, const GVariantType* type)
{
	GSimpleAction* action = g_simple_action_new_stateful(
        action_name,
        type,
        initial_state
    );
    g_signal_connect(action, "activate", handler, win);
	g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(action));
    g_object_unref(action);
}

void
register_g_action_stateless_parameterised(GtkWidget* win, const char* action_name, GCallback handler, const GVariantType* type)
{
	GSimpleAction* action = g_simple_action_new(action_name, type);
    g_signal_connect(action, "activate", handler, win);
	g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(action));
    g_object_unref(action);
}


#pragma endregion Action map utils

gboolean
action_state_to_bool_transform(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data)
{
    GVariant* state = g_value_get_variant(from_value);
    if(state)
    {
        g_value_set_boolean(to_value, g_variant_get_boolean(state));
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

gboolean
string_transform_to(GBinding* binding, const GValue* from_value, GValue* to_value, gpointer user_data)
{
    const char* src = g_value_get_string(from_value);
    g_value_set_string(to_value, src ? src : "");
    return TRUE;
}

gboolean
get_boolean_action_state(GActionMap* action_map, const char* action_name)
{
    GAction* action = g_action_map_lookup_action(action_map, action_name);

    if (!action) {
        log_error("get_boolean_action_state: cannot find action '%s' in specified action map", action_name);
        return FALSE;
    }

    GVariant* state = g_action_get_state(action);
    defer([state]{ g_variant_unref(state); });
    return g_variant_get_boolean(state);
}

std::string
get_string_action_state(GActionMap* action_map, const char* action_name)
{
    GAction* action = g_action_map_lookup_action(action_map, action_name);

    if (!action) {
        log_error("get_string_action_state: cannot find action '%s' in specified action map", action_name);
        return "";
    }

    GVariant* state = g_action_get_state(action);
    defer([state]{ g_variant_unref(state); });
    return std::string(g_variant_get_string(state, NULL));
}

int64_t
get_int64_action_state(GActionMap* action_map, const char* action_name)
{
    GAction* action = g_action_map_lookup_action(action_map, action_name);

    if (!action) {
        log_error("get_string_action_state: cannot find action '%s' in specified action map", action_name);
        return 0;
    }

    GVariant* state = g_action_get_state(action);
    defer([state]{ g_variant_unref(state); });
    return g_variant_get_int64(state);
}

void
toggle_action_state(GActionMap* action_map, const char* action_name)
{
    GAction* action = g_action_map_lookup_action(action_map, action_name);

    if (!action) {
        log_error("toggle_action_state: cannot find action '%s' in specified action map", action_name);
        return;
    }

    GVariant* state = g_action_get_state(action);
    defer([state]{ g_variant_unref(state); });
    gboolean current_value = g_variant_get_boolean(state);
    g_action_change_state(action, g_variant_new_boolean(!current_value));
}

void
set_boolean_action_state(GActionMap* action_map, const char* action_name, gboolean state)
{
    GAction* action = g_action_map_lookup_action(action_map, action_name);

    if (!action) {
        log_error("set_boolean_action_state: cannot find action '%s' in specified action map", action_name);
        return;
    }

    g_action_change_state(action, g_variant_new_boolean(state));
}

void
set_string_action_state(GActionMap* action_map, const char* action_name, const char* state)
{
    GAction* action = g_action_map_lookup_action(action_map, action_name);

    if (!action) {
        log_error("set_string_action_state: cannot find action '%s' in specified action map", action_name);
        return;
    }

    g_action_change_state(action, g_variant_new_string(state));
}

void
set_int64_action_state(GActionMap* action_map, const char* action_name, int64_t state)
{
    GAction* action = g_action_map_lookup_action(action_map, action_name);

    if (!action) {
        log_error("set_string_action_state: cannot find action '%s' in specified action map", action_name);
        return;
    }

    g_action_change_state(action, g_variant_new_int64(state));
}

} // namespace orion::gtk