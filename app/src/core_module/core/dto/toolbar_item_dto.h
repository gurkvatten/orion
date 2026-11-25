#pragma once

#include <string>
#include <unordered_map>

#include <common/src/dto/dto_member.h>
#include <core/dto/profile_dto.h>
#include <common/src/strong_types.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(ToolbarItemId, int64_t);

#define TOOLBAR_ITEMS(X) \
    X(not_set,             0) \
    X(summarize_page,      1) \
    X(programmable_button, 2) \
    X(privacy,             3) \
    X(page_tweaks,         4) \
    X(website_settings,    5) \
    X(flexible_space,      6) \
    X(profile,             7) \
    X(address_and_search,  8) \
    X(back_forward,        9) \
    X(home,               10) \
    X(start_page,         11) \
    X(notes,              12) \
    X(downloads,          13) \
    X(share,              14) \
    X(new_tab,            15) \
    X(history,            16) \
    X(sidebar,            17) \
    X(zoom,               18) \
    X(print,              19) \
    X(web_inspector,      20) \
    X(overflow_menu,      21) \
    X(extensions,         22) \
    X(toggle_reader,      23) \
    X(tab_overview,       24) \
    X(focus_mode,         25) \
    X(bookmark,           26) \
    X(kagi_privacy_pass,  27) \
    X(new_window,         28) \
    X(new_private_window, 29) \
    X(tab_groups,         30) \
// end of TOOLBAR_ITEMS list

enum class ToolbarItemType : int64_t {
#define ENTRY(name, value) name = value,
    TOOLBAR_ITEMS(ENTRY)
#undef ENTRY
};

const std::unordered_map<ToolbarItemType, std::string> global_toolbar_item_names = {
#define ENTRY(name, val) {ToolbarItemType::name, #name},
    TOOLBAR_ITEMS(ENTRY)
#undef ENTRY
};

const std::unordered_map<std::string, ToolbarItemType> global_toolbar_item_types = {
#define ENTRY(name, val) {#name, ToolbarItemType::name},
    TOOLBAR_ITEMS(ENTRY)
#undef ENTRY
};

#undef TOOLBAR_ITEMS

struct CreateToolbarItemQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(item_type, ToolbarItemType);
    // The toolbar item's ID that will follow the created item in sort order.
    // If 0, the item will be created at the end of the list.
    DTO_MEMBER(next_item_id, ToolbarItemId);
    DTO_MEMBER(is_compact_tab_layout, bool);
    DTO_MEMBER(in_overflow_menu, bool);
};

struct CreateToolbarItemReply {
    DTO_MEMBER(id, ToolbarItemId);
    DTO_MEMBER(err, Error);
};

struct ToolbarItemDto {
    DTO_MEMBER(id, ToolbarItemId);
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(item_type, ToolbarItemType);
    // The global position of the item in the whole list of items (including both toolbar and overflow menu items)
    DTO_MEMBER(position, int64_t);
    // The position of the item in the toolbar or overflow menu (depending on the value of in_overflow_menu)
    DTO_MEMBER(category_position, int64_t);
    DTO_MEMBER(is_compact_tab_layout, bool);
    DTO_MEMBER(in_overflow_menu, bool);
};

// Position cannot be updated directly, use MoveToolbarItemQuery instead
struct UpdateToolbarItemQuery {
    DTO_MEMBER(id, ToolbarItemId);

    //intentionally commented out to massively reduce complexity for programmable button profile id handling
    // DTO_MEMBER(profile_id, ProfileId);
    // DTO_MEMBER(update_profile_id, bool, false);

    DTO_MEMBER(item_type, ToolbarItemType);
    DTO_MEMBER(update_item_type, bool, false);
    
    DTO_MEMBER(in_overflow_menu, bool);
    DTO_MEMBER(update_in_overflow_menu, bool, false);

    // is_compact_tab_layout cannot be updated intentionally
};

struct GetToolbarItemsQuery {
    // If non-zero, only items belonging to the specified profile will be returned
    DTO_MEMBER(profile_id, ProfileId);
    // If set, only items with the specified IDs will be returned
    DTO_MEMBER(ids, std::optional<std::vector<ToolbarItemId>>);
    // If true, items that are in the overflow menu will be excluded from the results
    DTO_MEMBER(exclude_in_overflow_menu, bool);
    // If true, items that are in the toolbar will be excluded from the results
    DTO_MEMBER(exclude_in_toolbar, bool);
    DTO_MEMBER(is_compact_tab_layout, bool);
};

struct GetToolbarItemsReply {
    DTO_MEMBER(items, std::vector<ToolbarItemDto>);
    DTO_MEMBER(err, Error);
};

struct GetDefaultToolbarItemsQuery {
    DTO_MEMBER(is_compact_tab_layout, bool);
    DTO_MEMBER(in_toolbar, bool);
    DTO_MEMBER(in_overflow_menu, bool);
};

struct GetDefaultToolbarItemsReply {
    DTO_MEMBER(items, std::vector<ToolbarItemDto>);
    DTO_MEMBER(err, Error);
};

struct GetAvailableToolbarItemsQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(is_compact_tab_layout, bool);
};

struct GetAvailableToolbarItemsReply {
    DTO_MEMBER(items, std::vector<ToolbarItemDto>);
    DTO_MEMBER(err, Error);
};

struct MoveToolbarItemQuery {
    DTO_MEMBER(id, ToolbarItemId);
    // The toolbar item's ID that will follow the moved item in sort order.
    // If 0, the item will be moved to the end of the list.
    DTO_MEMBER(next_item_id, ToolbarItemId);
};

struct ToolbarItemsChange {
    DTO_MEMBER(created, std::vector<ToolbarItemDto>);
    DTO_MEMBER(updated, std::vector<ToolbarItemDto>);
    DTO_MEMBER(deleted, std::vector<ToolbarItemDto>);
};

struct ResetToolbarItemsQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(is_compact_tab_layout, bool);
    // If true, items in main toolbar will be reset
    DTO_MEMBER(in_toolbar, bool);
    // If true, items in overflow menu will be reset
    DTO_MEMBER(in_overflow_menu, bool);
};

} // namespace orion