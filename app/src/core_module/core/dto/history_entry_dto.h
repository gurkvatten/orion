#pragma once

#include <string>
#include <common/src/dto/dto_member.h>
#include <core/dto/profile_dto.h>
#include <common/src/strong_types.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(HistoryEntryId, int64_t);

struct HistoryEntryDto {
    DTO_MEMBER(id, HistoryEntryId, HistoryEntryId(0));
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0)); // the profile this entry belongs to
    DTO_MEMBER(visited_at, UnixTimeUtc64, UnixTimeUtc64(0)); // unix timestamp in seconds in utc timezone
    DTO_MEMBER(website, std::string);
    DTO_MEMBER(address, std::string);
};

struct GetHistoryEntryIdsQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(visited_after, UnixTimeUtc64, UnixTimeUtc64(0));  // inclusive
    DTO_MEMBER(visited_before, UnixTimeUtc64, UnixTimeUtc64(0)); // exclusive
    DTO_MEMBER(search_filter, std::string);
};

struct GetHistoryEntryIdsReply {
    DTO_MEMBER(ids, std::vector<HistoryEntryId>);
    DTO_MEMBER(err, Error);
};

struct HistoryEntryDay {
    DTO_MEMBER(start_of_day, UnixTimeUtc64); // start of the day 00:00:00 in UTC
    DTO_MEMBER(visit_count, int64_t);        // number of history entries for this day
};

struct GetNonemptyHistoryDaysQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(search_filter, std::string);
};

struct GetNonemptyHistoryDaysReply {
    DTO_MEMBER(days, std::vector<HistoryEntryDay>);
    DTO_MEMBER(err, Error);
};

// Note: visited_at fields in each entry record are in local system time, not UTC.
struct GetHistoryEntriesReply {
    DTO_MEMBER(entries, std::vector<HistoryEntryDto>);
    DTO_MEMBER(err, Error);
};

struct CreateHistoryEntryQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0)); // the profile this entry belongs to
    DTO_MEMBER(visited_at, UnixTimeUtc64, UnixTimeUtc64(0));
    DTO_MEMBER(website, std::string);
    DTO_MEMBER(address, std::string);
};

struct CreateHistoryEntryReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, HistoryEntryId, HistoryEntryId(0));
};

struct ProfileHistoryQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0)); // the profile this entry belongs to
    DTO_MEMBER(search_filter, std::string);
    DTO_MEMBER(offset, int64_t, 0);
    DTO_MEMBER(limit, int64_t, 0);
};

struct HistoryEntriesChange {
    DTO_MEMBER(created, std::vector<HistoryEntryDto>);
    DTO_MEMBER(updated, std::vector<HistoryEntryDto>);
    DTO_MEMBER(deleted, std::vector<HistoryEntryId>);
};

struct HistoryEntryDaysChange {
    DTO_MEMBER(created, std::vector<HistoryEntryDay>);
    DTO_MEMBER(updated, std::vector<HistoryEntryDay>);
    DTO_MEMBER(deleted, std::vector<UnixTimeUtc64>);
};

} // namespace orion