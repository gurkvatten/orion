#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "data_item/wrapping_item.h"

namespace {
    struct CppWrapper {
    };
};

struct _WrappingItem
{
    GObject object;
    
    GObject* wrapped_item;
    GObject* wrapper_data;

    CppWrapper cpp_data;
};

G_DEFINE_TYPE(WrappingItem, wrapping_item, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_ITEM,
    PROP_WRAPPER_DATA,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS];

WrappingItem*
wrapping_item_new(GObject* wrapped_data, GObject* wrapper_data)
{
    return static_cast<WrappingItem*>(g_object_new(APP_TYPE_WRAPPING_ITEM, "item", wrapped_data, "wrapper_data", wrapper_data, NULL));
}

GObject*
wrapping_item_get_wrapper_data(WrappingItem* self)
{
    return self->wrapper_data;
}

GObject*
wrapping_item_get_wrapped_item(WrappingItem* self)
{
    return self->wrapped_item;
}

static void
wrapping_item_set_property(GObject* object, guint property_id, const GValue* val, GParamSpec* pspec)
{
    WrappingItem* self = APP_WRAPPING_ITEM(object);

    switch(property_id)
    {
        case PROP_ITEM:
            g_clear_object(&self->wrapped_item);
            self->wrapped_item = G_OBJECT(g_value_get_object(val));
            break;
        case PROP_WRAPPER_DATA:
            g_clear_object(&self->wrapper_data);
            self->wrapper_data = G_OBJECT(g_value_get_object(val));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
wrapping_item_get_property(GObject* object, guint property_id, GValue* val, GParamSpec* pspec)
{
    WrappingItem* self = APP_WRAPPING_ITEM(object);

    switch(property_id)
    {
        case PROP_ITEM:
            g_value_set_object(val, wrapping_item_get_wrapped_item(self));
            break;
        case PROP_WRAPPER_DATA:
            g_value_set_object(val, wrapping_item_get_wrapper_data(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

void
wrapping_item_finalize(GObject* object)
{
    WrappingItem* item = APP_WRAPPING_ITEM(object);
    g_clear_object(&item->wrapped_item);
    g_clear_object(&item->wrapper_data);
    std::destroy_at(&item->cpp_data);
    G_OBJECT_CLASS(wrapping_item_parent_class)->finalize(object);
}

void
wrapping_item_init(WrappingItem* item)
{
    std::construct_at(&item->cpp_data);
}

static void
wrapping_item_class_init(WrappingItemClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = wrapping_item_finalize;
    object_class->get_property = wrapping_item_get_property;
    object_class->set_property = wrapping_item_set_property;

    obj_properties[PROP_ITEM] = g_param_spec_object("item", NULL, NULL, G_TYPE_ICON, GParamFlags(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    obj_properties[PROP_WRAPPER_DATA] = g_param_spec_object("wrapper_data", NULL, NULL, G_TYPE_ICON, GParamFlags(G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));


    g_object_class_install_properties(object_class, N_PROPS, obj_properties);
}