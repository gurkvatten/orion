#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "data_item/reading_list_columnview_item.h"

struct _ReadingListColumnviewItem
{
	GObject parent;
	GDateTime* date;
    GString* title;
    GString* address;
    GString* referrer;
    GString* desc;
    gboolean is_unred;
    ReadingListColumnviewItemType type;
    orion::ReadingListEntryId id;
    GListStore* store; //used for parent list store for address and folder list store for folder
};

G_DEFINE_TYPE(ReadingListColumnviewItem, reading_list_columnview_item, G_TYPE_OBJECT)

void
reading_list_columnview_item_finalize(GObject* object)
{
    ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(object);
    g_date_time_unref(item->date);
    if (item->title) g_string_free(item->title, TRUE);
    if (item->address) g_string_free(item->address, TRUE);
    if (item->referrer) g_string_free(item->referrer, TRUE);
    if (item->desc) g_string_free(item->desc, TRUE);
    if(item->store && item->type == ReadingListColumnviewItemType::folder) g_object_unref(item->store);
    G_OBJECT_CLASS(reading_list_columnview_item_parent_class)->finalize(G_OBJECT(item));
}

void
reading_list_columnview_item_init(ReadingListColumnviewItem* item)
{

}

static void
reading_list_columnview_item_class_init(ReadingListColumnviewItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = reading_list_columnview_item_finalize;
}

GDateTime*
reading_list_columnview_item_get_date(ReadingListColumnviewItem* item)
{
    return READING_LIST_COLUMNVIEW_ITEM(item)->date;
}

orion::ReadingListEntryId
reading_list_columnview_item_get_id(ReadingListColumnviewItem* item)
{
    return READING_LIST_COLUMNVIEW_ITEM(item)->id;
}

GString*
reading_list_columnview_item_get_title(ReadingListColumnviewItem* item)
{
    return READING_LIST_COLUMNVIEW_ITEM(item)->title;
}

gboolean
reading_list_columnview_item_get_is_unred(ReadingListColumnviewItem* item)
{
    return READING_LIST_COLUMNVIEW_ITEM(item)->is_unred;
}

GString*
reading_list_columnview_item_get_address(ReadingListColumnviewItem* item)
{
    return READING_LIST_COLUMNVIEW_ITEM(item)->address;
}

GString*
reading_list_columnview_item_get_referrer(ReadingListColumnviewItem* item)
{
    return READING_LIST_COLUMNVIEW_ITEM(item)->referrer;
}

ReadingListColumnviewItemType
reading_list_columnview_item_get_item_type(ReadingListColumnviewItem* item)
{
    return READING_LIST_COLUMNVIEW_ITEM(item)->type;
}

GListStore*
reading_list_columnview_item_get_store(ReadingListColumnviewItem* item)
{
    return READING_LIST_COLUMNVIEW_ITEM(item)->store;
}

GString*
reading_list_columnview_item_get_desc(ReadingListColumnviewItem* item)
{
    return READING_LIST_COLUMNVIEW_ITEM(item)->desc;
}

void
reading_list_columnview_item_set_date(ReadingListColumnviewItem* item, GDateTime* date)
{
    if(item->date)
    {
        g_date_time_unref(item->date);
        item->date = NULL;
    }
    READING_LIST_COLUMNVIEW_ITEM(item)->date = date;
}

void
reading_list_columnview_item_set_title(ReadingListColumnviewItem* item, GString* title)
{
    if(item->title)
    {
        g_string_free(item->title, true);
        item->title = NULL;
    }
    READING_LIST_COLUMNVIEW_ITEM(item)->title = title;
}

void
reading_list_columnview_item_set_address(ReadingListColumnviewItem* item, GString* address)
{
    if(item->address)
    {
        g_string_free(item->address, true);
        item->address = NULL;
    }
    READING_LIST_COLUMNVIEW_ITEM(item)->address = address;
}

void
reading_list_columnview_item_set_is_unred(ReadingListColumnviewItem* item, gboolean is_unred)
{
    READING_LIST_COLUMNVIEW_ITEM(item)->is_unred = is_unred;
}

