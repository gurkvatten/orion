#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>

#include <common/src/dto/dto_member.h>
#include <common/src/error.h>
#include <core/dto/profile_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(UiWindowId, void*);
DECLARE_STRONG_TYPEDEF(TabId, int64_t);
DECLARE_STRONG_TYPEDEF(UndoRedoTabStepId, int64_t);

// Represents a saved window snapshot or a tab group, not UI window
struct WindowDto {
    DTO_MEMBER(id, WindowId, WindowId(0));
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(keyboard_shortcut, std::string);
    DTO_MEMBER(is_suspended, bool, false);
    DTO_MEMBER(seq_num, double, 0.0);

    // If true then this is a group of tabs that is bound to a specific UI window - when
    // a UI window is closed, all unpinned tabs in its associated tab group are closed
    // (deleted). Unpinned tabs in a tab group probably do not need to persisted, they
    // are ephemeral (transient), but pinned tabs bound to a tab group must be persisted.
    // Each UI window is associated with exactly one tab group.
    DTO_MEMBER(is_tab_group, bool, false);

    // UI window handle (not persistent). This is probably useful just for a tab group.
    DTO_MEMBER(ui_window_id, UiWindowId, UiWindowId(nullptr)); // does not work yet

    // The tab that has been most recently selected in this window
    DTO_MEMBER(selected_tab_id, TabId, TabId(0));

    DTO_MEMBER(num_tabs, int64_t, 0); // number of tabs in this window

    // True if there is a UI window associated with this Saved/Unsaved Tab Group. This is
    // persisted in the DB and is useful when restoring a session or saving a workspace
    // snapshot.
    
    DTO_MEMBER(has_ui_window, bool, false);
    DTO_MEMBER(undo_redo_tab_step_seq_num, int64_t, 0);

    // The second most recently selected tab in this window
    DTO_MEMBER(prev_selected_tab_id, TabId);

    DTO_MEMBER(is_focus_mode, bool, false);
    DTO_MEMBER(is_private, bool, false);
};

struct GetWindowIdsQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(include_private_windows, bool, true);
};

struct GetWindowIdsReply {
    DTO_MEMBER(ids, std::vector<WindowId>);
    DTO_MEMBER(err, Error);
};

struct GetWindowsReply {
    DTO_MEMBER(windows, std::vector<WindowDto>);
    DTO_MEMBER(err, Error);
};

struct CreateWindowQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(name, std::string);
    // If set to non-empty Tab Group name (eg., "Tab Group"), then name field is ignored and predefined template
    // is used instead resulting in a name like "Tab Group 1", "Tab Group 2", etc. This is useful when creating
    // a new Saved Tab Group.
    DTO_MEMBER(template_name, std::string);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(keyboard_shortcut, std::string);
    DTO_MEMBER(is_suspended, bool, false);
    DTO_MEMBER(seq_num, std::optional<double>, std::nullopt); // if not set, then append at the end
    DTO_MEMBER(is_tab_group, bool, false);
    DTO_MEMBER(ui_window_id, UiWindowId, UiWindowId(nullptr)); // does not work yet
    DTO_MEMBER(selected_tab_id, TabId, TabId(0));
    DTO_MEMBER(has_ui_window, bool, false);
    DTO_MEMBER(undo_redo_tab_step_seq_num, int64_t, 0);
    DTO_MEMBER(is_focus_mode, bool, false);
    DTO_MEMBER(is_private, bool, false);
};

struct CreateWindowReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, WindowId, WindowId(0));
};

enum class SeqNumCollisionMode {
    after  = 0,
    before = 1,
};

