#pragma once

#include <string>
#include <vector>
#include <optional>
#include <stdint.h>
#include <common/src/dto/dto_member.h>
#include <common/src/error.h>
#include <core/dto/window_dto.h>

namespace orion {

struct TabDto {
    DTO_MEMBER(id, TabId, TabId(0));
    DTO_MEMBER(window_id, WindowId, WindowId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(is_pinned, bool, false);
    DTO_MEMBER(is_suspended, bool, false);
    DTO_MEMBER(seq_num, double, 0.0);
    DTO_MEMBER(uri, std::string);
    DTO_MEMBER(session_state, std::vector<uint8_t>);

    // Index in the webview tab view, not stored in the database.
    // Changing this field does not trigger any tab change events.
    DTO_MEMBER(index, int64_t, 0);

    // relevant only in Vertical Tabs layout, zero (0) means root tab
    DTO_MEMBER(parent_tab_id, TabId, TabId(0));

    // relevant only in Vertical Tabs layout and only for tabs that have at least one child tab
    DTO_MEMBER(is_collapsed, bool, false);
    
    DTO_MEMBER(back_forward_list_index, int64_t, 0);         // Orion back-forward list index
    DTO_MEMBER(webkit_back_forward_list_index, int64_t, 0);  // WebKit back-forward list index
    DTO_MEMBER(merged_back_forward_list_length, int64_t, 0); // length of the merged back-forward list (Orion + WebKit)
    DTO_MEMBER(snapshot_image, std::vector<uint8_t>);        // for example PNG image data of the webview snapshot
};

struct GetTabIdsQuery {
    DTO_MEMBER(window_id, WindowId, WindowId(0));
};

struct GetTabIdsReply {
    DTO_MEMBER(ids, std::vector<TabId>);
    DTO_MEMBER(err, Error);
};

struct GetOtherTabIdsQuery {
    DTO_MEMBER(exclude_tab_id, TabId, TabId(0)); // the returned list will not include this tab id
    DTO_MEMBER(exclude_pinned_tabs, bool, false); // if true, the returned list will not include pinned tabs
    DTO_MEMBER(exclude_suspended_tabs, bool, false); // if true, the returned list will not include suspended tabs
};

struct GetOtherTabIdsReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(other_tab_ids, std::vector<TabId>);
    DTO_MEMBER(before_tab_ids, std::vector<TabId>); // ids of tabs that are before the excluded tab (by seq_num)
    DTO_MEMBER(after_tab_ids, std::vector<TabId>);  // ids of tabs that are after the excluded tab (by seq_num)
};

struct GetDescendantTabIdsReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(descendant_tab_ids, std::vector<TabId>);
};

struct GetTabsReply {
    DTO_MEMBER(tabs, std::vector<TabDto>);
    DTO_MEMBER(err, Error);
};

struct CreateTabQuery {
    DTO_MEMBER(window_id, WindowId, WindowId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(is_pinned, bool, false);
    DTO_MEMBER(is_suspended, bool, false);
    DTO_MEMBER(seq_num, std::optional<double>, std::nullopt); // if not set, the tab will be appended to the end
    DTO_MEMBER(uri, std::string);
    DTO_MEMBER(session_state, std::vector<uint8_t>);

    // relevant only in Vertical Tabs layout, zero (0) means root tab
    DTO_MEMBER(parent_tab_id, TabId, TabId(0));

    // relevant only in Vertical Tabs layout and only for tabs that have at least one child tab
    DTO_MEMBER(is_collapsed, bool, false);

    DTO_MEMBER(back_forward_list_index, int64_t, 0);         // Orion back-forward list index
    DTO_MEMBER(webkit_back_forward_list_index, int64_t, 0);  // WebKit back-forward list index
    DTO_MEMBER(merged_back_forward_list_length, int64_t, 0); // length of the merged back-forward list (Orion + WebKit)
    DTO_MEMBER(snapshot_image, std::vector<uint8_t>);        // for example PNG image data of the webview snapshot
};

struct CreateTabReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, TabId, TabId(0));
};

struct UpdateTabQuery {
    DTO_MEMBER(id, TabId, TabId(0));
    
