#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "data_item/search_completion_columnview_item_full.h"

struct _SearchCompletionColumnviewItemFull
{
	GObject parent;
    GString* website;
    GString* address;
    const char* icon_name;
};

G_DEFINE_TYPE(SearchCompletionColumnviewItemFull, search_completion_columnview_item_full, G_TYPE_OBJECT)

void
search_completion_columnview_item_full_finalize(GObject* object)
{
    SearchCompletionColumnviewItemFull* item = SEARCH_COMPLETION_COLUMNVIEW_ITEM_FULL(object);
    g_string_free(item->website, TRUE);
    g_string_free(item->address, TRUE);
    G_OBJECT_CLASS(search_completion_columnview_item_full_parent_class)->finalize(object);
}

void
search_completion_columnview_item_full_init(SearchCompletionColumnviewItemFull* item)
{

}

static void
search_completion_columnview_item_full_class_init(SearchCompletionColumnviewItemFullClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = search_completion_columnview_item_full_finalize;
}

GString*
search_completion_columnview_item_full_get_website(SearchCompletionColumnviewItemFull* item)
{
    return SEARCH_COMPLETION_COLUMNVIEW_ITEM_FULL(item)->website;
}

GString*
search_completion_columnview_item_full_get_address(SearchCompletionColumnviewItemFull* item)
{
    return SEARCH_COMPLETION_COLUMNVIEW_ITEM_FULL(item)->address;
}

const char*
search_completion_columnview_item_full_get_icon_name(SearchCompletionColumnviewItemFull* item)
{
    return SEARCH_COMPLETION_COLUMNVIEW_ITEM_FULL(item)->icon_name;
}

void
search_completion_columnview_item_full_set_website(SearchCompletionColumnviewItemFull* item, GString* text)
{
    SEARCH_COMPLETION_COLUMNVIEW_ITEM_FULL(item)->website = text;
}

void
search_completion_columnview_item_full_set_address(SearchCompletionColumnviewItemFull* item, GString* text)
{
    SEARCH_COMPLETION_COLUMNVIEW_ITEM_FULL(item)->address = text;
}

void
search_completion_columnview_item_full_set_icon_name(SearchCompletionColumnviewItemFull* item, const char* icon_name)
{
    SEARCH_COMPLETION_COLUMNVIEW_ITEM_FULL(item)->icon_name = icon_name;
}

void
search_completion_columnview_item_full_destroy(SearchCompletionColumnviewItemFull* item)
{
    if(item!=NULL)
    {
        SearchCompletionColumnviewItemFull* safe_item = SEARCH_COMPLETION_COLUMNVIEW_ITEM_FULL(item);
        g_string_free(safe_item->website,true);
        g_string_free(safe_item->address,true);
    }
}

SearchCompletionColumnviewItemFull*
search_completion_columnview_item_full_new(GString* website, GString* address, const char* icon_name)
{
	SearchCompletionColumnviewItemFull* item = static_cast<SearchCompletionColumnviewItemFull*>(g_object_new(SEARCH_COMPLETION_COLUMNVIEW_ITEM_FULL_TYPE, NULL));
    item->website = website;
    item->address = address;
    item->icon_name = icon_name;
	return item;
}