struct UpdateWindowQuery {
    DTO_MEMBER(id, WindowId, WindowId(0));

    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(keyboard_shortcut, std::string);
    DTO_MEMBER(ui_window_id, UiWindowId, UiWindowId(nullptr)); // does not work yet
    DTO_MEMBER(selected_tab_id, TabId, TabId(0));
    DTO_MEMBER(is_tab_group, bool, false);
    DTO_MEMBER(is_suspended, bool, false);
    DTO_MEMBER(seq_num, double, 0.0);
    DTO_MEMBER(seq_num_collision_mode, SeqNumCollisionMode, SeqNumCollisionMode::after);
    DTO_MEMBER(has_ui_window, bool, false);
    DTO_MEMBER(undo_redo_tab_step_seq_num, int64_t, 0);
    DTO_MEMBER(is_focus_mode, bool);
    DTO_MEMBER(is_private, bool);

    DTO_MEMBER(update_name, bool, false);
    DTO_MEMBER(update_icon_name, bool, false);
    DTO_MEMBER(update_keyboard_shortcut, bool, false);
    DTO_MEMBER(update_ui_window_id, bool, false);
    DTO_MEMBER(update_selected_tab_id, bool, false);
    DTO_MEMBER(update_is_tab_group, bool, false);
    DTO_MEMBER(update_is_suspended, bool, false);
    DTO_MEMBER(update_seq_num, bool, false);
    DTO_MEMBER(update_has_ui_window, bool, false);
    DTO_MEMBER(update_undo_redo_tab_step_seq_num, bool, false);
    DTO_MEMBER(update_is_focus_mode, bool, false);
    DTO_MEMBER(update_is_private, bool, false);
};

struct SaveWindowQuery {
    DTO_MEMBER(window_id, WindowId, WindowId(0));
};

struct DeletedWindowDto {
    DTO_MEMBER(id, WindowId);
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(is_private, bool);
};

//if this gets modified, any places that hold a temporary buffer corresponding to a transaction should be updated
//currently known places:
//core_module/core/profiles_db/window.cpp:save_window(...)
struct WindowsChange {
    DTO_MEMBER(created, std::vector<WindowDto>);
    DTO_MEMBER(updated, std::vector<WindowDto>);
    DTO_MEMBER(deleted, std::vector<DeletedWindowDto>);

    DTO_MEMBER(updated_name, bool, false);
    DTO_MEMBER(updated_icon_name, bool, false);
    DTO_MEMBER(updated_keyboard_shortcut, bool, false);
    DTO_MEMBER(updated_ui_window_id, bool, false);
    DTO_MEMBER(updated_selected_tab_id, bool, false);
    DTO_MEMBER(updated_is_tab_group, bool, false);
    DTO_MEMBER(updated_is_suspended, bool, false);
    DTO_MEMBER(updated_seq_num, bool, false);
    DTO_MEMBER(updated_has_ui_window, bool, false);
    DTO_MEMBER(updated_undo_redo_tab_step_seq_num, bool, false);
    DTO_MEMBER(updated_is_focus_mode, bool, false);
    DTO_MEMBER(updated_is_private, bool, false);
};

struct SetSelectedWindowIdQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(window_id, WindowId, WindowId(0));
};

struct GetSelectedWindowIdReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(window_id, WindowId, WindowId(0));
    DTO_MEMBER(window_exists, bool, false);
    DTO_MEMBER(is_private, bool);
};

struct GetSharedPinnedTabsWindowIdReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(window_id, WindowId, WindowId(0));
};

struct MoveWindowQuery {
    DTO_MEMBER(window_id, WindowId, WindowId(0));
    // The window that will follow the moved window in sort order.
    // If 0, the window is moved to the end of the list.
    DTO_MEMBER(next_window_id, WindowId, WindowId(0));
};

struct DuplicateWindowQuery {
    DTO_MEMBER(window_id, WindowId, WindowId(0));

    // The name of the duplicated window. Its should be empty if the source window is not private.
    // Otherwise, it should be set to a translated name like "Copy of %s" where %s is the name of the source window.
    DTO_MEMBER(name, std::string);
};

struct DuplicateWindowReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(new_window_id, WindowId, WindowId(0));
};

struct UiWindowsChange {
    DTO_MEMBER(created, std::vector<UiWindowId>);
    DTO_MEMBER(deleted, std::vector<UiWindowId>);
};

} // namespace orion
