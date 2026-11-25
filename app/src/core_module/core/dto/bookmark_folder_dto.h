#pragma once

#include <string>
#include <vector>
#include <optional>
#include <common/src/dto/dto_member.h>
#include <common/src/strong_typedef.h>
#include <common/src/error.h>
#include <core/dto/profile_dto.h>

namespace orion {

enum class BookmarkFolderType {
    normal        = 0, // "default" name cannot be used because it is a keyword
    bookmarks_bar = 1,
    favorites     = 2,
};

struct BookmarkFolderDto {
    DTO_MEMBER(id, BookmarkFolderId, BookmarkFolderId(0));
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0)); // the profile this folder belongs to
    DTO_MEMBER(name, std::string);

    // Bookmarks Bar and Favorites are internal (cannot be modified, deleted,
    // and they should have localized names in the UI)
    DTO_MEMBER(is_internal, bool, false);

    DTO_MEMBER(parent_folder_id, BookmarkFolderId, BookmarkFolderId(0)); // zero means root level
    DTO_MEMBER(seq_num, double, 0.0); // sequence number inside the parent folder
    DTO_MEMBER(type, BookmarkFolderType, BookmarkFolderType::normal);
    DTO_MEMBER(is_expanded, bool, false); // used for UI state in Bookmarks dialog
};

struct GetBookmarkFolderIdsReply {
    DTO_MEMBER(ids, std::vector<BookmarkFolderId>);
    DTO_MEMBER(err, Error);
};

struct GetBookmarkFoldersReply {
    DTO_MEMBER(folders, std::vector<BookmarkFolderDto>);
    DTO_MEMBER(err, Error);
};

struct GetBookmarkFolderIdByTypeReply {
    DTO_MEMBER(folder_id, BookmarkFolderId, BookmarkFolderId(0));
    DTO_MEMBER(err, Error);
};

enum class BookmarkInsertMode {
    not_set = 0, // default insert mode (this is typically the same as after)
    before  = 1,
    after   = 2,
};

struct CreateBookmarkFolderQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(parent_folder_id, BookmarkFolderId, BookmarkFolderId(0)); // zero means root level

    // Sequence number inside the parent folder. If not provided (std::nullopt), then the folder
    // will be created at the end of the parent folder (appended to it). On collision with an
    // existing seq_num, the new folder will be inserted right after the existing one (this is
    // default behavior that can be changed by setting insert_mode). Also, when inserting a new
    // folder between two existing objects, the average of their seq_num's can be computed and
    // used as the new folder's seq_num.
    DTO_MEMBER(seq_num, std::optional<double>, std::nullopt);

    DTO_MEMBER(insert_mode, BookmarkInsertMode, BookmarkInsertMode::not_set);
};

struct CreateBookmarkFolderReply {
    DTO_MEMBER(folder_id, BookmarkFolderId, BookmarkFolderId(0)); // the id of the created folder
    DTO_MEMBER(err, Error);
};

struct UpdateBookmarkFolderQuery {
    DTO_MEMBER(id, BookmarkFolderId, BookmarkFolderId(0)); // the folder to update
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(parent_folder_id, BookmarkFolderId, BookmarkFolderId(0)); // zero means root level

    // See also CreateBookmarkFolderQuery::seq_num.
    // On collision with an existing seq_num, the updated folder will be moved right after the
    // existing one.
    DTO_MEMBER(seq_num, double, 0.0);

    DTO_MEMBER(insert_mode, BookmarkInsertMode, BookmarkInsertMode::not_set);
    DTO_MEMBER(is_expanded, bool, false);

    DTO_MEMBER(update_name, bool, false);
    DTO_MEMBER(update_parent_folder_id, bool, false);
    DTO_MEMBER(update_seq_num, bool, false);
    DTO_MEMBER(update_is_expanded, bool, false);
};

struct BookmarkFoldersChange {
    DTO_MEMBER(created, std::vector<BookmarkFolderDto>);
    DTO_MEMBER(updated, std::vector<BookmarkFolderDto>);
    DTO_MEMBER(deleted, std::vector<BookmarkFolderId>);
};

} //namespace orion