#pragma once

#include <common/src/dto/dto_member.h>
#include <core/dto/snapshot_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(SnapshotBackForwardEntryId, int64_t);

struct SnapshotBackForwardEntry {
    DTO_MEMBER(id, SnapshotBackForwardEntryId, SnapshotBackForwardEntryId(0));
    DTO_MEMBER(snapshot_tab_id, SnapshotTabId, SnapshotTabId(0));
    DTO_MEMBER(webkit_index, int64_t, 0); // WebKit back-forward list index
    DTO_MEMBER(uri, std::string);
};

} // namespace orion