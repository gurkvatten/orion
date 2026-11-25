#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <common/src/log.h>

#include <core/dto/bookmark_dto.h>
#include <core/dto/bookmark_folder_dto.h>

#include "data_item/bookmarks_columnview_item.h"

struct _BookmarksColumnviewItem
{
	GObject parent;
    BookmarkOrFolderId id;
	int items;
    GString* leftside_text;
    GString* rightside_text;
    const char* icon_name; //TODO: probably delete this, needs favicons
    BookmarksColumnviewItemType type;
    gboolean is_internal;
    double seq_num;
    orion::BookmarkFolderId parent_folder_id; //zero means root level
    GListStore* parent_folder_list_store; //holds a reference to the list store this item is in
    GListStore* subdirectory_list_store; //holds a reference to the list store of the directory under this item
    GtkListView* parent_folder_list_view; //useful in bookmarks bar listview
};

G_DEFINE_TYPE(BookmarksColumnviewItem, bookmarks_columnview_item, G_TYPE_OBJECT)

gsize
bookmarks_columnview_item_size()
{
    return sizeof(BookmarksColumnviewItem);
}

void
bookmarks_columnview_item_finalize(GObject* object)
{
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(object);
    g_string_free(item->leftside_text, TRUE);
    g_string_free(item->rightside_text, TRUE);
    if(item->subdirectory_list_store) g_object_unref(item->subdirectory_list_store);
    std::destroy_at(&item->id);
    std::destroy_at(&item->parent_folder_id);
    G_OBJECT_CLASS(bookmarks_columnview_item_parent_class)->finalize(G_OBJECT(item));
}

void
bookmarks_columnview_item_init(BookmarksColumnviewItem* item)
{
    std::construct_at(&item->id);
    std::construct_at(&item->parent_folder_id);
}

static void
bookmarks_columnview_item_class_init(BookmarksColumnviewItemClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = bookmarks_columnview_item_finalize;
}

int
bookmarks_columnview_item_get_items(BookmarksColumnviewItem* item)
{
    return BOOKMARKS_COLUMNVIEW_ITEM(item)->items;
}

BookmarkOrFolderId
bookmarks_columnview_item_get_id(BookmarksColumnviewItem* item)
{
    return BOOKMARKS_COLUMNVIEW_ITEM(item)->id;
}

GString*
bookmarks_columnview_item_get_leftside_text(BookmarksColumnviewItem* item)
{
    return BOOKMARKS_COLUMNVIEW_ITEM(item)->leftside_text;
}

GString*
bookmarks_columnview_item_get_rightside_text(BookmarksColumnviewItem* item)
{
    return BOOKMARKS_COLUMNVIEW_ITEM(item)->rightside_text;
}

const char*
bookmarks_columnview_item_get_icon_name(BookmarksColumnviewItem* item)
{
    return BOOKMARKS_COLUMNVIEW_ITEM(item)->icon_name;
}

BookmarksColumnviewItemType
bookmarks_columnview_item_get_item_type(BookmarksColumnviewItem* item)
{
    return BOOKMARKS_COLUMNVIEW_ITEM(item)->type;
}

double
bookmarks_columnview_item_get_sequence_number(gconstpointer item)
{
    //TODO: not sure how bad this is
    BookmarksColumnviewItem* sanitized_item = BOOKMARKS_COLUMNVIEW_ITEM(const_cast<gpointer>(item));
    return sanitized_item->seq_num;
}

orion::BookmarkFolderId
bookmarks_columnview_item_get_parent_folder_id(BookmarksColumnviewItem* item)
{
    return BOOKMARKS_COLUMNVIEW_ITEM(item)->parent_folder_id;
}

GListStore*
bookmarks_columnview_item_get_folder_list_store(BookmarksColumnviewItem* item)
{
    return BOOKMARKS_COLUMNVIEW_ITEM(item)->parent_folder_list_store;
}

GListStore*
bookmarks_columnview_item_get_subdirectory_list_store(BookmarksColumnviewItem* item)
{
    return BOOKMARKS_COLUMNVIEW_ITEM(item)->subdirectory_list_store;
}

gboolean
bookmarks_columnview_item_get_is_internal(BookmarksColumnviewItem* item)
{
    return BOOKMARKS_COLUMNVIEW_ITEM(item)->is_internal;
}

void
bookmarks_columnview_item_set_sequence_number(BookmarksColumnviewItem* item, int seq_num)
{
    BOOKMARKS_COLUMNVIEW_ITEM(item)->seq_num = seq_num;
}

void
bookmarks_columnview_item_set_items(BookmarksColumnviewItem* item, int items)
{
    BOOKMARKS_COLUMNVIEW_ITEM(item)->items = items;
}

void
bookmarks_columnview_item_set_leftside_text(BookmarksColumnviewItem* item, GString* text)
{
    BOOKMARKS_COLUMNVIEW_ITEM(item)->leftside_text = text;
}

void
bookmarks_columnview_item_set_rightside_text(BookmarksColumnviewItem* item, GString* text)
{
    BOOKMARKS_COLUMNVIEW_ITEM(item)->rightside_text = text;
}

void
bookmarks_columnview_item_set_item_type(BookmarksColumnviewItem* item, BookmarksColumnviewItemType type)
{
    BOOKMARKS_COLUMNVIEW_ITEM(item)->type = type;
}

void
bookmarks_columnview_item_set_icon_name(BookmarksColumnviewItem* item, const char* icon_name)
{
    BOOKMARKS_COLUMNVIEW_ITEM(item)->icon_name = icon_name;
}

