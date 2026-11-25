#pragma once

#include <string>
#include <vector>
#include <optional>
#include <common/src/dto/dto_member.h>
#include <common/src/error.h>
#include <core/dto/profile_dto.h>
#include <common/src/strong_types.h>
#include <limits>

namespace orion {

DECLARE_STRONG_TYPEDEF(ReadingListEntryId, int64_t);

struct ReadingListEntryDto {
    DTO_MEMBER(id, ReadingListEntryId, ReadingListEntryId(0));
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(added_on, UnixTimeUtc64, UnixTimeUtc64(0));
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(title, std::string);
    DTO_MEMBER(address, std::string);
    DTO_MEMBER(referrer, std::string);
    DTO_MEMBER(is_unread, bool, true);
    DTO_MEMBER(is_saved_offline, bool, false);
    DTO_MEMBER(description, std::string);
};

struct GetReadingListEntryIdsQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(added_after, UnixTimeUtc64, UnixTimeUtc64(0));  // inclusive
    DTO_MEMBER(added_before, UnixTimeUtc64, UnixTimeUtc64Max); // exclusive
    DTO_MEMBER(search_filter, std::string);
    DTO_MEMBER(address, std::optional<std::string>);
};

struct GetReadingListEntryIdsReply {
    DTO_MEMBER(ids, std::vector<ReadingListEntryId>);
    DTO_MEMBER(err, Error);
};

struct GetReadingListEntriesReply {
    DTO_MEMBER(entries, std::vector<ReadingListEntryDto>);
    DTO_MEMBER(err, Error);
};

struct CreateReadingListEntryQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(added_on, UnixTimeUtc64, UnixTimeUtc64(0));
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(title, std::string);
    DTO_MEMBER(address, std::string);
    DTO_MEMBER(referrer, std::string);
    DTO_MEMBER(description, std::string);
};

struct CreateReadingListEntryReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, ReadingListEntryId, ReadingListEntryId(0));
};

struct UpdateReadingListEntryQuery {
    DTO_MEMBER(id, ReadingListEntryId);

    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(added_on, UnixTimeUtc64, UnixTimeUtc64(0));
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(title, std::string);
    // Address field cannot be updated to reduce complexity
    //DTO_MEMBER(address, std::string);
    DTO_MEMBER(referrer, std::string);
    DTO_MEMBER(is_unread, bool, true);
    DTO_MEMBER(is_saved_offline, bool, false);
    DTO_MEMBER(description, std::string);
    
    DTO_MEMBER(update_profile_id, bool, false);
    DTO_MEMBER(update_added_on, bool, false);
    DTO_MEMBER(update_icon_name, bool, false);
    DTO_MEMBER(update_title, bool, false);
    // Address field cannot be updated to reduce complexity
    //DTO_MEMBER(update_address, bool, false);
    DTO_MEMBER(update_referrer, bool, false);
    DTO_MEMBER(update_is_unread, bool, false);
    DTO_MEMBER(update_is_saved_offline, bool, false);
    DTO_MEMBER(update_description, bool, false);
};

struct ReadingListEntriesChange {
    DTO_MEMBER(created, std::vector<ReadingListEntryDto>);
    DTO_MEMBER(updated, std::vector<ReadingListEntryDto>);
    DTO_MEMBER(deleted, std::vector<ReadingListEntryId>);
};

} // namespace orion