#pragma once

#include <string>
#include <common/src/dto/dto_member.h>
#include <core/dto/bookmark_folder_dto.h>
#include <core/dto/tab_dto.h>
#include <common/src/strong_typedef.h>
#include <optional>

namespace orion {

DECLARE_STRONG_TYPEDEF(BookmarkId, int64_t);

struct BookmarkDto {
    DTO_MEMBER(id, BookmarkId, BookmarkId(0));
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0)); // the profile this bookmark belongs to
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(address, std::string);
    DTO_MEMBER(invocation_keyword, std::string); // or character
    DTO_MEMBER(parent_folder_id, BookmarkFolderId, BookmarkFolderId(0)); // zero means root level
    DTO_MEMBER(seq_num, double, 0.0); // sequence number inside the parent folder
};

struct GetBookmarkIdsQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(search_filter, std::string);
    DTO_MEMBER(parent_folder_id, std::optional<BookmarkFolderId>); // ignored if std::nullopt
    DTO_MEMBER(created_before, std::optional<UnixTimeUtc64>); // ignored if std::nullopt
};

struct GetBookmarkIdsReply {
    DTO_MEMBER(ids, std::vector<BookmarkId>);
    DTO_MEMBER(err, Error);
};

struct GetRecentBookmarkIdsQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
};

struct GetRecentBookmarkIdsReply {
    DTO_MEMBER(ids, std::vector<BookmarkId>);
    DTO_MEMBER(err, Error);
};

enum class BookmarkOrderByMode {
    parent_folder_id_asc_seq_num_asc = 0, // default
    created_at_desc = 1,
};

struct GetBookmarksQuery {
    DTO_MEMBER(ids, std::vector<BookmarkId>);
    DTO_MEMBER(order_by, BookmarkOrderByMode, BookmarkOrderByMode::parent_folder_id_asc_seq_num_asc);
};

struct GetBookmarksByQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(search_filter, std::string);
    DTO_MEMBER(parent_folder_id, std::optional<BookmarkFolderId>); // ignored if std::nullopt
    DTO_MEMBER(order_by, BookmarkOrderByMode, BookmarkOrderByMode::parent_folder_id_asc_seq_num_asc);
};

struct GetBookmarksReply {
    DTO_MEMBER(bookmarks, std::vector<BookmarkDto>);
    DTO_MEMBER(err, Error);
};

struct CreateBookmarkQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(address, std::string);
    DTO_MEMBER(invocation_keyword, std::string); // or character
    DTO_MEMBER(parent_folder_id, BookmarkFolderId, BookmarkFolderId(0)); // zero means root level

    // Sequence number inside the parent folder. If not provided (std::nullopt), then the bookmark
    // will be created at the end of the parent folder (appended to it). On collision with an
    // existing seq_num, the new bookmark will be inserted right after the existing one (this is
    // default behavior that can be changed by setting insert_mode). Also, when inserting a new
    // bookmark between two existing objects, the average of their seq_num's can be computed and
    // used as the new bookmark's seq_num.
    DTO_MEMBER(seq_num, std::optional<double>, std::nullopt);

    DTO_MEMBER(insert_mode, BookmarkInsertMode, BookmarkInsertMode::not_set);
};

struct CreateBookmarkReply {
    DTO_MEMBER(bookmark_id, BookmarkId, BookmarkId(0)); // the id of the created bookmark
    DTO_MEMBER(err, Error);
};

struct UpdateBookmarkQuery {
    DTO_MEMBER(id, BookmarkId, BookmarkId(0)); // the bookmark to update
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(address, std::string);
    DTO_MEMBER(invocation_keyword, std::string); // or character
    DTO_MEMBER(parent_folder_id, BookmarkFolderId, BookmarkFolderId(0)); // zero means root level

    // See also CreateBookmarkFolderQuery::seq_num.
    // On collision with an existing seq_num, the updated bookmark will be moved right after the
    // existing one (this is default behavior that can be changed by setting insert_mode).
    DTO_MEMBER(seq_num, double, 0.0);

    DTO_MEMBER(insert_mode, BookmarkInsertMode, BookmarkInsertMode::not_set);

    DTO_MEMBER(update_name, bool, false);
    DTO_MEMBER(update_address, bool, false);
    DTO_MEMBER(update_invocation_keyword, bool, false);
    DTO_MEMBER(update_parent_folder_id, bool, false);
    DTO_MEMBER(update_seq_num, bool, false);
};

struct MoveBookmarkQuery {
    DTO_MEMBER(bookmark_id, BookmarkId, BookmarkId(0));
    DTO_MEMBER(target_parent_folder_id, BookmarkFolderId, BookmarkFolderId(0)); // 0 = root level
    // The item that will follow the moved bookmark in sort order.
    // Set exactly one, or both 0 to append at end.
    DTO_MEMBER(next_bookmark_id, BookmarkId, BookmarkId(0));
    DTO_MEMBER(next_folder_id, BookmarkFolderId, BookmarkFolderId(0));
};

struct MoveBookmarkFolderQuery {
    DTO_MEMBER(folder_id, BookmarkFolderId, BookmarkFolderId(0));
    DTO_MEMBER(target_parent_folder_id, BookmarkFolderId, BookmarkFolderId(0)); // 0 = root level
    // The item that will follow the moved folder in sort order.
    // Set exactly one, or both 0 to append at end.
    DTO_MEMBER(next_bookmark_id, BookmarkId, BookmarkId(0));
    DTO_MEMBER(next_folder_id, BookmarkFolderId, BookmarkFolderId(0));
};

struct CopyBookmarksQuery {
    DTO_MEMBER(source_folder_ids, std::vector<BookmarkFolderId>);
    DTO_MEMBER(source_bookmark_ids, std::vector<BookmarkId>);
    DTO_MEMBER(target_folder_id, BookmarkFolderId, BookmarkFolderId(0)); // zero means root level
    DTO_MEMBER(target_bookmark_id, BookmarkId, BookmarkId(0));           // ignored if zero
};

struct CopyBookmarksReply {
    DTO_MEMBER(copied_folder_ids, std::vector<BookmarkFolderId>);
    DTO_MEMBER(copied_bookmark_ids, std::vector<BookmarkId>);
    DTO_MEMBER(err, Error);
};

struct BookmarksChange {
    DTO_MEMBER(created, std::vector<BookmarkDto>);
    DTO_MEMBER(updated, std::vector<BookmarkDto>);
    DTO_MEMBER(deleted, std::vector<BookmarkId>);
};

struct IsCurrentTabBookmarkedQuery {
    DTO_MEMBER(window_id, WindowId);
};

struct IsCurrentTabBookmarkedReply {
    DTO_MEMBER(is_bookmarked, bool, false);
    DTO_MEMBER(err, Error);
    DTO_MEMBER(current_tab_id, TabId);
    DTO_MEMBER(address, std::string);
};

enum class BookmarkItemType {
    not_set,
    folder,
    bookmark
};

struct BookmarkItemDto {
    DTO_MEMBER(folder, BookmarkFolderDto);
    DTO_MEMBER(bookmark, BookmarkDto);
    DTO_MEMBER(type, BookmarkItemType);
    DTO_MEMBER(children, std::vector<BookmarkItemDto>);
};

} //namespace orion