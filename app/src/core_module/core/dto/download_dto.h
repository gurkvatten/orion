#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>

#include <common/src/dto/dto_member.h>
#include <common/src/error.h>
#include <core/dto/profile_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(DownloadId, int64_t);

enum class DownloadOperationState {
    downloading,
    finished,
    paused,
    failed,
    cancelled,
    cancelling,
};

// Represents a saved window snapshot or a tab group, not UI window
struct DownloadDto {
    DTO_MEMBER(id, DownloadId, DownloadId(0));
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_string, std::string);
    DTO_MEMBER(path, std::string);
    DTO_MEMBER(operation_state, DownloadOperationState);
    DTO_MEMBER(timestamp, UnixTimeUtc64);
    DTO_MEMBER(total_size, int64_t);
    DTO_MEMBER(current_size, int64_t);
    DTO_MEMBER(origin_uri, std::string);
};

struct GetDownloadIdsQuery {
    DTO_MEMBER(profile_id, ProfileId);
};

struct GetDownloadIdsReply {
    DTO_MEMBER(ids, std::vector<DownloadId>);
    DTO_MEMBER(err, Error);
};

struct GetDownloadsReply {
    DTO_MEMBER(downloads, std::vector<DownloadDto>);
    DTO_MEMBER(err, Error);
};

struct GetDownloadsPathReply {
    DTO_MEMBER(is_unset, bool);
    DTO_MEMBER(path, std::string);
    DTO_MEMBER(err, Error);
};

//if this gets modified, any places that hold a temporary buffer corresponding to a transaction should be updated
//currently known places:
//core_module/core/profiles_db/window.cpp:save_window(...)
struct DownloadsChange {
    DTO_MEMBER(created, std::vector<DownloadDto>);
    DTO_MEMBER(updated, std::vector<DownloadDto>);
    DTO_MEMBER(deleted, std::vector<DownloadId>);
};

struct CreateDownloadQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_string, std::string);
    DTO_MEMBER(path, std::string);
    DTO_MEMBER(operation_state, DownloadOperationState);
    DTO_MEMBER(timestamp, UnixTimeUtc64);
    DTO_MEMBER(total_size, int64_t);
    DTO_MEMBER(current_size, int64_t);
    DTO_MEMBER(origin_uri, std::string);
};

struct CreateDownloadReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, DownloadId);
};

// If update_operation_state is true and operation_state is DownloadOperationState::finished and
// settings::general::remove_download_items_mode is set to RemoveDownloadItemsMode::upon_successful_download,
// then the download will be removed from the database during the update operation.
struct UpdateDownloadQuery {
    DTO_MEMBER(id, DownloadId, DownloadId(0));

    DTO_MEMBER(update_name, bool);
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(update_icon_string, bool);
    DTO_MEMBER(icon_string, std::string);
    DTO_MEMBER(update_path, bool);
    DTO_MEMBER(path, std::string);
    DTO_MEMBER(update_operation_state, bool);
    DTO_MEMBER(operation_state, DownloadOperationState);
    DTO_MEMBER(update_timestamp, bool);
    DTO_MEMBER(timestamp, UnixTimeUtc64);
    DTO_MEMBER(update_total_size, bool);
    DTO_MEMBER(total_size, int64_t);
    DTO_MEMBER(update_current_size, bool);
    DTO_MEMBER(current_size, int64_t);
    DTO_MEMBER(update_origin_uri, bool);
    DTO_MEMBER(origin_uri, std::string);
};

struct DeleteDownloadsQuery {
    DTO_MEMBER(ids, std::vector<DownloadId>);
    // If set, then delete only downloads belonging to this profile. If set and ids vector is empty,
    // then delete all downloads belonging to this profile.
    DTO_MEMBER(profile_id, ProfileId);
    // If set, then delete only downloads older than this timestamp. Ignored if not set.
    DTO_MEMBER(older_than_timestamp, std::optional<UnixTimeUtc64>);
};

struct DeleteDownloadsReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(num_deleted, int64_t);
};

} // namespace orion
