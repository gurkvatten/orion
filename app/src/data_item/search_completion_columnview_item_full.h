#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define SEARCH_COMPLETION_COLUMNVIEW_ITEM_FULL_TYPE (search_completion_columnview_item_full_get_type())

G_DECLARE_FINAL_TYPE(SearchCompletionColumnviewItemFull, search_completion_columnview_item_full, SEARCH_COMPLETION, COLUMNVIEW_ITEM_FULL, GObject)

SearchCompletionColumnviewItemFull* search_completion_columnview_item_full_new(GString* website, GString* address, const char* icon_name);
void search_completion_columnview_item_full_destroy(SearchCompletionColumnviewItemFull* item);
GString* search_completion_columnview_item_full_get_website(SearchCompletionColumnviewItemFull* item);
GString* search_completion_columnview_item_full_get_address(SearchCompletionColumnviewItemFull* item);
const char* search_completion_columnview_item_full_get_icon_name(SearchCompletionColumnviewItemFull* item);
void search_completion_columnview_item_full_set_website(SearchCompletionColumnviewItemFull* item, GString* text);
void search_completion_columnview_item_full_set_address(SearchCompletionColumnviewItemFull* item, GString* text);
void search_completion_columnview_item_full_set_icon_name(SearchCompletionColumnviewItemFull* item, const char* icon_name);
