#pragma once

#include "app.h"
#include "window/main_window/main_window.h"

namespace orion::gtk {

void
delete_bookmarks(const std::vector<orion::BookmarkId>& ids);

void
delete_bookmark_folders(const std::vector<orion::BookmarkFolderId>& ids);

void
delete_history_entries(const std::vector<orion::HistoryEntryId>& ids);

void
delete_history_day(GDateTime* start_of_day);

void
delete_reading_list_entries(const std::vector<orion::ReadingListEntryId>& ids);

void
delete_reading_list_day(GDateTime* start_of_day, MainWindow* win);

} // namespace orion::gtk