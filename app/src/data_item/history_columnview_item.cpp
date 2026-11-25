#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "data_item/history_columnview_item.h"
#include "util/text_util.h"

int global_history_columnview_item_count = 0;

struct _HistoryColumnviewItem
{
	GObject parent;
    orion::HistoryEntryId id;
    GDateTime* date;
    GString* website;
    GString* address;
    HistoryColumnviewItemType type;
    guint items;
    GtkLabel* label;
    // just a holder for "weak reference", the item does not remove the observers on finalize being called anymore
    std::optional<orion::ObserverId> observer_id;
    GListStore* list_store;
};

G_DEFINE_TYPE(HistoryColumnviewItem, history_columnview_item, G_TYPE_OBJECT)

void
history_columnview_item_dispose(GObject* object)
{
    HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(object);
    g_clear_object(&(item->label));
    if(item->type == HistoryColumnviewItemType::folder) 
    {
        g_clear_object(&item->list_store);
    }
    G_OBJECT_CLASS(history_columnview_item_parent_class)->dispose(G_OBJECT(item));
}

void
history_columnview_item_finalize(GObject* object)
{
    global_history_columnview_item_count--;
    HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(object);
    
    g_date_time_unref(item->date);
    if(item->website) g_string_free(item->website, TRUE);
    if(item->address) g_string_free(item->address, TRUE);
    
    std::destroy_at(&item->observer_id);
    G_OBJECT_CLASS(history_columnview_item_parent_class)->finalize(G_OBJECT(item));
}

void
history_columnview_item_init(HistoryColumnviewItem* item)
{
    std::construct_at(&item->observer_id);
    global_history_columnview_item_count++;
}

static void
history_columnview_item_class_init(HistoryColumnviewItemClass* klass)
{
    G_OBJECT_CLASS(klass)->dispose = history_columnview_item_dispose;
    G_OBJECT_CLASS(klass)->finalize = history_columnview_item_finalize;
}

GDateTime*
history_columnview_item_get_date(HistoryColumnviewItem* item)
{
    return HISTORY_COLUMNVIEW_ITEM(item)->date;
}

GString*
history_columnview_item_get_website(HistoryColumnviewItem* item)
{
    return HISTORY_COLUMNVIEW_ITEM(item)->website;
}

GString*
history_columnview_item_get_address(HistoryColumnviewItem* item)
{
    return HISTORY_COLUMNVIEW_ITEM(item)->address;
}

HistoryColumnviewItemType
history_columnview_item_get_item_type(HistoryColumnviewItem* item)
{
    return HISTORY_COLUMNVIEW_ITEM(item)->type;
}

uint
history_columnview_item_get_items(HistoryColumnviewItem* item)
{
    return HISTORY_COLUMNVIEW_ITEM(item)->items;
}

GListStore*
history_columnview_item_get_list_store(HistoryColumnviewItem* item)
{
    return HISTORY_COLUMNVIEW_ITEM(item)->list_store;
}

orion::HistoryEntryId
history_columnview_item_get_id(HistoryColumnviewItem* item)
{
    return HISTORY_COLUMNVIEW_ITEM(item)->id;
}

std::optional<orion::ObserverId>
history_columnview_item_get_observer_id(HistoryColumnviewItem* item)
{
    return HISTORY_COLUMNVIEW_ITEM(item)->observer_id;
}

void
history_columnview_item_set_website(HistoryColumnviewItem* item, GString* website)
{
    HISTORY_COLUMNVIEW_ITEM(item)->website = website;
}

void
history_columnview_item_set_address(HistoryColumnviewItem* item, GString* address)
{
    HISTORY_COLUMNVIEW_ITEM(item)->address = address;
}

void
history_columnview_item_set_item_type(HistoryColumnviewItem* item, HistoryColumnviewItemType type)
{
    HISTORY_COLUMNVIEW_ITEM(item)->type = type;
}

void
history_columnview_item_set_items(HistoryColumnviewItem* item, uint items)
{
    HISTORY_COLUMNVIEW_ITEM(item)->items = items;
    if(item->label)
    {
        GString* string = g_string_new_take(orion::text::n_items_get_text(items));
        gtk_label_set_text(GTK_LABEL(item->label), string->str);
        g_string_free(item->address, TRUE);
        item->address = string;
    }
}

void
history_columnview_item_increment_items(HistoryColumnviewItem* item)
{
    history_columnview_item_set_items(item, (item->items)+1);
}

void
history_columnview_item_decrement_items(HistoryColumnviewItem* item)
{
    history_columnview_item_set_items(item, (item->items)-1>0?(item->items)-1:0);
}

void
history_columnview_item_set_observer_id(HistoryColumnviewItem* item, std::optional<orion::ObserverId> id)
{
    HISTORY_COLUMNVIEW_ITEM(item)->observer_id = id;
}

void
history_columnview_item_set_items_label(HistoryColumnviewItem* item, GtkLabel* items_label)
{
    HISTORY_COLUMNVIEW_ITEM(item)->label = items_label;
    if(items_label)
    {
        GString* string = g_string_new("");
        g_string_printf(string, ngettext("%u item", "%u items", item->items), item->items);
        gtk_label_set_text(GTK_LABEL(item->label), string->str);
        g_string_free(string, true);
    }
}

HistoryColumnviewItem*
history_columnview_item_new_from_day(const orion::HistoryEntryDay& day)
{
	HistoryColumnviewItem* item = static_cast<HistoryColumnviewItem*>(g_object_new(HISTORY_COLUMNVIEW_ITEM_TYPE, NULL));
    item->website = NULL;
    GString* address = g_string_new_take(orion::text::n_items_get_text(day.visit_count));
    item->address = address;
    item->type = HistoryColumnviewItemType::folder;
    item->items = day.visit_count;
    GDateTime* date = g_date_time_new_from_unix_utc(day.start_of_day.value);
    item->date = g_date_time_to_local(date);
    g_date_time_unref(date);
    item->list_store = g_list_store_new(G_TYPE_OBJECT);
	return item;
}

HistoryColumnviewItem*
history_columnview_item_new_from_history_struct(const orion::HistoryEntryDto& history_struct, GListStore* folder_list_store)
{
	HistoryColumnviewItem* item = static_cast<HistoryColumnviewItem*>(g_object_new(HISTORY_COLUMNVIEW_ITEM_TYPE, NULL));

    item->items = 0;
    item->website = g_string_new(history_struct.website.c_str());
    item->address = g_string_new(history_struct.address.c_str());
    item->type = HistoryColumnviewItemType::address;
    item->id = history_struct.id;

    item->list_store = folder_list_store;

    const gint64 time = history_struct.visited_at.value;
    GDateTime* date = g_date_time_new_from_unix_utc(time);
    item->date = g_date_time_to_local(date);
    g_date_time_unref(date);

    return item;
}

HistoryColumnviewItem*
history_columnview_item_new(GDateTime* date, GString* website, GString* address, HistoryColumnviewItemType type)
{
	HistoryColumnviewItem* item = static_cast<HistoryColumnviewItem*>(g_object_new(HISTORY_COLUMNVIEW_ITEM_TYPE, NULL));
    item->date = date;
    item->website = website;
    item->address = address;
    item->type = type;
    item->items = 0;
	return item;
}