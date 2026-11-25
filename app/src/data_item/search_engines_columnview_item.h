#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>
#include <core/dto/search_engine_dto.h>

#define SEARCH_ENGINES_COLUMNVIEW_ITEM_TYPE (search_engines_columnview_item_get_type ())

G_DECLARE_FINAL_TYPE(SearchEnginesColumnviewItem, search_engines_columnview_item, SEARCH_ENGINES_COLUMNVIEW, ITEM, GObject)

SearchEnginesColumnviewItem* search_engines_columnview_item_new(const orion::SearchEngineDto& dto);
orion::SearchEngineId search_engines_columnview_item_get_id(SearchEnginesColumnviewItem* item);
GString* search_engines_columnview_item_get_name(SearchEnginesColumnviewItem* item);
GString* search_engines_columnview_item_get_search_url_template(SearchEnginesColumnviewItem* item);
GString* search_engines_columnview_item_get_suggestions_url_template(SearchEnginesColumnviewItem* item);
GString* search_engines_columnview_item_get_keyword(SearchEnginesColumnviewItem* item);
void search_engines_columnview_item_set_name(SearchEnginesColumnviewItem* item, const char* name);
void search_engines_columnview_item_set_search_url_template(SearchEnginesColumnviewItem* item, const char* search_url);
void search_engines_columnview_item_set_suggestions_url_template(SearchEnginesColumnviewItem* item, const char* suggestions_url);
void search_engines_columnview_item_set_keyword(SearchEnginesColumnviewItem* item, const char* keyword);
