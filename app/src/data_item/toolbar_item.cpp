#include <map>

#include <glib/gi18n.h>

#include "toolbar_item.h"

struct _ToolbarItem
{
    GObject parent;

    orion::ToolbarItemId id;
    orion::ToolbarItemType item_type;
    int64_t position;
    bool in_overflow_menu;
    GString* translated_name; // convenience field for UI, not stored in DB
    GString* icon_name; // convenience field for UI, not stored in DB
    bool use_label_instead_of_icon; // convenience field for UI, not stored in DB
    orion::ProfileId profile_id;
    ToolbarItemSource source; // useful for drag and drop operations
};

G_DEFINE_TYPE(ToolbarItem, toolbar_item, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_ID,
    PROP_ITEM_TYPE,
    PROP_POSITION,
    PROP_IN_OVERFLOW_MENU,
    PROP_TRANSLATED_NAME,
    PROP_ICON_NAME,
    PROP_USE_IMAGE_ICON,
    PROP_USE_IMAGE_LABEL,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS];


ToolbarItemDetail
toolbar_item_get_toolbar_item_details(ToolbarItem* item)
{
    switch(item->item_type)
    {
        case orion::ToolbarItemType::not_set:
            return {};
        case orion::ToolbarItemType::summarize_page: 
            return {
                .translated_name = _("Summarize Page"),
                .action = "win.summarize-page",
                .icon_name = "format-text-direction-ltr-symbolic",
            };
        case orion::ToolbarItemType::programmable_button:
            return {
                .translated_name = _("Programmable Button"),
                .action = "win.edit-programmable-button",
                .can_be_in_overflow_menu_multiple_times = true,
                .can_be_in_toolbar_multiple_times = true,
                .target = item->id.value,
                .icon_name = "❖",
                .use_label_instead_of_icon = true,
            };
        case orion::ToolbarItemType::privacy: 
            return {
                .translated_name = _("Privacy"),
                .action = "win.show-privacy",
                .icon_name = "shield-half-symbolic",
            };
        case orion::ToolbarItemType::page_tweaks:
            return {
                .translated_name = _("Page Tweaks"),
                .action = "win.show-page-tweaks",
                .icon_name = "brush-symbolic",
            };
        case orion::ToolbarItemType::website_settings:
            return {
                .translated_name = _("Website Settings"),
                .action = "win.settings-for-current-page",
                .icon_name = "cogged-wheel-symbolic",
            };
        case orion::ToolbarItemType::flexible_space:
            return {
                .translated_name = _("Flexible Space"),
                .can_be_in_overflow_menu = false,
                .can_be_in_toolbar_multiple_times = true,
                .icon_name = "size-horizontally-symbolic",
            };
        case orion::ToolbarItemType::profile:
            return {
                .translated_name = _("Profile"),
                .action = "win.profile",
                .can_be_in_overflow_menu = false,
            };
        case orion::ToolbarItemType::address_and_search:
            return  {
                .translated_name = _("Address and Search"),
                .can_be_in_overflow_menu = false,
            };
        case orion::ToolbarItemType::back_forward:
            return {
                .translated_name = _("Back/Forward"),
                .can_be_in_overflow_menu = false,
            };
        case orion::ToolbarItemType::home:
            return {
                .translated_name = _("Home"),
                .action = "win.home",
                .icon_name = "go-home-symbolic",
            };
        case orion::ToolbarItemType::start_page:
            return {
                .translated_name = _("Start Page"),
                .action = "win.open-start-page",
                .icon_name = "compass-symbolic",
            };
        case orion::ToolbarItemType::notes:
            return {
                .translated_name = _("Notes"),
                .action = "win.notes", // TODO: implement this action
                .icon_name = "notepad-symbolic",
            };
        case orion::ToolbarItemType::downloads:
            return {
                .translated_name = _("Downloads"),
                .action = "win.show-downloads",
                .icon_name = "arrow-pointing-at-line-down",
            };
        case orion::ToolbarItemType::share:
            return {
                .translated_name = _("Share"),
                .action = "win.share", // TODO: implement this action
                .icon_name = "share-symbolic",
            };
        case orion::ToolbarItemType::new_tab:
            return {
                .translated_name = _("New Tab"),
                .action = "win.new-tab",
                .icon_name = "plus-large-symbolic",
            };
        case orion::ToolbarItemType::history:
            return {
                .translated_name = _("History"),
                .action = "win.show-history",
                .icon_name = "clock-alt-symbolic",
            };
        case orion::ToolbarItemType::sidebar:
            return {
                .translated_name = _("Sidebar"),
                .action = "win.sidebar",
                .can_be_in_overflow_menu = false,
            };
        case orion::ToolbarItemType::zoom:
            return {
                .translated_name = _("Zoom"),
                .can_be_in_overflow_menu = false,
            };
        case orion::ToolbarItemType::print:
            return {
                .translated_name = _("Print"),
                .action = "win.print",
                .icon_name = "printer2-symbolic",
            };
        case orion::ToolbarItemType::web_inspector:
            return {
                .translated_name = _("Web Inspector"),
                .action = "win.toggle-web-inspector",
                .icon_name = "info-outline-symbolic",
            };
        case orion::ToolbarItemType::overflow_menu:
            return {
                .translated_name = _("Overflow Menu"),
                .can_be_in_overflow_menu = false,
                .icon_name = "more-small-symbolic",
            };
        case orion::ToolbarItemType::extensions:
            return{
                .translated_name = _("Extensions"),
                .action = "win.manage-extensions",
                .icon_name = "puzzle-piece-symbolic",
            };
        case orion::ToolbarItemType::toggle_reader:
            return {
                .translated_name = _("Show Reader"),
                .action = "win.toggle-reader",
                .icon_name = "open-book-symbolic",
            };
        case orion::ToolbarItemType::tab_overview:
            return {
                .translated_name = _("Tab Overview"),
                .action = "win.show-tab-overview",
                .icon_name = "grid-symbolic",
            };
        case orion::ToolbarItemType::focus_mode:
            return {
                .translated_name = _("Focus Mode"),
                .action = "win.toggle-focus-mode",
                .icon_name = "cd-symbolic",
            };
        case orion::ToolbarItemType::bookmark:
            return {
                .translated_name = _("Bookmark"),
                .action = "win.add-bookmark",
                .icon_name = "star-outline-thin-symbolic",
            };
        case orion::ToolbarItemType::kagi_privacy_pass:
            return {
                .translated_name = _("Kagi Privacy Pass"),
                .action = "win.show-kagi-privacy-pass",
                .icon_name = "orion-logo",
            };
        case orion::ToolbarItemType::new_window:
            return {
                .translated_name = _("New Window"),
                .action = "win.new-window",
                .icon_name = "window-symbolic",
            };
        case orion::ToolbarItemType::new_private_window:
            return {
                .translated_name = _("New Private Window"),
                .action = "win.new-private-window",
                .icon_name = "hand-openyay-symbolic",
            };
        case orion::ToolbarItemType::tab_groups:
            return {
                .translated_name = _("Tab Groups"),
                .can_be_in_overflow_menu = false,
            };
    };
    orion::log_warning("toolbar_item_get_toolbar_item_details(): item type unaccounted for");
    return {};
}

