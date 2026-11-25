#pragma once

#include <string>
#include <vector>
#include <optional>
#include <common/src/dto/dto_member.h>
#include <common/src/error.h>
#include <core/dto/profile_dto.h>
#include <core/dto/recently_closed_window_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(RecentlyClosedTabId, int64_t);

struct RecentlyClosedTabDto {
    DTO_MEMBER(id, RecentlyClosedTabId, {});
    DTO_MEMBER(profile_id, ProfileId, {});
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(uri, std::string);
    DTO_MEMBER(closed_at, UnixTimeUtc64, {});
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(window_id, std::optional<RecentlyClosedWindowId>);
};

struct GetRecentlyClosedTabIdsQuery {
    DTO_MEMBER(profile_id, ProfileId, {});
    DTO_MEMBER(window_id, std::optional<RecentlyClosedWindowId>); // ignored if std::nullopt
};

struct GetRecentlyClosedTabIdsReply {
    DTO_MEMBER(ids, std::vector<RecentlyClosedTabId>);
    DTO_MEMBER(err, Error);
};

struct GetRecentlyClosedTabsReply {
    DTO_MEMBER(tabs, std::vector<RecentlyClosedTabDto>);
    DTO_MEMBER(err, Error);
};

struct CreateRecentlyClosedTabQuery {
    DTO_MEMBER(profile_id, ProfileId, {});
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(uri, std::string);
    DTO_MEMBER(closed_at, UnixTimeUtc64, {});
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(window_id, std::optional<RecentlyClosedWindowId>);
};

struct CreateRecentlyClosedTabReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, RecentlyClosedTabId, {});
};

struct RecentlyClosedTabsChange {
    std::vector<RecentlyClosedTabDto> created;
    std::vector<RecentlyClosedTabId> deleted;
};

struct DeleteRecentlyClosedTabsQuery {
    DTO_MEMBER(ids, std::vector<RecentlyClosedTabId>);

    // If true, for each deleted tab, resets window_id to NULL of other tabs that had the same window_id
    // and deletes the window with this window_id.
    DTO_MEMBER(reset_window_ids, bool, false);
};

} // namespace orion