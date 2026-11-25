#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "data_item/g_string_item.h"

struct _GStringItem
{
	GObject parent;
    GString* string;
};

G_DEFINE_TYPE(GStringItem, g_string_item, G_TYPE_OBJECT)

void
g_string_item_finalize(GObject* object)
{
    GStringItem* item = G_STRING_ITEM(object);
    g_string_free(item->string, TRUE);
    G_OBJECT_CLASS(g_string_item_parent_class)->finalize(object);
}

void
g_string_item_init(GStringItem* item)
{

}

static void
g_string_item_class_init(GStringItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = g_string_item_finalize;
}

GString*
g_string_item_get_string(GStringItem* item)
{
    return G_STRING_ITEM(item)->string;
}

void
g_string_item_set_string(GStringItem* item, GString* text)
{
    G_STRING_ITEM(item)->string = text;
}
void
g_string_item_destroy(GStringItem* item)
{
    if(item!=NULL)
    {
        GStringItem* safe_item = G_STRING_ITEM(item);
        g_string_free(safe_item->string,true);
    }
}

GStringItem*
g_string_item_new(GString* string)
{
	GStringItem* item = static_cast<GStringItem*>(g_object_new(G_STRING_ITEM_TYPE, NULL));
    item->string = string;
	return item;
}