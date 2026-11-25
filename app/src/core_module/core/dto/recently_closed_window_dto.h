#pragma once

#include <string>
#include <vector>
#include <common/src/dto/dto_member.h>
#include <common/src/error.h>
#include <core/dto/profile_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(RecentlyClosedWindowId, int64_t);

struct RecentlyClosedWindowDto {
    DTO_MEMBER(id, RecentlyClosedWindowId, {});
    DTO_MEMBER(profile_id, ProfileId, {});
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(closed_at, UnixTimeUtc64, {});
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(window_id, std::optional<RecentlyClosedWindowId>);
};

struct CreateRecentlyClosedWindowQuery {
    DTO_MEMBER(profile_id, ProfileId, {});
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(closed_at, UnixTimeUtc64, {});
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(window_id, std::optional<RecentlyClosedWindowId>);
};

struct GetRecentlyClosedWindowIdsReply {
    DTO_MEMBER(ids, std::vector<RecentlyClosedWindowId>);
    DTO_MEMBER(err, Error);
};

struct GetRecentlyClosedWindowsReply {
    DTO_MEMBER(windows, std::vector<RecentlyClosedWindowDto>);
    DTO_MEMBER(err, Error);
};

} // namespace orion