#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "data_item/g_string_and_icon_name_item.h"

struct _GStringAndIconNameItem
{
	GObject parent;
    GString* string;
    const char* icon_name;
    GStringAndIconNameType type;
};

G_DEFINE_TYPE(GStringAndIconNameItem, g_string_and_icon_name_item, G_TYPE_OBJECT)

void
g_string_and_icon_name_item_finalize(GObject* object)
{
    GStringAndIconNameItem* item = G_STRING_AND_ICON_NAME_ITEM(object);
    g_string_free(item->string, TRUE);
    G_OBJECT_CLASS(g_string_and_icon_name_item_parent_class)->finalize(object);
}

void
g_string_and_icon_name_item_init(GStringAndIconNameItem* item)
{

}

static void
g_string_and_icon_name_item_class_init(GStringAndIconNameItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = g_string_and_icon_name_item_finalize;
}

GString*
g_string_and_icon_name_item_get_string(GStringAndIconNameItem* item)
{
    return G_STRING_AND_ICON_NAME_ITEM(item)->string;
}

const char*
g_string_and_icon_name_item_get_icon_name(GStringAndIconNameItem* item)
{
    return G_STRING_AND_ICON_NAME_ITEM(item)->icon_name;
}

GStringAndIconNameType
g_string_and_icon_name_item_get_item_type(GStringAndIconNameItem* item)
{
    return G_STRING_AND_ICON_NAME_ITEM(item)->type;
}

void
g_string_and_icon_name_item_set_string(GStringAndIconNameItem* item, GString* text)
{
    G_STRING_AND_ICON_NAME_ITEM(item)->string = text;
}

void
g_string_and_icon_name_item_set_icon_name(GStringAndIconNameItem* item, const char* icon_name)
{
    G_STRING_AND_ICON_NAME_ITEM(item)->icon_name = icon_name;
}

void
g_string_and_icon_name_item_set_item_type(GStringAndIconNameItem* item, GStringAndIconNameType type)
{
    G_STRING_AND_ICON_NAME_ITEM(item)->type = type;
}

void
g_string_and_icon_name_item_destroy(GStringAndIconNameItem* item)
{
    if(item!=NULL)
    {
        GStringAndIconNameItem* safe_item = G_STRING_AND_ICON_NAME_ITEM(item);
        g_string_free(safe_item->string,true);
    }
}

GStringAndIconNameItem*
g_string_and_icon_name_item_new(GString* string, const char* icon_name, GStringAndIconNameType type)
{
	GStringAndIconNameItem* item = static_cast<GStringAndIconNameItem*>(g_object_new(G_STRING_AND_ICON_NAME_ITEM_TYPE, NULL));
    item->string = string;
    item->icon_name = icon_name;
    item->type = type;
	return item;
}