ToolbarItem*
toolbar_item_new_from_dto(const orion::ToolbarItemDto& dto)
{
    ToolbarItem* item = static_cast<ToolbarItem*>(g_object_new(TOOLBAR_ITEM_TYPE, NULL));

    item->id = dto.id;
    item->item_type = dto.item_type;
    item->position = dto.position;
    item->in_overflow_menu = dto.in_overflow_menu;
    item->translated_name = g_string_new(toolbar_item_get_translated_name(item).c_str());
    item->icon_name = g_string_new(toolbar_item_get_toolbar_item_details(item).icon_name.c_str());
    item->use_label_instead_of_icon = toolbar_item_get_toolbar_item_details(item).use_label_instead_of_icon;
    item->profile_id = dto.profile_id;
    return item;
}

orion::ToolbarItemId
toolbar_item_get_id(ToolbarItem* self)
{
    return APP_TOOLBAR_ITEM(self)->id;
}

orion::ToolbarItemType
toolbar_item_get_item_type(ToolbarItem* self)
{
    return APP_TOOLBAR_ITEM(self)->item_type;
}

int64_t
toolbar_item_get_position(ToolbarItem* self)
{
    return APP_TOOLBAR_ITEM(self)->position;
}

bool
toolbar_item_get_in_overflow_menu(ToolbarItem* self)
{
    return APP_TOOLBAR_ITEM(self)->in_overflow_menu;
}

std::string
toolbar_item_get_translated_name(ToolbarItem* self)
{
    return toolbar_item_get_toolbar_item_details(self).translated_name;
}

std::string
toolbar_item_get_action(ToolbarItem* self)
{
    return toolbar_item_get_toolbar_item_details(self).action;
}

bool
toolbar_item_get_can_be_in_overflow_menu(ToolbarItem* self)
{
    return toolbar_item_get_toolbar_item_details(self).can_be_in_overflow_menu;
}

bool
toolbar_item_get_can_be_in_overflow_menu_multiple_times(ToolbarItem* self)
{
    return toolbar_item_get_toolbar_item_details(self).can_be_in_overflow_menu_multiple_times;
}

bool
toolbar_item_get_can_be_in_toolbar_multiple_times(ToolbarItem* self)
{
    return toolbar_item_get_toolbar_item_details(self).can_be_in_toolbar_multiple_times;
}

ToolbarItemSource
toolbar_item_get_source(ToolbarItem* self)
{
    return APP_TOOLBAR_ITEM(self)->source;
}

void
toolbar_item_set_source(ToolbarItem* self, ToolbarItemSource source)
{
    APP_TOOLBAR_ITEM(self)->source = source;
}

orion::ProfileId
toolbar_item_get_profile_id(ToolbarItem* self)
{
    return APP_TOOLBAR_ITEM(self)->profile_id;
}

