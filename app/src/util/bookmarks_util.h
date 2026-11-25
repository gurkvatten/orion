#include <functional>
#include <../core_module/core/core_api.h>
#include <../core_module/core/dto/bookmark_dto.h>
#include <../core_module/core/dto/bookmark_folder_dto.h>
#include <vector>
#include "app.h"

struct BookmarksAllReply
{
    std::vector<orion::BookmarkFolderDto> folders;
    std::vector<orion::BookmarkDto> bookmarks;
    orion::Error error;
};

void fetch_all_bookmarks_data(orion::ProfileId profile_id, std::function<void(BookmarksAllReply)> callback);
void fetch_bookmarks_data(orion::ProfileId profile_id, std::string query, std::function<void(BookmarksAllReply)> callback); 