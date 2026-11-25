#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "data_item/int64_item.h"

struct _Int64Item
{
	GObject parent;
    int64_t val;
};

G_DEFINE_TYPE(Int64Item, int64_item, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_VALUE,
    N_PROPS,
};

GParamSpec* obj_properties[N_PROPS] = {NULL, };

Int64Item*
int64_item_new(int64_t val)
{
	Int64Item* item = static_cast<Int64Item*>(g_object_new(APP_TYPE_INT64_ITEM, "value", val, NULL));
    return item;
}

void
int64_item_init(Int64Item* item)
{
}

int64_t int64_item_get_value(Int64Item* item)
{
    return item->val;
}

static void
int64_item_get_property(GObject* object, guint prop_id, GValue* val, GParamSpec* pspec)
{
    Int64Item* item = APP_INT64_ITEM(object);

    switch(prop_id)
    {
        case PROP_VALUE:
            g_value_set_int64(val, int64_item_get_value(item));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
int64_item_set_property(GObject* object, guint prop_id, const GValue* val, GParamSpec* pspec)
{
    Int64Item* item = APP_INT64_ITEM(object);

    switch(prop_id)
    {
        case PROP_VALUE:
            item->val = g_value_get_int64(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
int64_item_class_init(Int64ItemClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->get_property = int64_item_get_property;
    object_class->set_property = int64_item_set_property;

    obj_properties[PROP_VALUE] = g_param_spec_int64("value", NULL, NULL, INT64_MIN, INT64_MAX, 0, GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
    g_object_class_install_properties(object_class, N_PROPS, obj_properties);
}

