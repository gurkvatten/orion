#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "core/webkit_bridge.h"
#include "popular_extension_item.h"

struct _PopularExtensionItem
{
	GObject parent;
    WebExtensions::ExtensionInfo extension;
    ExtensionInstallState state;
};

G_DEFINE_TYPE(PopularExtensionItem, popular_extension_item, G_TYPE_OBJECT)

void
popular_extension_item_finalize(GObject* object)
{
    PopularExtensionItem* item = POPULAR_EXTENSION_ITEM(object);
    std::destroy_at(&item->extension);
    G_OBJECT_CLASS(popular_extension_item_parent_class)->finalize(G_OBJECT(item));
}

void
popular_extension_item_init(PopularExtensionItem* item)
{
    std::construct_at(&item->extension);
}

static void
popular_extension_item_class_init(PopularExtensionItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = popular_extension_item_finalize;
}

ExtensionInstallState
popular_extension_item_get_install_state(PopularExtensionItem* item)
{
    return POPULAR_EXTENSION_ITEM(item)->state;
}

void
popular_extension_item_set_install_state(PopularExtensionItem* item, ExtensionInstallState new_state)
{
    item->state = new_state;
}

PopularExtensionItem*
popular_extension_item_new(const WebExtensions::ExtensionInfo& extension)
{
	PopularExtensionItem* item = static_cast<PopularExtensionItem*>(g_object_new(POPULAR_EXTENSION_ITEM_TYPE, NULL));
    item->extension = extension;
    popular_extension_item_set_install_state(item, ExtensionInstallState::None);
	return item;
}

const WebExtensions::ExtensionInfo&
popular_extension_item_get_extension(PopularExtensionItem* item)
{
    return item->extension;
}
