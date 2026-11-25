#pragma once

#include <string>

#include <common/src/dto/dto_member.h>
#include <core/dto/profile_dto.h>
#include <common/src/strong_types.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(OverflowMenuItemId, int64_t);

struct CreateOverflowMenuItemQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(action, std::string);
    DTO_MEMBER(command_id, int64_t);

    // 0-based index at which the new item should be inserted. If there is already an item
    // at the specified index, the new item and all items after it will be shifted by 1.
    // If the index is -1, the item will be appended to the end of the list.
    DTO_MEMBER(idx, int64_t);
};

struct CreateOverflowMenuItemReply {
    DTO_MEMBER(id, OverflowMenuItemId);
    DTO_MEMBER(err, Error);
};

struct OverflowMenuItemDto {
    DTO_MEMBER(id, OverflowMenuItemId);
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(action, std::string);
    DTO_MEMBER(command_id, int64_t);
    DTO_MEMBER(idx, int64_t);
};

struct GetOverflowMenuItemsQuery {
    // If non-zero, only items belonging to the specified profile will be returned
    DTO_MEMBER(profile_id, ProfileId);

    // If set, only items with the specified IDs will be returned
    DTO_MEMBER(ids, std::optional<std::vector<OverflowMenuItemId>>);
};

struct GetOverflowMenuItemsReply {
    DTO_MEMBER(items, std::vector<OverflowMenuItemDto>);
    DTO_MEMBER(err, Error);
};

struct MoveOverflowMenuItemQuery {
    DTO_MEMBER(id, OverflowMenuItemId);
    // The overflow menu item's ID that will follow the moved item in sort order.
    // If 0, the item will be moved to the end of the list.
    DTO_MEMBER(next_id, OverflowMenuItemId);
};

struct OverflowMenuItemsChange {
    DTO_MEMBER(created, std::vector<OverflowMenuItemDto>);
    DTO_MEMBER(updated, std::vector<OverflowMenuItemDto>);
    DTO_MEMBER(deleted, std::vector<OverflowMenuItemId>);
};

} // namespace orion