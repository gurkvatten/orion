#pragma once

#include "app.h"

namespace orion::gtk {
    
#pragma region Action map utils

void register_g_action(GtkWidget* win, const char* action_name, GCallback handler);
void register_g_action_stateful(GtkWidget* win, const char* action_name, GCallback handler, gboolean initial_state);
void register_g_action_stateful_radio(
    GtkWidget* win, const char* action_name, GCallback handler, GVariant* initial_state, const GVariantType* type
);
void register_g_action_stateless_parameterised(
    GtkWidget* win, const char* action_name, GCallback handler, const GVariantType* type
);
gboolean action_state_to_bool_transform(
    GBinding* binding, const GValue* from_value, GValue* to_value, gpointer user_data
);
gboolean string_transform_to(GBinding* binding, const GValue* from_value, GValue* to_value, gpointer user_data);
gboolean get_boolean_action_state(GActionMap* action_map, const char* action_name);
std::string get_string_action_state(GActionMap* action_map, const char* action_name);
int64_t get_int64_action_state(GActionMap* action_map, const char* action_name);
void toggle_action_state(GActionMap* action_map, const char* action_name);
void set_boolean_action_state(GActionMap* action_map, const char* action_name, gboolean state);
void set_string_action_state(GActionMap* action_map, const char* action_name, const char* state);
void set_int64_action_state(GActionMap* action_map, const char* action_name, int64_t state);

template <typename T> T get_enum_action_state(GActionMap* action_map, const char* action_name)
{
    GAction* action = g_action_map_lookup_action(action_map, action_name);

    if (!action) {
        log_error("get_enum_action_state: cannot find action '%s' in specified action map", action_name);
        return static_cast<T>(0);
    }

    GVariant* state = g_action_get_state(action);
    defer([state]{ g_variant_unref(state); });
    return static_cast<T>(g_variant_get_int64(state));
};

template <typename T> void set_enum_action_state(GActionMap* action_map, const char* action_name, T value)
{
    GAction* action = g_action_map_lookup_action(action_map, action_name);

    if (!action) {
        log_error("set_enum_action_state: cannot find action '%s' in specified action map", action_name);
        return;
    }

    g_action_change_state(action, g_variant_new_int64(static_cast<gint64>(value)));
};

//the callback is required to return a strong reference
template<StaticString Name>
struct _StaticStringActionClosure {
    static GAction* closure(GObject* _this, GActionMap* map)
    {
        if(map)
        {
            return g_object_ref(g_action_map_lookup_action(map, Name.data));
        }
        else 
        {
            return NULL;
        }
    };
};

template <StaticString Name>
static GtkExpression* action_expression_new(GtkExpression* action_map_expression)
{
    return gtk_cclosure_expression_new(G_TYPE_ACTION, NULL, 1, &action_map_expression, G_CALLBACK(_StaticStringActionClosure<Name>::closure), NULL, NULL);
}

#pragma endregion Action map utils



} // namespace orion::gtk