static void
toolbar_item_set_property(GObject* object, guint property_id, const GValue* val, GParamSpec* pspec)
{
    ToolbarItem* self = APP_TOOLBAR_ITEM(object);

    switch (property_id)
    {
        case PROP_ID:
            self->id = orion::ToolbarItemId(g_value_get_int64(val));
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_ID]);
            break;
        case PROP_ITEM_TYPE:
            self->item_type = orion::ToolbarItemType(g_value_get_int64(val));
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_ITEM_TYPE]);
            break;
        case PROP_POSITION:
            self->position = g_value_get_int64(val);
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_POSITION]);
            break;
        case PROP_IN_OVERFLOW_MENU:
            self->in_overflow_menu = g_value_get_boolean(val);
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_IN_OVERFLOW_MENU]);
            break;
        case PROP_TRANSLATED_NAME:
            if (self->translated_name) g_string_free(self->translated_name, TRUE);
            self->translated_name = g_string_new(g_value_get_string(val));
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_TRANSLATED_NAME]);
            break;
        case PROP_ICON_NAME:
            if (self->icon_name) g_string_free(self->icon_name, TRUE);
            self->icon_name = g_string_new(g_value_get_string(val));
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_ICON_NAME]);
            break;
        case PROP_USE_IMAGE_ICON:
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_USE_IMAGE_ICON]);
            self->use_label_instead_of_icon = !g_value_get_boolean(val);
            break;
        case PROP_USE_IMAGE_LABEL:
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_USE_IMAGE_LABEL]);
            self->use_label_instead_of_icon = !!g_value_get_boolean(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
toolbar_item_get_property(GObject* object, guint property_id, GValue* val, GParamSpec* pspec)
{
    ToolbarItem* self = APP_TOOLBAR_ITEM(object);

    switch (property_id)
    {
        case PROP_ID:
            g_value_set_int64(val, self->id.value);
            break;
        case PROP_ITEM_TYPE:
            g_value_set_int64(val, static_cast<int64_t>(self->item_type));
            break;
        case PROP_POSITION:
            g_value_set_int64(val, self->position);
            break;
        case PROP_IN_OVERFLOW_MENU:
            g_value_set_boolean(val, self->in_overflow_menu);
            break;
        case PROP_TRANSLATED_NAME:
            g_value_set_string(val, self->translated_name->str);
            break;
        case PROP_ICON_NAME:
            g_value_set_string(val, self->icon_name->str);
            break;
        case PROP_USE_IMAGE_ICON:
            g_value_set_boolean(val, !self->use_label_instead_of_icon);
            break;
        case PROP_USE_IMAGE_LABEL:
            g_value_set_boolean(val, self->use_label_instead_of_icon);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
toolbar_item_finalize(GObject* object)
{
    ToolbarItem* item = APP_TOOLBAR_ITEM(object);
    if (item->translated_name) g_string_free(item->translated_name, TRUE);
    if (item->icon_name) g_string_free(item->icon_name, TRUE);

    std::destroy_at(&item->id);
    std::destroy_at(&item->item_type); // future proofing
    std::destroy_at(&item->profile_id);

    G_OBJECT_CLASS(toolbar_item_parent_class)->finalize(G_OBJECT(item));
}

static void
toolbar_item_init(ToolbarItem* item)
{
    std::construct_at(&item->id);
    std::construct_at(&item->item_type); // future proofing
    std::construct_at(&item->profile_id);
}

static void
toolbar_item_class_init(ToolbarItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = toolbar_item_finalize;
    G_OBJECT_CLASS(klass)->get_property = toolbar_item_get_property;
    G_OBJECT_CLASS(klass)->set_property = toolbar_item_set_property;

    obj_properties[PROP_ID] = g_param_spec_int64(
        "id", NULL, NULL, G_MININT64, G_MAXINT64, 0,
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE)
    );
    obj_properties[PROP_ITEM_TYPE] = g_param_spec_int64(
        "item-type", NULL, NULL, G_MININT64, G_MAXINT64, 0,
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE)
    );
    obj_properties[PROP_POSITION] = g_param_spec_int64(
        "position", NULL, NULL, G_MININT64, G_MAXINT64, 0,
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE)
    );
    obj_properties[PROP_IN_OVERFLOW_MENU] = g_param_spec_boolean(
        "in-overflow-menu", NULL, NULL, FALSE,
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE)
    );
    obj_properties[PROP_TRANSLATED_NAME] = g_param_spec_string(
        "translated-name", NULL, NULL, "",
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE)
    );
    obj_properties[PROP_ICON_NAME] = g_param_spec_string(
        "icon-name", NULL, NULL, "",
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE)
    );
    obj_properties[PROP_USE_IMAGE_ICON] = g_param_spec_boolean(
        "use-image-icon", NULL, NULL, FALSE,
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE)
    );
    obj_properties[PROP_USE_IMAGE_LABEL] = g_param_spec_boolean(
        "use-image-label", NULL, NULL, FALSE,
        GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE)
    );

    g_object_class_install_properties(G_OBJECT_CLASS(klass), N_PROPS, obj_properties);
}