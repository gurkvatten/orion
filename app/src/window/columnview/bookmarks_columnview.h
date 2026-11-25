#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>
#include <vector>

#include "window/bookmarks_dialog.h"

#include <core/dto/bookmark_dto.h>
#include <core/dto/bookmark_folder_dto.h>

//used in bookmarks_dialog.cpp

void bookmarks_column_view_map(GtkColumnView* bookmarksColumnView, BookmarksDialog* win);
void bookmarks_column_view_map_flat_query(GtkColumnView* bookmarks_column_view, BookmarksDialog* win, std::string query);

void bookmarks_column_view_init(GtkColumnView* bookmarksColumnView, BookmarksDialog* win);
void on_bookmarks_column_view_activate(GtkColumnView* view, guint pos, gpointer user_data);

void bookmarks_columnview_model_user_data_destroy(gpointer data);

GListModel* bookmarks_columnview_model_create_submodel(gpointer item, gpointer data);

void on_bookmarks_folders_change(std::vector<orion::BookmarkFolderDto>* folders_vec_ptr, GtkColumnView* column_view, orion::BookmarkFoldersChange& change);
void on_bookmarks_change(std::vector<orion::BookmarkDto>* bookmarks_vec_ptr, GtkColumnView* column_view, orion::BookmarksChange& change);

GdkContentProvider* bookmarks_columnview_on_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data);
gboolean bookmarks_columnview_on_drop_target_drop(GtkDropTarget* self, const GValue* value, gdouble x, gdouble y, gpointer user_data);
GdkDragAction bookmarks_columnview_on_drop_target_motion(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data);
GdkDragAction bookmarks_columnview_on_drop_target_leave(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data);
gboolean bookmarks_columnview_on_drop_target_accept(GtkDropTarget* self, GdkDrop* drop, gpointer user_data);



struct BookmarksColumnviewTreeListModelArgs {
    std::vector<orion::BookmarkDto>* bookmark_struct_vector;
    std::vector<orion::BookmarkFolderDto>* folder_struct_vector;
};