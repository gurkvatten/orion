#pragma once

#include <common/src/dto/dto_member.h>
#include <core/dto/profile_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(SnapshotTabId, int64_t);

struct SnapshotDto {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(created_at, UnixTimeUtc64, UnixTimeUtc64(0));
};

struct CreateSnapshotReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(created_at, UnixTimeUtc64, UnixTimeUtc64(0));
};

struct GetLastSnapshotReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(snapshot, SnapshotDto);
};

struct SnapshotTab {
    DTO_MEMBER(id, SnapshotTabId, SnapshotTabId(0));
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(created_at, UnixTimeUtc64, UnixTimeUtc64(0));
    DTO_MEMBER(window_seq_num, double, 0.0);
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(is_pinned, bool, false);
    DTO_MEMBER(is_suspended, bool, false);
    DTO_MEMBER(seq_num, double, 0.0);
    DTO_MEMBER(uri, std::string);
    DTO_MEMBER(session_state, std::vector<uint8_t>);
    DTO_MEMBER(is_tab_group, bool, false);
    DTO_MEMBER(has_ui_window, bool, false);
    DTO_MEMBER(is_window_selected, bool, false);
    DTO_MEMBER(window_name, std::string);
    DTO_MEMBER(window_icon_name, std::string);
    DTO_MEMBER(window_keyboard_shortcut, std::string);
    DTO_MEMBER(window_is_suspended, bool, false);
    DTO_MEMBER(is_tab_selected, bool, false);
    DTO_MEMBER(back_forward_list_index, int64_t, 0);         // Orion back-forward list index
    DTO_MEMBER(webkit_back_forward_list_index, int64_t, 0);  // WebKit back-forward list index
    DTO_MEMBER(merged_back_forward_list_length, int64_t, 0); // length of the merged back-forward list (Orion + WebKit)
};

struct RestoreSnapshotQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(created_at, UnixTimeUtc64, UnixTimeUtc64(0));

    // On Mac, new snapshot is always created before restoring. If true, this will
    // create a new snapshot atomically together with restoring new old windows.
    // Changing parameter to true could malfunction due to closing windows before creating snapshot. 
    DTO_MEMBER(create_snapshot_before_restore, bool, false);
};

struct RestoreSnapshotReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(created_at, UnixTimeUtc64, UnixTimeUtc64(0));
};

struct DeleteSnapshotReply {
    DTO_MEMBER(err, Error);
};

} // namespace orion