void
reading_list_columnview_item_set_referrer(ReadingListColumnviewItem* item, GString* referrer)
{
    if(item->referrer)
    {
        g_string_free(item->referrer, true);
        item->referrer = NULL;
    }
    READING_LIST_COLUMNVIEW_ITEM(item)->referrer = referrer;
}

void
reading_list_columnview_item_set_item_type(ReadingListColumnviewItem* item, ReadingListColumnviewItemType type)
{
    READING_LIST_COLUMNVIEW_ITEM(item)->type = type;
}

void
reading_list_columnview_item_folder_set_store(ReadingListColumnviewItem* item, GListStore* store)
{
    if(item->store)
    {
        g_object_unref(item->store);
    }
    g_object_ref(store);
    READING_LIST_COLUMNVIEW_ITEM(item)->store = store;
}

void
reading_list_columnview_item_address_set_store(ReadingListColumnviewItem* item, GListStore* store)
{
    READING_LIST_COLUMNVIEW_ITEM(item)->store = store;
}

int
reading_list_columnview_item_date_compare(gconstpointer item_a, gconstpointer item_b, gpointer user_data)
{
    GDateTime* time_a = reading_list_columnview_item_get_date(static_cast<ReadingListColumnviewItem*>(const_cast<gpointer>(item_a)));
    GDateTime* time_b = reading_list_columnview_item_get_date(static_cast<ReadingListColumnviewItem*>(const_cast<gpointer>(item_b)));
    return -g_date_time_compare(time_a, time_b);
}

void
reading_list_columnview_item_update_from_dto(ReadingListColumnviewItem* item, orion::ReadingListEntryDto dto)
{
    item->date = g_date_time_new_from_unix_utc(dto.added_on.value);

    std::string title = dto.title;
    item->title = g_string_new(title.c_str());
    std::string address = dto.address;
    item->address = g_string_new(address.c_str());
    std::string referrer = dto.referrer;
    item->referrer = g_string_new(referrer.c_str());
    item->id = dto.id;
}

ReadingListColumnviewItem*
reading_list_columnview_item_new_from_dto(const orion::ReadingListEntryDto& dto, GListStore* store)
{
	ReadingListColumnviewItem* item = static_cast<ReadingListColumnviewItem*>(g_object_new(READING_LIST_COLUMNVIEW_ITEM_TYPE, NULL));
    item->date = g_date_time_new_from_unix_utc(dto.added_on.value);

    std::string title = dto.title;
    item->title = g_string_new(title.c_str());
    std::string address = dto.address;
    item->address = g_string_new(address.c_str());
    std::string referrer = dto.referrer;
    item->referrer = g_string_new(referrer.c_str());
    item->id = dto.id;
    item->store = store;
    item->is_unred = dto.is_unread;

    item->type = ReadingListColumnviewItemType::address;

    return item;
}

ReadingListColumnviewItem*
reading_list_columnview_item_new_from_dto_with_desc(const orion::ReadingListEntryDto& dto, GListStore* store)
{
    ReadingListColumnviewItem* item =  reading_list_columnview_item_new_from_dto(dto, store);
    item->desc = g_string_new(dto.description.c_str());
    return item;
}

ReadingListColumnviewItem*
reading_list_columnview_item_new_day(GDateTime* date, uint items)
{
	ReadingListColumnviewItem* item = static_cast<ReadingListColumnviewItem*>(g_object_new(READING_LIST_COLUMNVIEW_ITEM_TYPE, NULL));
    item->date = date;
    item->title = NULL;
    item->referrer = NULL;
    item->type = ReadingListColumnviewItemType::folder;
	return item;
}

ReadingListColumnviewItem*
reading_list_columnview_item_new(GDateTime* date, GString* title, GString* address, GString* referrer, ReadingListColumnviewItemType type)
{
	ReadingListColumnviewItem* item = static_cast<ReadingListColumnviewItem*>(g_object_new(READING_LIST_COLUMNVIEW_ITEM_TYPE, NULL));
    item->date = date;
    item->title = title;
    item->address = address;
    item->referrer = referrer;
    item->type = type;
	return item;
}