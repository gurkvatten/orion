#include <gtk/gtk.h>

#include "data_item/menu_item.h"

#include <common/src/defer.h>

struct _AppMenuItem
{
	GObject parent;

    gchar* label;
    gchar** path;
    gchar* action;
    gchar* target;
};

G_DEFINE_TYPE(AppMenuItem, menu_item, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_LABEL,
    PROP_PATH,
    PROP_ACTION,
    PROP_TARGET,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS] = {NULL, };

bool
menu_item_matches_action_string(AppMenuItem* item, std::string& string)
{
    gchar* action_name = NULL;
    GVariant* target = NULL;
    GError* err = NULL;
    
    if(!g_action_parse_detailed_name(string.c_str(), &action_name, &target, &err))
    {
        defer([err]{ g_error_free(err); });
        return false;
    }

    defer([action_name]{ g_free(action_name); });
    defer([target]{ if(target) g_variant_unref(target); });

    if(0 != g_strcmp0(action_name, item->action))
    {
        return false;
    }

    if(target)
    {
        if(!g_variant_is_of_type(target, G_VARIANT_TYPE_STRING))
        {
            return false;
        }
        else
        {
            const char* target_string = g_variant_get_string(target, NULL);

            return 0 == g_strcmp0(target_string, item->target);
        }
    }
    else
    {
        return item->target == NULL;
    }
}

gboolean
menu_item_filter_func(AppMenuItem* item, std::vector<std::string>* terms) {
    //orion::log_debug("filter run");
    if(!(terms->size()))
    {
        return TRUE;
    }

    if(!(item->path || item->label)) return TRUE;
    //check if every word exists as a substring in the icon_name
    for(const auto& term : *terms)
    {
        if(item->label)
        {
            if(strcasestr(item->label, term.c_str())) continue;
        }
        if(item->path)
        {
            char** ptr = item->path;
            for(; char* path_part = *ptr; ptr++)
            {
                if(strcasestr(path_part, term.c_str())) break;
            }
            if(*ptr) continue;
        }

        return FALSE;
    }
    return TRUE;
}

std::string
menu_item_get_detailed_action_name(AppMenuItem* item)
{
    GVariant* target = item->target?g_variant_new_string(item->target):NULL;

    gchar* name = g_action_print_detailed_name(item->action, target);

    g_clear_pointer(&target, g_variant_unref);

    if(name)
    {
        std::string res = name;
        g_free(name);
        return res;
    }
    else
    {
        return "";
    }
}

void
menu_item_finalize(GObject* object)
{
    AppMenuItem* item = APP_MENU_ITEM(object);
    g_clear_pointer(&item->label, g_free);
    g_clear_pointer(&item->path, g_strfreev);
    g_clear_pointer(&item->action, g_free);
    g_clear_pointer(&item->target, g_free);
    G_OBJECT_CLASS(menu_item_parent_class)->finalize(object);
}

void
menu_item_init(AppMenuItem* item)
{

}

static void
menu_item_set_property(GObject* object, guint prop_id, const GValue* val, GParamSpec* pspec)
{
    AppMenuItem* item = APP_MENU_ITEM(object);
    
    switch(prop_id)
    {
        case PROP_LABEL:
            g_clear_pointer(&item->label, g_free);
            item->label = g_strdup(g_value_get_string(val));
            break;
        case PROP_PATH:
            g_clear_pointer(&item->path, g_strfreev);
            item->path = g_strdupv((char**)g_value_get_boxed(val));
            break;
        case PROP_ACTION:
            g_clear_pointer(&item->action, g_free);
            item->action = g_strdup(g_value_get_string(val));
            break;
        case PROP_TARGET:
            g_clear_pointer(&item->target, g_free);
            item->target = g_strdup(g_value_get_string(val));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
menu_item_get_property(GObject* object, guint prop_id, GValue* val, GParamSpec* pspec)
{
    AppMenuItem* item = APP_MENU_ITEM(object);
    
    switch(prop_id)
    {
        case PROP_LABEL:
            g_value_set_string(val, item->label);
            break;
        case PROP_PATH:
            g_value_set_boxed(val, item->path);
            break;
        case PROP_ACTION:
            g_value_set_string(val, item->action);
            break;
        case PROP_TARGET:
            g_value_set_string(val, item->target);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
menu_item_class_init(AppMenuItemClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = menu_item_finalize;
    object_class->set_property = menu_item_set_property;
    object_class->get_property = menu_item_get_property;

    obj_properties[PROP_LABEL] = g_param_spec_string("label", NULL, NULL, NULL,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_PATH] = g_param_spec_boxed("path", NULL, NULL, G_TYPE_STRV,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_ACTION] = g_param_spec_string("action", NULL, NULL, NULL,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_TARGET] = g_param_spec_string("target", NULL, NULL, NULL,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_properties(object_class, N_PROPS, obj_properties);
}
