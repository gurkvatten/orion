#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

enum class ReadingListColumnviewItemType { folder, address };

#define READING_LIST_COLUMNVIEW_ITEM_TYPE (reading_list_columnview_item_get_type())

G_DECLARE_FINAL_TYPE(ReadingListColumnviewItem, reading_list_columnview_item, READING_LIST_COLUMNVIEW, ITEM, GObject)

ReadingListColumnviewItem* reading_list_columnview_item_new(GDateTime* date, GString* title, GString* address, GString* referrer, ReadingListColumnviewItemType type);

ReadingListColumnviewItem* reading_list_columnview_item_new_from_dto(const orion::ReadingListEntryDto& dto, GListStore* store);
ReadingListColumnviewItem* reading_list_columnview_item_new_from_dto_with_desc(const orion::ReadingListEntryDto& dto, GListStore* store);
ReadingListColumnviewItem* reading_list_columnview_item_new_day(GDateTime* date, uint items);

void reading_list_columnview_item_update_from_dto(ReadingListColumnviewItem* item, orion::ReadingListEntryDto dto);

GDateTime* reading_list_columnview_item_get_date(ReadingListColumnviewItem* item);
orion::ReadingListEntryId reading_list_columnview_item_get_id(ReadingListColumnviewItem* item);
GString* reading_list_columnview_item_get_title(ReadingListColumnviewItem* item);
GString* reading_list_columnview_item_get_address(ReadingListColumnviewItem* item);
GString* reading_list_columnview_item_get_referrer(ReadingListColumnviewItem* item);
ReadingListColumnviewItemType reading_list_columnview_item_get_item_type(ReadingListColumnviewItem* item);
GListStore* reading_list_columnview_item_get_store(ReadingListColumnviewItem* item);
GString* reading_list_columnview_item_get_desc(ReadingListColumnviewItem* item);
gboolean reading_list_columnview_item_get_is_unred(ReadingListColumnviewItem* item);

void reading_list_columnview_item_set_date(ReadingListColumnviewItem* item, GDateTime*);
void reading_list_columnview_item_set_title(ReadingListColumnviewItem* item, GString* title);
void reading_list_columnview_item_set_address(ReadingListColumnviewItem* item, GString* address);
void reading_list_columnview_item_set_referrer(ReadingListColumnviewItem* item, GString* referrer);
void reading_list_columnview_item_set_item_type(ReadingListColumnviewItem* item, ReadingListColumnviewItemType type);
void reading_list_columnview_item_folder_set_store(ReadingListColumnviewItem* item, GListStore* store);
void reading_list_columnview_item_address_set_store(ReadingListColumnviewItem* item, GListStore* store);
void reading_list_columnview_item_set_is_unred(ReadingListColumnviewItem* item, gboolean is_unred);

int reading_list_columnview_item_date_compare(gconstpointer item_a, gconstpointer item_b, gpointer user_data);