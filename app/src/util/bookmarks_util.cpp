#include "bookmarks_util.h"

//On error returns TRUE
static bool
dispatch_callback_on_error(
    orion::Error err,
    std::function<void(BookmarksAllReply)> callback)
{
    if (err)
    {
        std::vector<orion::BookmarkFolderDto> empty_folders;
        std::vector<orion::BookmarkDto> empty_bookmarks;
        callback({empty_folders, empty_bookmarks, WRAP_ERROR(err)});
        return true;
    }

    return false;
}

void
fetch_all_bookmarks_data(orion::ProfileId profile_id,
    std::function<void(BookmarksAllReply)> callback)
{
    fetch_bookmarks_data(profile_id, "" ,std::move(callback));
}

void
fetch_bookmarks_data(orion::ProfileId profile_id, std::string query,
    std::function<void(BookmarksAllReply)> callback)
{
    global_core_api->get_bookmark_folder_ids(profile_id, [profile_id, callback, query_copy = std::move(query)](orion::GetBookmarkFolderIdsReply reply)
    {
        if(dispatch_callback_on_error(reply.err, callback)) return;
        
        global_core_api->get_bookmark_folders(reply.ids, [profile_id, callback, query_copy = std::move(query_copy)](orion::GetBookmarkFoldersReply reply)
        {
            if(dispatch_callback_on_error(reply.err, callback)) return;
            //MAKE_ERROR(orion::ErrorCode::not_found, "test")

            std::vector<orion::BookmarkFolderDto> folders = std::move(reply.folders);
            orion::GetBookmarkIdsQuery query{};
            query.profile_id = profile_id;
            if(!query_copy.empty()){
                query.search_filter = std::move(query_copy);
            }
            global_core_api->get_bookmark_ids(query, [folders = std::move(folders), callback](orion::GetBookmarkIdsReply reply) mutable
            {
                if(dispatch_callback_on_error(reply.err, callback)) return;
                
                orion::GetBookmarksQuery query{
                    .ids = reply.ids,
                };
                global_core_api->get_bookmarks(query, [folders = std::move(folders), callback](orion::GetBookmarksReply reply) mutable
                {
                    if(dispatch_callback_on_error(reply.err, callback)) return;

                    callback({std::move(folders), std::move(reply.bookmarks), reply.err});
                });
            });
        });
    });
}
