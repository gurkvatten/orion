#include "toolbar_util.h"

namespace orion::gtk {

GVariant*
programmable_button_parse_args(const ProgrammableButtonDto& dto)
{
    using ActionType = ProgrammableButtonActionType;
    switch(dto.action_type)
    {
        case ActionType::not_set:
            return NULL;
        case ActionType::menu_action:
            {
                //documentation states that g_action_parse_detailed_name is NULL safe, alas, it is not
                gchar* action_c_string = NULL;
                GVariant* var = NULL;
                GError* err = NULL;
                if(g_action_parse_detailed_name(dto.action_string.c_str(), &action_c_string, &var, &err))
                {
                    g_free(action_c_string);
                    return var;
                }
                else 
                {
                    g_error_free(err);
                    return NULL;
                }
            }
        case ActionType::open_uri_in_current_tab:
            return g_variant_new_string(dto.action_string.c_str());
        case ActionType::open_uri_in_new_tab:
            return g_variant_new_string(dto.action_string.c_str());
        case ActionType::open_uri_in_link_preview:
            return g_variant_new("(sdd)", dto.action_string.c_str(), -1, -1);
        case ActionType::open_uri_in_background:
            return g_variant_new_string(dto.action_string.c_str());
        case ActionType::run_javascript_code:
        case ActionType::run_javascript_code_open_sidebar:
            return g_variant_new("(ssb)", dto.title.c_str(), dto.action_string.c_str(),
                gboolean(dto.action_type == ActionType::run_javascript_code_open_sidebar));
    }
    return NULL;
}

std::string
programmable_button_parse_action_name(const ProgrammableButtonDto& dto)
{
    switch(dto.action_type)
    {
        case ProgrammableButtonActionType::not_set:
            return "nothing";
        case ProgrammableButtonActionType::menu_action:
            if(dto.action_string.empty())
            {
                return "";
            }
            else
            {
                //documentation states that g_action_parse_detailed_name is NULL safe, alas, it is not
                gchar* action_c_string = NULL;
                GVariant* var = NULL;
                GError* err = NULL;
                if(g_action_parse_detailed_name(dto.action_string.c_str(), &action_c_string, &var, &err))
                {
                    if(var) g_variant_unref(var);
                    std::string action_string = action_c_string;
                    g_free(action_c_string);
                    return "action(" + action_string + ")";
                }
                else
                {
                    g_error_free(err);
                    return "";
                }
            }
        case ProgrammableButtonActionType::open_uri_in_current_tab:
            return "action(win.open-uri-with-placeholders-in-current-tab)";
        case ProgrammableButtonActionType::open_uri_in_new_tab:
            return "action(win.open-uri-with-placeholders-in-new-tab)";
        case ProgrammableButtonActionType::open_uri_in_link_preview:
            return "action(win.open-uri-with-placeholders-in-preview-window)";
        case ProgrammableButtonActionType::open_uri_in_background:
            return "action(win.open-uri-with-placeholders-in-background)";
        case ProgrammableButtonActionType::run_javascript_code:
        case ProgrammableButtonActionType::run_javascript_code_open_sidebar:
            return "action(win.run-javascript-code)";
    }
    return "empty";
}

std::string
programmable_button_parse_action_name_raw(const ProgrammableButtonDto& dto)
{
    std::string name = programmable_button_parse_action_name(dto);
    if (name.rfind("action(", 0) == 0 && name.back() == ')') {
        return name.substr(7, name.size() - 8); // remove "action(" prefix and ")" suffix
    }
    return name; // return the original name if it doesn't match the expected format
}

GtkShortcutAction*
programmable_button_parse_action(const ProgrammableButtonDto& dto)
{
    return gtk_shortcut_action_parse_string(programmable_button_parse_action_name(dto).c_str());
}

GtkShortcutTrigger*
programmable_button_parse_trigger(const ProgrammableButtonDto& dto)
{
    GtkShortcutTrigger* trigger = NULL;
    if(dto.keyboard_shortcut.length())
    {
        trigger = gtk_shortcut_trigger_parse_string(dto.keyboard_shortcut.c_str());
    }
    return trigger;
}

GtkShortcut*
programmable_button_parse_shortcut(const ProgrammableButtonDto& dto)
{
    GtkShortcutAction* action = programmable_button_parse_action(dto);
    GtkShortcutTrigger* trigger = programmable_button_parse_trigger(dto);
    GVariant* args = programmable_button_parse_args(dto);

    GtkShortcut* shortcut = gtk_shortcut_new(trigger, action);
    //this will sink the floating ref of args 
    gtk_shortcut_set_arguments(shortcut, args);

    return shortcut;
}

} // namespace orion::gtk