static void
folder_list_store_items_changed()
{
    //
}

void
bookmarks_columnview_item_set_parent_folder_list_store(BookmarksColumnviewItem* item, GListStore* store)
{
    g_signal_connect(store, "items-changed", G_CALLBACK(folder_list_store_items_changed), item);
    BOOKMARKS_COLUMNVIEW_ITEM(item)->parent_folder_list_store = store;
}

void
bookmarks_columnview_item_set_parent_list_view(BookmarksColumnviewItem* item, GtkListView* view)
{
    BOOKMARKS_COLUMNVIEW_ITEM(item)->parent_folder_list_view = view;
}

GtkListView*
bookmarks_columnview_item_get_parent_list_view(BookmarksColumnviewItem* item)
{
    return BOOKMARKS_COLUMNVIEW_ITEM(item)->parent_folder_list_view;
}

int
bookmarks_items_sequence_number_compare(gconstpointer item_a, gconstpointer item_b, gpointer user_data)
{
    double seq_num_a = bookmarks_columnview_item_get_sequence_number(item_a);
    double seq_num_b = bookmarks_columnview_item_get_sequence_number(item_b);
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

void
bookmarks_columnview_item_update_from_folder_dto(BookmarksColumnviewItem* item, const orion::BookmarkFolderDto& folder)
{
    g_return_if_fail(BOOKMARKS_COLUMNVIEW_IS_ITEM(item));
    item->parent_folder_id = folder.parent_folder_id;
    if(item->leftside_text) g_string_free(item->leftside_text, true);
    item->leftside_text = g_string_new(folder.name.c_str());
    item->seq_num = folder.seq_num;
}

BookmarksColumnviewItem*
bookmarks_columnview_item_new_from_bookmark_struct(const orion::BookmarkDto& bookmark_struct, GListStore* parent_folder_list_store)
{
	BookmarksColumnviewItem* item = static_cast<BookmarksColumnviewItem*>(g_object_new(BOOKMARKS_COLUMNVIEW_ITEM_TYPE, NULL));
    item->id.bookmark_id = bookmark_struct.id;
    item->items = 0;
    item->leftside_text = g_string_new(bookmark_struct.name.c_str());
    item->rightside_text = g_string_new(bookmark_struct.address.c_str());
    item->icon_name = "";
    item->type = BookmarksColumnviewItemType::bookmark;
    item->seq_num = bookmark_struct.seq_num;
    item->parent_folder_id = bookmark_struct.parent_folder_id;
    //I believe the struct is zeroed out in preparation so this should be unnecessary
    //item->is_internal = false;

    item->parent_folder_list_store = parent_folder_list_store;

    return item;
}

BookmarksColumnviewItem*
bookmarks_columnview_item_new_from_folder_struct(const orion::BookmarkFolderDto& folder_struct, GListStore* parent_folder_list_store, GListStore* subdirectory_list_store)
{
	BookmarksColumnviewItem* item = static_cast<BookmarksColumnviewItem*>(g_object_new(BOOKMARKS_COLUMNVIEW_ITEM_TYPE, NULL));
    item->id.folder_id = folder_struct.id;
    item->items = 0;
    item->leftside_text = g_string_new(folder_struct.name.c_str());
    item->rightside_text = g_string_new("");
    item->is_internal = folder_struct.is_internal;
    
    item->seq_num = folder_struct.seq_num;
    item->parent_folder_list_store = parent_folder_list_store;
    item->subdirectory_list_store = subdirectory_list_store;
    item->parent_folder_id = folder_struct.parent_folder_id;

    if(folder_struct.is_internal)
    {
        if(folder_struct.type == orion::BookmarkFolderType::bookmarks_bar)
        {
            item->icon_name = "library-symbolic";
        }
        else if(folder_struct.type == orion::BookmarkFolderType::favorites)
        {
            item->icon_name = "star-outline-thick-symbolic";
        }
        item->type = BookmarksColumnviewItemType::folder_internal;
    }
    else
    {
        item->icon_name = "folder-visiting-symbolic"; //TODO: better icon? 
        item->type = BookmarksColumnviewItemType::folder;
    }

    return item;
}

BookmarksColumnviewItem*
bookmarks_columnview_item_new(int items, GString* leftside_text, GString* rightside_text, const char* icon_name, BookmarksColumnviewItemType type)
{
	BookmarksColumnviewItem* item = static_cast<BookmarksColumnviewItem*>(g_object_new(BOOKMARKS_COLUMNVIEW_ITEM_TYPE, NULL));
    item->items = items;
    item->leftside_text = leftside_text;
    item->rightside_text = rightside_text;
    item->icon_name = icon_name;
    item->type = type;
	return item;
}


BookmarkArray* bookmark_array_new()
{
    BookmarkArray* res = g_new(BookmarkArray, 1);
    if(res!=NULL)
    {
        res->array = g_array_new(false, false, sizeof(IdAndType));
    }
    return res;
}


BookmarkArray* bookmark_array_copy(BookmarkArray* self)
{
    BookmarkArray* res = g_new(BookmarkArray, 1);
    if(res!=NULL)
    {
        res->array = g_array_copy(self->array);
    }
    return res;
}

void bookmark_array_free(BookmarkArray* self)
{
    g_array_free(self->array, true);
    g_free(self);
}


G_DEFINE_BOXED_TYPE(BookmarkArray, bookmark_array, bookmark_array_copy, bookmark_array_free);



