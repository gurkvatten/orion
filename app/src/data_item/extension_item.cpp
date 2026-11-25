#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "extension_item.h"

struct _ExtensionItem
{
	GObject parent;
    orion::ExtensionId* id;
    orion::ExtensionSku* sku;
    orion::ProfileId profile_id;
    GString* title;
    GString* description;
    GString* version;
    GString* icon_ref;
    bool is_enabled;
    bool do_not_update;
    bool has_options;
    GString* activate_shortcut;
    GString* lock_shortcut;
    orion::ExtensionType type;
    orion::UnixTimeUtc64 last_updated_at;
};

G_DEFINE_TYPE(ExtensionItem, extension_item, G_TYPE_OBJECT)

void
extension_item_finalize(GObject* object)
{
    ExtensionItem* item = EXTENSION_ITEM(object);
    g_string_free(item->title, TRUE);
    if(item->description) g_string_free(item->description, TRUE);
    g_string_free(item->activate_shortcut, TRUE);
    g_string_free(item->lock_shortcut, TRUE);
    g_string_free(item->version, TRUE);
    delete item->sku;
    item->sku = nullptr;
    delete item->id;
    item->id = nullptr;
    G_OBJECT_CLASS(extension_item_parent_class)->finalize(G_OBJECT(item));
}

void
extension_item_init(ExtensionItem* item)
{
    item->id = new orion::ExtensionId;
    item->sku = new orion::ExtensionSku;
}

static void
extension_item_class_init(ExtensionItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = extension_item_finalize;
}

GString*
extension_item_get_title(ExtensionItem* item)
{
    return EXTENSION_ITEM(item)->title;
}

GString*
extension_item_get_description(ExtensionItem* item)
{
    return EXTENSION_ITEM(item)->description;
}

GString*
extension_item_get_version(ExtensionItem* item)
{
    return EXTENSION_ITEM(item)->version;
}

orion::ExtensionType
extension_item_get_extension_type(ExtensionItem* item)
{
    return EXTENSION_ITEM(item)->type;
}

GString*
extension_item_get_icon_ref(ExtensionItem* item)
{
    return EXTENSION_ITEM(item)->icon_ref;
}

orion::ExtensionId*
extension_item_get_id(ExtensionItem* item)
{
    return EXTENSION_ITEM(item)->id;
}

bool
extension_item_get_is_enabled(ExtensionItem* item)
{
    return EXTENSION_ITEM(item)->is_enabled;
}

orion::UnixTimeUtc64
extension_item_get_last_updated_at(ExtensionItem* item)
{
    return EXTENSION_ITEM(item)->last_updated_at;
}

bool
extension_item_get_has_options(ExtensionItem* item)
{
    return EXTENSION_ITEM(item)->has_options;
}

bool
extension_item_get_do_not_update(ExtensionItem* item)
{
    return EXTENSION_ITEM(item)->do_not_update;
}

int
extension_item_compare(gconstpointer a, gconstpointer b, gpointer user_data)
{
    ExtensionItem* item_a = EXTENSION_ITEM(const_cast<gpointer>(a));
    ExtensionItem* item_b = EXTENSION_ITEM(const_cast<gpointer>(b));
    return strcmp(item_a->title->str, item_b->title->str);
}

ExtensionItem*
extension_item_new_no_desc(orion::ExtensionDto extension)
{
	ExtensionItem* item = static_cast<ExtensionItem*>(g_object_new(EXTENSION_ITEM_TYPE, NULL));
    *item->id = extension.id.value;
    *item->sku = extension.sku.value;
    item->profile_id = extension.profile_id.value;
    item->title = g_string_new(extension.name.c_str());
    item->version = g_string_new(extension.version.c_str());
    item->icon_ref = g_string_new(extension.icon_ref.c_str());
    item->activate_shortcut = g_string_new(extension.activate_shortcut.c_str());
    item->lock_shortcut = g_string_new(extension.lock_shortcut.c_str());
    item->is_enabled = extension.is_enabled;
    item->type = extension.type;
    item->last_updated_at = extension.last_updated_at;
    item->do_not_update = extension.do_not_update;
    item->has_options = extension.has_options;
	return item;
}

ExtensionItem*
extension_item_new_with_desc(orion::ExtensionDto extension)
{
    ExtensionItem* item = extension_item_new_no_desc(extension);
    item->description = g_string_new(extension.description.c_str());
    return item;
}