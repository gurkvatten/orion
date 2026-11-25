#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>
#include "util/click_util.h"

#define BOOKMARKS_DIALOG_TYPE (bookmarks_dialog_get_type ())

G_DECLARE_FINAL_TYPE(BookmarksDialog, bookmarks_dialog, BOOKMARKS, DIALOG, AdwDialog)

AdwDialog* bookmarks_dialog_new();

void bookmarks_dialog_sidebar_select(BookmarksDialog* win, int i);
void bookmarks_dialog_add_dynamic_popover_menu(BookmarksDialog* self, GtkPopoverMenu* menu);
void bookmarks_dialog_add_history_entries_observer_id(BookmarksDialog* self, orion::ObserverId id);
void bookmarks_dialog_remove_history_entries_observer_id(BookmarksDialog* self, orion::ObserverId id);
void bookmarks_dialog_clear_history_entries_observer_ids(BookmarksDialog* self);
std::vector<orion::ObserverId> bookmarks_dialog_get_history_entries_observer_ids(BookmarksDialog* self);

GtkPopoverMenu* bookmarks_dialog_get_bookmarks_internal_folder_popover_menu(BookmarksDialog* self);
GtkPopoverMenu* bookmarks_dialog_get_bookmarks_folder_popover_menu(BookmarksDialog* self);
GtkPopoverMenu* bookmarks_dialog_get_bookmarks_bookmark_popover_menu(BookmarksDialog* self);
GtkPopoverMenu* bookmarks_dialog_get_bookmarks_column_view_popover_menu(BookmarksDialog* self);

GtkColumnView* bookmarks_dialog_get_bookmarks_columnview(BookmarksDialog* self);
GtkColumnView* bookmarks_dialog_get_history_columnview(BookmarksDialog* self);
GtkColumnView* bookmarks_dialog_get_reading_list_columnview(BookmarksDialog* self);

GtkPopoverMenu* bookmarks_dialog_get_history_folder_popover_menu(BookmarksDialog* self);

GtkPopoverMenu* bookmarks_dialog_get_reading_list_folder_popover_menu(BookmarksDialog* self);
GtkPopoverMenu* bookmarks_dialog_get_reading_list_address_popover_menu(BookmarksDialog* self);

void bookmarks_dialog_set_passwords_popover_menu(BookmarksDialog* self, GtkPopoverMenu* menu);
GtkPopoverMenu* bookmarks_dialog_get_passwords_popover_menu(BookmarksDialog* self);

GtkPopoverMenu* bookmarks_dialog_get_profiles_popover_menu(BookmarksDialog* self);
GtkPopoverMenu* bookmarks_dialog_get_internal_profile_popover_menu(BookmarksDialog* self);

void bookmarks_dialog_set_selected_bookmark_folder(BookmarksDialog* self, std::optional<orion::BookmarkFolderId> id);
std::optional<orion::BookmarkFolderId> bookmarks_dialog_get_selected_bookmark_folder(BookmarksDialog* self);

std::string bookmarks_dialog_get_history_search_filter(BookmarksDialog* self);
std::string bookmarks_dialog_get_passwords_search_filter(BookmarksDialog* self);
void bookmarks_dialog_on_passwords_columnview_changed(BookmarksDialog* self);