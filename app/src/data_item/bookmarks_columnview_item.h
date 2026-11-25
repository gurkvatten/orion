#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>
#include <variant>

#include <core/dto/bookmark_folder_dto.h>
#include <core/dto/bookmark_dto.h>

enum class BookmarksColumnviewItemType { folder, bookmark, folder_internal };

#define BOOKMARKS_COLUMNVIEW_ITEM_TYPE (bookmarks_columnview_item_get_type())

G_DECLARE_FINAL_TYPE(BookmarksColumnviewItem, bookmarks_columnview_item, BOOKMARKS_COLUMNVIEW, ITEM, GObject)

// Union of class types (which expects constructors/destructors to be called) must be handled
// carefully using std::construct_at and std::destroy_at. We use struct instead of union to avoid
// these issues.
struct BookmarkOrFolderId {
    orion::BookmarkFolderId folder_id;
    orion::BookmarkId bookmark_id;
};

gsize bookmarks_columnview_item_size();

BookmarksColumnviewItem* bookmarks_columnview_item_new(int items, GString* leftside_text, GString* rightside_text, const char* icon_name, BookmarksColumnviewItemType type);

BookmarksColumnviewItem* bookmarks_columnview_item_new_from_bookmark_struct(const orion::BookmarkDto& bookmark_struct, GListStore* folder_list_store);
BookmarksColumnviewItem* bookmarks_columnview_item_new_from_folder_struct(const orion::BookmarkFolderDto& folder_struct, GListStore* parent_folder_list_store, GListStore* subdirectory_list_store);

void bookmarks_columnview_item_update_from_folder_dto(BookmarksColumnviewItem* item, const orion::BookmarkFolderDto& folder);

int bookmarks_items_sequence_number_compare(gconstpointer item_a, gconstpointer item_b, gpointer user_data);

BookmarkOrFolderId bookmarks_columnview_item_get_id(BookmarksColumnviewItem* item);
int bookmarks_columnview_item_get_items(BookmarksColumnviewItem* item);
GString* bookmarks_columnview_item_get_leftside_text(BookmarksColumnviewItem* item);
GString* bookmarks_columnview_item_get_rightside_text(BookmarksColumnviewItem* item);
const char* bookmarks_columnview_item_get_icon_name(BookmarksColumnviewItem* item);
BookmarksColumnviewItemType bookmarks_columnview_item_get_item_type(BookmarksColumnviewItem* item);
GListStore* bookmarks_columnview_item_get_folder_list_store(BookmarksColumnviewItem* item);
GListStore* bookmarks_columnview_item_get_subdirectory_list_store(BookmarksColumnviewItem* item);
double bookmarks_columnview_item_get_sequence_number(gconstpointer item);
orion::BookmarkFolderId bookmarks_columnview_item_get_parent_folder_id(BookmarksColumnviewItem* item);
gboolean bookmarks_columnview_item_get_is_internal(BookmarksColumnviewItem* item);
GtkListView* bookmarks_columnview_item_get_parent_list_view(BookmarksColumnviewItem* item);

void bookmarks_columnview_item_set_sequence_number(BookmarksColumnviewItem* item, double seq_num);
void bookmarks_columnview_item_set_items(BookmarksColumnviewItem* item, int items);
void bookmarks_columnview_item_set_leftside_text(BookmarksColumnviewItem* item, GString* text);
void bookmarks_columnview_item_set_rightside_text(BookmarksColumnviewItem* item, GString* text);
void bookmarks_columnview_item_set_icon_name(BookmarksColumnviewItem* item, const char* icon_name);
void bookmarks_columnview_item_set_item_type(BookmarksColumnviewItem* item, BookmarksColumnviewItemType type);
void bookmarks_columnview_item_set_parent_folder_list_store(BookmarksColumnviewItem* item, GListStore* store);
void bookmarks_columnview_item_set_parent_list_view(BookmarksColumnviewItem* item, GtkListView* view);

struct IdAndType {
    BookmarksColumnviewItemType type;
    BookmarkOrFolderId id;
};

typedef struct {
    GArray* array;
} BookmarkArray;

GType bookmark_array_get_type (void) G_GNUC_CONST;
#define BOOKMARK_ARRAY_TYPE (bookmark_array_get_type())

BookmarkArray* bookmark_array_new();
BookmarkArray* bookmark_array_copy(BookmarkArray* self);
void bookmark_array_free(BookmarkArray* self);
