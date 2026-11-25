#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#include <core/dto/history_entry_dto.h>

extern int global_history_columnview_item_count;

enum class HistoryColumnviewItemType { folder, address };

#define HISTORY_COLUMNVIEW_ITEM_TYPE (history_columnview_item_get_type())

G_DECLARE_FINAL_TYPE(HistoryColumnviewItem, history_columnview_item, HISTORY_COLUMNVIEW, ITEM, GObject)

HistoryColumnviewItem* history_columnview_item_new(GDateTime* date, GString* website, GString* address, HistoryColumnviewItemType type);

HistoryColumnviewItem* history_columnview_item_new_from_history_struct(const orion::HistoryEntryDto& history_struct, GListStore* folder_list_store);

//should always be set to start of day in current timezone
HistoryColumnviewItem* history_columnview_item_new_from_day(const orion::HistoryEntryDay&);

GDateTime* history_columnview_item_get_date(HistoryColumnviewItem* item);
GString* history_columnview_item_get_website(HistoryColumnviewItem* item);
GString* history_columnview_item_get_address(HistoryColumnviewItem* item);
uint history_columnview_item_get_items(HistoryColumnviewItem* item);
orion::HistoryEntryId history_columnview_item_get_id(HistoryColumnviewItem* item);
GListStore* history_columnview_item_get_list_store(HistoryColumnviewItem* item);
HistoryColumnviewItemType history_columnview_item_get_item_type(HistoryColumnviewItem* item);
std::optional<orion::ObserverId> history_columnview_item_get_observer_id(HistoryColumnviewItem* item);

void history_columnview_item_set_website(HistoryColumnviewItem* item, GString* website);
void history_columnview_item_set_address(HistoryColumnviewItem* item, GString* address);
void history_columnview_item_set_item_type(HistoryColumnviewItem* item, HistoryColumnviewItemType type);
void history_columnview_item_set_items(HistoryColumnviewItem* item, uint items);
void history_columnview_item_increment_items(HistoryColumnviewItem* item);
void history_columnview_item_decrement_items(HistoryColumnviewItem* item);
void history_columnview_item_set_observer_id(HistoryColumnviewItem* item, std::optional<orion::ObserverId> id);
void history_columnview_item_set_items_label(HistoryColumnviewItem* item, GtkLabel* label);
