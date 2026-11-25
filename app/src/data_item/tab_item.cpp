#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "tab_item.h"

struct _TabItem
{
	GObject parent;
    GString* title;
    GString* address;
    double seq_num;
    orion::TabId id;
};

G_DEFINE_TYPE(TabItem, tab_item, G_TYPE_OBJECT)

void
tab_item_finalize(GObject* object)
{
    TabItem* item = TAB_ITEM(object);
    g_string_free(item->title, TRUE);
    g_string_free(item->address, TRUE);
    G_OBJECT_CLASS(tab_item_parent_class)->finalize(G_OBJECT(item));
}

void
tab_item_init(TabItem* item)
{

}

static void
tab_item_class_init(TabItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = tab_item_finalize;
}

GString*
tab_item_get_title(TabItem* item)
{
    return TAB_ITEM(item)->title;
}

GString*
tab_item_get_address(TabItem* item)
{
    return TAB_ITEM(item)->address;
}


double
tab_item_get_seq_num(TabItem* item)
{
    return TAB_ITEM(item)->seq_num;
}


orion::TabId
tab_item_get_id(TabItem* item)
{
    return TAB_ITEM(item)->id;
}

void
tab_item_set_title(TabItem* item, GString* title)
{
    TAB_ITEM(item)->title = title;
}

void
tab_item_set_address(TabItem* item, GString* address)
{
    TAB_ITEM(item)->address = address;
}


void
tab_item_set_seq_num(TabItem* item, double seq_num)
{
    TAB_ITEM(item)->seq_num = seq_num;
}


void
tab_item_set_id(TabItem* item, orion::TabId id)
{
    TAB_ITEM(item)->id = id;
}

int
tab_item_seq_num_compare(gconstpointer item_a, gconstpointer item_b, gpointer user_data)
{
    double seq_num_a = tab_item_get_seq_num(static_cast<TabItem*>(const_cast<gpointer>(item_a)));
    double seq_num_b = tab_item_get_seq_num(static_cast<TabItem*>(const_cast<gpointer>(item_b)));
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

TabItem*
tab_item_new(orion::TabDto tab)
{
	TabItem* item = static_cast<TabItem*>(g_object_new(TAB_ITEM_TYPE, NULL));
    item->title = g_string_new(tab.name.c_str());
    item->address = g_string_new(tab.uri.c_str());
    item->seq_num = tab.seq_num;
    item->id = tab.id;
	return item;
}