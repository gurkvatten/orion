#pragma once

#include <string>
#include <vector>
#include <optional>
#include <common/src/dto/dto_member.h>
#include <common/src/error.h>
#include <core/dto/profile_dto.h>
#include <common/src/strong_types.h>
#include <limits>
#include <core/dto/tab_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(TabBackForwardListEntryId, int64_t);

struct TabBackForwardListEntryDto {
    // for debugging purpose only, each entry is identified by tab_id + webkit_index pair
    DTO_MEMBER(id, TabBackForwardListEntryId, TabBackForwardListEntryId(0));

    DTO_MEMBER(tab_id, TabId, TabId(0));
    DTO_MEMBER(webkit_index, int64_t, 0); // index in WebKit's back-forward list
    DTO_MEMBER(uri, std::string);
};

struct GetTabBackForwardListReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(entries, std::vector<TabBackForwardListEntryDto>);
};

struct CreateTabBackForwardListEntryQuery {
    DTO_MEMBER(tab_id, TabId, TabId(0));
    DTO_MEMBER(webkit_index, int64_t, 0); // index in WebKit's back-forward list
    DTO_MEMBER(uri, std::string);
};

struct CreateTabBackForwardListEntryReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(entry, TabBackForwardListEntryDto, TabBackForwardListEntryDto{});
};

struct DeleteTabBackForwardListEntriesQuery {
    DTO_MEMBER(tab_id, TabId, TabId(0));

    // first index in WebKit's back-forward list that should be deleted, each entry with higher
    // index will also be deleted in the same tab_id
    DTO_MEMBER(start_webkit_index, int64_t, 0);
};

//if this gets modified, any places that hold a temporary buffer corresponding to a transaction should be updated
//currently known places:
//core_module/core/profiles_db/window.cpp:save_window(...)
struct TabBackForwardListEntriesChange {
    DTO_MEMBER(created, std::vector<TabBackForwardListEntryDto>);
    DTO_MEMBER(updated, std::vector<TabBackForwardListEntryDto>);
    DTO_MEMBER(deleted, std::vector<TabBackForwardListEntryDto>);
};

} // namespace orion