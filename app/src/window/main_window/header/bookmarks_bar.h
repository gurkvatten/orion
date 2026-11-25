#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "util/click_util.h"
#include <core/dto/settings_dto.h>
#include <core/core_api.h>

#define APP_BOOKMARKS_BAR_TYPE (bookmarks_bar_get_type ())

G_DECLARE_FINAL_TYPE(BookmarksBar, bookmarks_bar, APP, BOOKMARKS_BAR, AdwBin)

struct BookmarksFoldersStruct;

GtkPopoverMenu* bookmarks_bar_get_folder_popover_menu(BookmarksBar* self);
GtkPopoverMenu* bookmarks_bar_get_bookmark_popover_menu(BookmarksBar* self);
void bookmarks_bar_set_style(BookmarksBar* self, orion::settings::BookmarksBarStyle style);
orion::settings::BookmarksBarStyle bookmarks_bar_get_style(BookmarksBar* self);
void bookmarks_bar_set_bookmarks_change_observer_id(BookmarksBar* self, orion::ObserverId id);
void bookmarks_bar_set_folders_change_observer_id(BookmarksBar* self, orion::ObserverId id);
orion::ObserverId bookmarks_bar_get_bookmarks_change_observer_id(BookmarksBar* self);
orion::ObserverId bookmarks_bar_get_folders_change_observer_id(BookmarksBar* self);
void bookmarks_bar_set_bookmarks(BookmarksBar* self, std::vector<orion::BookmarkDto> bookmarks);
void bookmarks_bar_set_folders(BookmarksBar* self, std::vector<orion::BookmarkFolderDto> folders);
std::vector<orion::BookmarkDto>* bookmarks_bar_get_bookmarks(BookmarksBar* self);
std::vector<orion::BookmarkFolderDto>* bookmarks_bar_get_folders(BookmarksBar* self);
void bookmarks_bar_set_bookmars_bar_folder_id(BookmarksBar* self, orion::BookmarkFolderId id);
orion::BookmarkFolderId bookmarks_bar_get_bookmars_bar_folder_id(BookmarksBar* self);
void bookmarks_bar_set_mini_toolbar_enabled(BookmarksBar* self, bool enabled);