#include "data_util.h"

namespace orion::gtk {

void
delete_bookmarks(const std::vector<orion::BookmarkId>& ids)
{
    if(!ids.empty())
    {
        global_core_api->delete_bookmarks(ids, [](orion::Error err){VERIFY_LOG(err);});
    }
}

void
delete_bookmark_folders(const std::vector<orion::BookmarkFolderId>& ids)
{
    if(!ids.empty())
    {
        global_core_api->delete_bookmark_folders(ids, [](orion::Error err){VERIFY_LOG(err);});
    }
}

void
delete_history_entries(const std::vector<orion::HistoryEntryId>& ids)
{
    global_core_api->delete_history_entries(ids, [](orion::Error err){VERIFY_LOG(err);});
}

void
delete_history_day(GDateTime* start_of_day)
{
    orion::GetHistoryEntryIdsQuery query;
    query.visited_after = orion::UnixTimeUtc64(g_date_time_to_unix(start_of_day));
    GDateTime* end_of_day = g_date_time_add_days(start_of_day, 1);
    query.visited_before = orion::UnixTimeUtc64(g_date_time_to_unix(end_of_day));
    g_date_time_unref(end_of_day);
    global_core_api->get_history_entry_ids(query, [](orion::GetHistoryEntryIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->delete_history_entries(reply.ids, [](orion::Error err){VERIFY_LOG(err);});
    });
}

void
delete_reading_list_entries(const std::vector<orion::ReadingListEntryId>& ids)
{
    global_core_api->delete_reading_list_entries(ids, [](orion::Error err){VERIFY_LOG(err);});
}

void
delete_reading_list_day(GDateTime* start_of_day, MainWindow* win)
{
    (void)start_of_day;
    (void)win;
    //TODO: wait for core api getter for ids between timestamps

    orion::log_debug("WARNING: delete_reading_list_day not yet implemented, check data_util.cpp");
}

} // namespace orion::gtk