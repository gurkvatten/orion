#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "window_item.h"
#include "util/favicon_util.h"
#include "util/text_util.h"

struct _WindowItem
{
	GObject parent;
    GString* title;
    GString* icon_name;
    double seq_num;
    orion::WindowId id;
    bool is_suspended;
};

enum {
    PROP_0,
    PROP_TITLE,
    PROP_ICON_NAME,
    PROP_ICON_LABEL,
    PROP_SEQ_NUM,
    PROP_ID,
    PROP_IS_SUSPENDED,
    PROP_IS_IMAGE_ICON, // needed in sidebar at the moment, will be removed later
    PROP_IS_EMOJI_ICON, // needed in sidebar at the moment, will be removed later
    N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(WindowItem, window_item, G_TYPE_OBJECT)

void
window_item_finalize(GObject* object)
{
    WindowItem* item = WINDOW_ITEM(object);
    g_string_free(item->title, TRUE);
    g_string_free(item->icon_name, TRUE);
    G_OBJECT_CLASS(window_item_parent_class)->finalize(G_OBJECT(item));
}

static void
window_item_set_property(GObject      *object,
                         guint         property_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    WindowItem *self = WINDOW_ITEM(object);

    switch (property_id) {
    case PROP_TITLE:
        window_item_set_title(self, g_value_get_string(value));
        break;
    case PROP_SEQ_NUM:
        window_item_set_seq_num(self, g_value_get_double(value));
        break;
    case PROP_ID:
        window_item_set_id(self, orion::WindowId(g_value_get_int64(value)));
        break;
    case PROP_IS_SUSPENDED:
        window_item_set_is_suspended(self, g_value_get_boolean(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}


static void
window_item_get_property(GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    WindowItem *self = WINDOW_ITEM(object);

    switch (property_id) {
    case PROP_TITLE:
        g_value_set_string(value, window_item_get_title(self));
        break;
    case PROP_ICON_NAME:
        g_value_set_string(value, window_item_get_icon_name(self));
        break;
    case PROP_ICON_LABEL:
        g_value_set_string(value, window_item_get_icon_label(self));
        break;
    case PROP_SEQ_NUM:
        g_value_set_double(value, window_item_get_seq_num(self));
        break;
    case PROP_ID:
        g_value_set_int64(value, int64_t(window_item_get_id(self)));
        break;
    case PROP_IS_SUSPENDED:
        g_value_set_boolean(value, self->is_suspended);
        break;
    case PROP_IS_IMAGE_ICON:
        g_value_set_boolean(value, window_item_get_is_image_icon(self));
        break;
    case PROP_IS_EMOJI_ICON:
        g_value_set_boolean(value, !window_item_get_is_image_icon(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}


void
window_item_init(WindowItem* item)
{

}

static void
window_item_class_init(WindowItemClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = window_item_finalize;
    object_class->set_property = window_item_set_property;
    object_class->get_property = window_item_get_property;
    
    obj_properties[PROP_TITLE] =
        g_param_spec_string("title",
                         "Title",
                         "The window's title",
                         "(No Title)",
                         GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_ICON_NAME] =
        g_param_spec_string("icon-name",
                         "Icon Name",
                         "The window's icon name (ignores emojis)",
                         "",
                         GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_ICON_LABEL] =
        g_param_spec_string("icon-label",
                         "Icon Name",
                         "The window's icon label (only includes emojis)",
                         "",
                         GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_SEQ_NUM] =
        g_param_spec_double("seq-num",
                         "Seq num",
                         "The window's sequential number",
                         -DBL_MAX, DBL_MAX, 0.0,
                         GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_ID] =
        g_param_spec_int64("id",
                         "Id",
                         "The window's id",
                         INT64_MIN, INT64_MAX, 0,
                         GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_IS_SUSPENDED] =
        g_param_spec_boolean("is-suspended",
                         "Is Suspended",
                         "Whether the window is suspended",
                         false,
                         GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_IS_IMAGE_ICON] =
        g_param_spec_boolean("is-image-icon",
                         "Is Image Icon",
                         "Whether the icon is an image icon (not emoji)",
                         false,
                         GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_IS_EMOJI_ICON] =
        g_param_spec_boolean("is-emoji-icon",
                         "Is Emoji Icon",
                         "Whether the icon is an emoji icon",
                         false,
                         GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

const char*
window_item_get_title(WindowItem* item)
{
    return WINDOW_ITEM(item)->title->str;
}

const char*
window_item_get_icon_name(WindowItem* item)
{
    return (item)->icon_name->str;
}

// can be refactored out, no need to differentiate between icon name and icon label anymore
const char*
window_item_get_icon_label(WindowItem* item)
{
    return (item)->icon_name->str;
}

double
window_item_get_seq_num(WindowItem* item)
{
    return WINDOW_ITEM(item)->seq_num;
}


orion::WindowId
window_item_get_id(WindowItem* item)
{
    return WINDOW_ITEM(item)->id;
}

bool
window_item_get_is_suspended(WindowItem* item)
{
    return WINDOW_ITEM(item)->is_suspended;
}

bool
window_item_get_is_image_icon(WindowItem* item)
{
    return !orion::text::text_is_emoji(WINDOW_ITEM(item)->icon_name->str, pango_cairo_font_map_get_default());
}

void
window_item_set_title(WindowItem* item, const char* title)
{
    if(WINDOW_ITEM(item)->title) g_string_free(item->title, true);
    WINDOW_ITEM(item)->title = g_string_new(title);
    g_object_notify(G_OBJECT(item), "title");
}

void
window_item_set_icon_name(WindowItem* item, const char* icon_name)
{
    if(WINDOW_ITEM(item)->icon_name) g_string_free(item->icon_name, true);
    WINDOW_ITEM(item)->icon_name = g_string_new(icon_name);
    g_object_notify(G_OBJECT(item), "icon-name");
}


void
window_item_set_seq_num(WindowItem* item, double seq_num)
{
    WINDOW_ITEM(item)->seq_num = seq_num;
    g_object_notify(G_OBJECT(item), "seq-num");
}


void
window_item_set_id(WindowItem* item, orion::WindowId id)
{
    WINDOW_ITEM(item)->id = id;
    g_object_notify(G_OBJECT(item), "id");
}

void
window_item_set_is_suspended(WindowItem* item, bool is_suspended)
{
    WINDOW_ITEM(item)->is_suspended = is_suspended;
    g_object_notify(G_OBJECT(item), "is-suspended");
}

int
window_item_seq_num_compare(gconstpointer item_a, gconstpointer item_b, gpointer user_data)
{
    double seq_num_a = window_item_get_seq_num(static_cast<WindowItem*>(const_cast<gpointer>(item_a)));
    double seq_num_b = window_item_get_seq_num(static_cast<WindowItem*>(const_cast<gpointer>(item_b)));
    if(seq_num_a>seq_num_b)
    {
        return 1;
    }
    else if (seq_num_a<seq_num_b)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}


WindowItem*
window_item_new(orion::WindowDto window)
{
	WindowItem* item = static_cast<WindowItem*>(g_object_new(WINDOW_ITEM_TYPE, NULL));
    if (window.is_tab_group && !window.is_private) {
        std::string label = orion::text::ngettext_string("%" PRId64 " Tab", "%" PRId64 " Tabs", window.num_tabs);
        item->title = g_string_new(label.c_str());
    } else {
        // Saved Tab Group or Private Tab Group
        item->title = g_string_new(window.name.c_str());
    }
    item->icon_name = g_string_new(window.icon_name.c_str());
    item->seq_num = window.seq_num;
    item->id = window.id;
    item->is_suspended = window.is_suspended;
	return item;
}