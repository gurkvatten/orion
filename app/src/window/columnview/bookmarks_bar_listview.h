#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <string>

#include "app.h"
#include "window/main_window/header/bookmarks_bar.h"

//used in search_completion_popover.cpp

void bookmarks_bar_list_view_fetch_backend_data(GtkListView* view, gpointer user_data, orion::ProfileId profile_id);
void bookmarks_bar_list_view_map(GtkListView* view, gpointer user_data);
void bookmarks_bar_list_view_unmap(GtkListView* view, gpointer user_data);
void bookmarks_bar_list_view_init(GtkListView* view, BookmarksBar* bookmarks_bar);
void bookmarks_bar_list_view_constructed(GtkListView* view, BookmarksBar* bookmarks_bar);
void bookmarks_bar_list_view_dispose(GtkListView* view, BookmarksBar* bookmarks_bar);
void bookmarks_bar_list_view_finalize(GtkListView* view, BookmarksBar* bookmarks_bar);
void bookmarks_bar_list_view_refresh(GtkListView* view);