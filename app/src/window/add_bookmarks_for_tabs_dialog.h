#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define ADD_BOOKMARKS_FOR_TABS_DIALOG_TYPE (add_bookmarks_for_tabs_dialog_get_type ())

G_DECLARE_FINAL_TYPE(AddBookmarksForTabsDialog, add_bookmarks_for_tabs_dialog, ADD_BOOKMARKS_FOR_TABS, DIALOG, AdwDialog)

AdwDialog* add_bookmarks_for_tabs_dialog_new();
void add_bookmarks_for_tabs_dialog_set_folder_id(AddBookmarksForTabsDialog* win, orion::BookmarkFolderId folder_id);
void add_bookmarks_for_tabs_dialog_set_target_folder_name(AddBookmarksForTabsDialog* win, const char* name);
void add_bookmarks_for_tabs_dialog_set_source_tab_ids(AddBookmarksForTabsDialog* win, const std::vector<orion::TabId>& tab_ids);