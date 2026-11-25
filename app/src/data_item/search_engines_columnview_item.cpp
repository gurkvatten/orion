#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "data_item/search_engines_columnview_item.h"

struct _SearchEnginesColumnviewItem
{
    GObject parent;

    orion::SearchEngineId id;
    GString* name;
    GString* search_url_template;
    GString* suggestions_url_template;
    GString* keyword;
};

G_DEFINE_TYPE(SearchEnginesColumnviewItem, search_engines_columnview_item, G_TYPE_OBJECT)

void
search_engines_columnview_item_finalize(GObject* object)
{
    SearchEnginesColumnviewItem* item = SEARCH_ENGINES_COLUMNVIEW_ITEM(object);

    std::destroy_at(&item->id);

    g_string_free(item->name, TRUE);
    g_string_free(item->search_url_template, TRUE);
    g_string_free(item->suggestions_url_template, TRUE);
    g_string_free(item->keyword, TRUE);
    
    G_OBJECT_CLASS(search_engines_columnview_item_parent_class)->finalize(object);
}

void
search_engines_columnview_item_init(SearchEnginesColumnviewItem* item)
{
    std::construct_at(&item->id);
}

static void
search_engines_columnview_item_class_init(SearchEnginesColumnviewItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = search_engines_columnview_item_finalize;
}

orion::SearchEngineId
search_engines_columnview_item_get_id(SearchEnginesColumnviewItem* item)
{
    return SEARCH_ENGINES_COLUMNVIEW_ITEM(item)->id;
}

GString*
search_engines_columnview_item_get_name(SearchEnginesColumnviewItem* item)
{
    return SEARCH_ENGINES_COLUMNVIEW_ITEM(item)->name;
}

GString*
search_engines_columnview_item_get_search_url_template(SearchEnginesColumnviewItem* item)
{
    return SEARCH_ENGINES_COLUMNVIEW_ITEM(item)->search_url_template;
}

GString*
search_engines_columnview_item_get_suggestions_url_template(SearchEnginesColumnviewItem* item)
{
    return SEARCH_ENGINES_COLUMNVIEW_ITEM(item)->suggestions_url_template;
}

GString*
search_engines_columnview_item_get_keyword(SearchEnginesColumnviewItem* item)
{
    return SEARCH_ENGINES_COLUMNVIEW_ITEM(item)->keyword;
}

void
search_engines_columnview_item_set_name(SearchEnginesColumnviewItem* item, const char* name)
{
    g_string_assign(item->name, name);
}

void
search_engines_columnview_item_set_search_url_template(SearchEnginesColumnviewItem* item, const char* search_url_template)
{
    g_string_assign(item->search_url_template, search_url_template);
}

void
search_engines_columnview_item_set_suggestions_url_template(SearchEnginesColumnviewItem* item, const char* suggestions_url_template)
{
    g_string_assign(item->suggestions_url_template, suggestions_url_template);
}

void
search_engines_columnview_item_set_keyword(SearchEnginesColumnviewItem* item, const char* keyword)
{
    g_string_assign(item->keyword, keyword);
}

SearchEnginesColumnviewItem*
search_engines_columnview_item_new(const orion::SearchEngineDto& dto)
{
	SearchEnginesColumnviewItem* item = static_cast<SearchEnginesColumnviewItem*>(g_object_new(SEARCH_ENGINES_COLUMNVIEW_ITEM_TYPE, NULL));

    item->id = dto.id;
    item->name = g_string_new(dto.name.c_str());
    item->search_url_template = g_string_new(dto.search_url_template.c_str());
    item->suggestions_url_template = g_string_new(dto.suggestions_url_template.c_str());
    item->keyword = g_string_new(dto.keyword.c_str());

	return item;
}