    DTO_MEMBER(window_id, WindowId, WindowId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(is_pinned, bool, false);
    DTO_MEMBER(is_suspended, bool, false);
    DTO_MEMBER(seq_num, double, 0.0);
    DTO_MEMBER(uri, std::string);
    DTO_MEMBER(session_state, std::vector<uint8_t>);
    DTO_MEMBER(parent_tab_id, TabId, TabId(0));
    DTO_MEMBER(is_collapsed, bool, false);
    DTO_MEMBER(back_forward_list_index, int64_t, 0);         // Orion back-forward list index
    DTO_MEMBER(webkit_back_forward_list_index, int64_t, 0);  // WebKit back-forward list index
    DTO_MEMBER(merged_back_forward_list_length, int64_t, 0); // length of the merged back-forward list (Orion + WebKit)
    DTO_MEMBER(snapshot_image, std::vector<uint8_t>);        // for example PNG image data of the webview snapshot

    DTO_MEMBER(update_window_id, bool, false);
    DTO_MEMBER(update_name, bool, false);
    DTO_MEMBER(update_icon_name, bool, false);
    //also needs to have a set window id to know which window to move the tab into if unpinning in a tab group
    DTO_MEMBER(update_is_pinned, bool, false);
    DTO_MEMBER(update_is_suspended, bool, false);
    DTO_MEMBER(update_seq_num, bool, false);
    DTO_MEMBER(update_uri, bool, false);
    DTO_MEMBER(update_session_state, bool, false);
    DTO_MEMBER(update_parent_tab_id, bool, false);
    DTO_MEMBER(update_is_collapsed, bool, false);
    DTO_MEMBER(update_back_forward_list_index, bool, false);
    DTO_MEMBER(update_webkit_back_forward_list_index, bool, false);
    DTO_MEMBER(update_merged_back_forward_list_length, bool, false);
    DTO_MEMBER(update_snapshot_image, bool, false);

    // Relevant only for nested tabs. If true, moves all child tabs of this tab to the current parent of
    // this tab (if it exists). This is useful when reordering tabs in Standard Tabs layout.
    DTO_MEMBER(move_children_to_parent, bool, false);

    // Relevant only for nested tabs. If true and update_seq_num is also true, reorders all descendant tabs
    // of this tab according to the new seq_num of this tab.
    DTO_MEMBER(reorder_descendants, bool, false);
};

//if this gets modified, any places that hold a temporary buffer corresponding to a transaction should be updated
//currently known places:
//core_module/core/profiles_db/window.cpp:save_window(...)
struct TabsChange {
    DTO_MEMBER(created, std::vector<TabDto>);
    DTO_MEMBER(updated, std::vector<TabDto>);
    DTO_MEMBER(deleted, std::vector<TabId>);

    DTO_MEMBER(updated_window_id, bool, false);
    DTO_MEMBER(updated_name, bool, false);
    DTO_MEMBER(updated_icon_name, bool, false);
    DTO_MEMBER(updated_is_pinned, bool, false);
    DTO_MEMBER(updated_is_suspended, bool, false);
    DTO_MEMBER(updated_seq_num, bool, false);
    DTO_MEMBER(updated_uri, bool, false);
    DTO_MEMBER(updated_session_state, bool, false);
    DTO_MEMBER(updated_parent_tab_id, bool, false);
    DTO_MEMBER(updated_is_collapsed, bool, false);
    DTO_MEMBER(updated_back_forward_list_index, bool, false);
    DTO_MEMBER(updated_webkit_back_forward_list_index, bool, false);
    DTO_MEMBER(updated_merged_back_forward_list_length, bool, false);
    DTO_MEMBER(updated_snapshot_image, bool, false);
};

struct DuplicateTabQuery {
    DTO_MEMBER(id, TabId);
};

struct DuplicateTabWithNewWindowIdQuery {
    DTO_MEMBER(id, TabId);
    DTO_MEMBER(window_id, WindowId);
};

struct DuplicateTabReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(new_tab, TabDto);
};

struct CanMoveTabQuery {
    DTO_MEMBER(tab_id, TabId);
    DTO_MEMBER(target_window_id, WindowId);
    DTO_MEMBER(target_position, int64_t);

    // if true, then the tab should become a child of an existing tab at target_position
    DTO_MEMBER(group_with_existing, bool);
};

struct CanMoveTabReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(can_move, bool);
};

// Useful for nested tabs, used on Windows platform
struct MoveTabQuery {
    DTO_MEMBER(tab_id, TabId);
    DTO_MEMBER(target_window_id, WindowId);
    DTO_MEMBER(parent_tab_id, TabId);
    DTO_MEMBER(after_tab_id, TabId);
    DTO_MEMBER(is_nested_tabs_operation, bool); //if false, will instead drop children into parent
};

} // namespace orion