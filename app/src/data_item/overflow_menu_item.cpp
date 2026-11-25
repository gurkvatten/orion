#include <common/src/defer.h>

#include "overflow_menu_item.h"
#include "toolbar_item.h"

struct _OverflowMenuItem
{
    GObject parent;

    orion::OverflowMenuItemId id;
    GString* name;
    GString* action;
    int64_t idx;
};

G_DEFINE_TYPE(OverflowMenuItem, overflow_menu_item, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_ID,
    PROP_NAME,
    PROP_IDX,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS];

OverflowMenuItem*
overflow_menu_item_new_from_dto(const orion::OverflowMenuItemDto& dto)
{
    OverflowMenuItem* item = static_cast<OverflowMenuItem*>(g_object_new(OVERFLOW_MENU_ITEM_TYPE, NULL));

    orion::ToolbarItemType item_type{};
    if (dto.action == "summarize_page") {
        item_type = orion::ToolbarItemType::summarize_page;
    } else if (dto.action == "programmable_button") {
        item_type = orion::ToolbarItemType::programmable_button;
    } else {
        orion::log_warning("Unknown overflow menu item action: %s", dto.action.c_str());
    }

    ToolbarItem* toolbar_item = toolbar_item_new_from_dto(orion::ToolbarItemDto{
        .item_type = item_type,
    });
    defer([toolbar_item]{ g_object_unref(toolbar_item); });

    item->id = dto.id;
    item->name = g_string_new(toolbar_item_get_translated_name(toolbar_item).c_str());
    item->action = g_string_new(dto.action.c_str());
    item->idx = dto.idx;
    return item;
}

orion::OverflowMenuItemId
overflow_menu_item_get_id(OverflowMenuItem* self)
{
    return APP_OVERFLOW_MENU_ITEM(self)->id;
}

int64_t
overflow_menu_item_get_idx(OverflowMenuItem* self)
{
    return APP_OVERFLOW_MENU_ITEM(self)->idx;
}

static void
overflow_menu_item_set_property(GObject* object, guint property_id, const GValue* val, GParamSpec* pspec)
{
    OverflowMenuItem* self = APP_OVERFLOW_MENU_ITEM(object);

    switch (property_id)
    {
        case PROP_ID:
            self->id = orion::OverflowMenuItemId(g_value_get_int64(val));
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_ID]);
            break;
        case PROP_NAME:
            if (self->name) g_string_free(self->name, TRUE);
            self->name = g_string_new(g_value_get_string(val));
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_NAME]);
            break;
        case PROP_IDX:
            self->idx = g_value_get_int64(val);
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_IDX]);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
overflow_menu_item_get_property(GObject* object, guint property_id, GValue* val, GParamSpec* pspec)
{
    OverflowMenuItem* self = APP_OVERFLOW_MENU_ITEM(object);

    switch (property_id)
    {
        case PROP_ID:
            g_value_set_int64(val, self->id.value);
            break;
        case PROP_NAME:
            g_value_set_string(val, self->name->str);
            break;
        case PROP_IDX:
            g_value_set_int64(val, self->idx);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
overflow_menu_item_finalize(GObject* object)
{
    OverflowMenuItem* item = APP_OVERFLOW_MENU_ITEM(object);
    if (item->name) g_string_free(item->name, TRUE);
    if (item->action) g_string_free(item->action, TRUE);

    std::destroy_at(&item->id);

    G_OBJECT_CLASS(overflow_menu_item_parent_class)->finalize(G_OBJECT(item));
}

static void
overflow_menu_item_init(OverflowMenuItem* item)
{
    std::construct_at(&item->id);
}

static void
overflow_menu_item_class_init(OverflowMenuItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = overflow_menu_item_finalize;
    G_OBJECT_CLASS(klass)->get_property = overflow_menu_item_get_property;
    G_OBJECT_CLASS(klass)->set_property = overflow_menu_item_set_property;

    obj_properties[PROP_ID] = g_param_spec_int64(
        "id", NULL, NULL, G_MININT64, G_MAXINT64, 0,
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
    );
    obj_properties[PROP_NAME] = g_param_spec_string(
        "name", NULL, NULL, "",
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
    );
    obj_properties[PROP_IDX] = g_param_spec_int64(
        "idx", NULL, NULL, G_MININT64, G_MAXINT64, 0,
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
    );

    g_object_class_install_properties(G_OBJECT_CLASS(klass), N_PROPS, obj_properties);
}