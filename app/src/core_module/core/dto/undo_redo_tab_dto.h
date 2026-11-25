#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>

#include <common/src/dto/dto_member.h>
#include <common/src/error.h>
#include <core/dto/profile_dto.h>
#include <core/dto/window_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(UndoRedoTabId, int64_t);
DECLARE_STRONG_TYPEDEF(UndoRedoTabBackForwardListEntryId, int64_t);

//-------------------------------------------------------------------------
// UndoRedoTabStep
//-------------------------------------------------------------------------

struct UndoRedoTabStepDto {
    DTO_MEMBER(id, UndoRedoTabStepId);
    DTO_MEMBER(window_id, WindowId);
    DTO_MEMBER(seq_num, int64_t);
};

struct CreateUndoRedoTabStepQuery {
    DTO_MEMBER(window_id, WindowId);
    DTO_MEMBER(tab_ids, std::vector<TabId>);
};

struct CreateUndoRedoTabStepReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, UndoRedoTabStepId);
};

struct IsUndoRedoTabStepAvailableQuery {
    DTO_MEMBER(window_id, WindowId);
};

struct IsUndoRedoTabStepAvailableReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(is_undo_available, bool);
    DTO_MEMBER(is_redo_available, bool);
};

struct ChangeUndoRedoTabStepQuery {
    DTO_MEMBER(window_id, WindowId);
    DTO_MEMBER(is_undo, bool);
};

struct ChangeUndoRedoTabStepReply {
    DTO_MEMBER(err, Error);
};

//-------------------------------------------------------------------------
// UndoRedoTab
//-------------------------------------------------------------------------

struct UndoRedoTabDto {
    DTO_MEMBER(id, UndoRedoTabId);
    DTO_MEMBER(undo_redo_step_id, UndoRedoTabStepId);
    DTO_MEMBER(original_tab_id, TabId);
    DTO_MEMBER(original_parent_tab_id, TabId);
    DTO_MEMBER(original_seq_num, double);
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(uri, std::string);
    DTO_MEMBER(is_pinned, bool, false);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(session_state, std::vector<uint8_t>);
    DTO_MEMBER(back_forward_list_index, int64_t, 0);         // Orion back-forward list index
    DTO_MEMBER(webkit_back_forward_list_index, int64_t, 0);  // WebKit back-forward list index
    DTO_MEMBER(merged_back_forward_list_length, int64_t, 0); // length of the merged back-forward list (Orion + WebKit)
};

struct CreateUndoRedoTabQuery {
    DTO_MEMBER(undo_redo_step_id, UndoRedoTabStepId);
    DTO_MEMBER(original_tab_id, TabId);
    DTO_MEMBER(original_parent_tab_id, TabId);
    DTO_MEMBER(original_seq_num, double);
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(uri, std::string);
    DTO_MEMBER(is_pinned, bool, false);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(session_state, std::vector<uint8_t>);
    DTO_MEMBER(back_forward_list_index, int64_t, 0);         // Orion back-forward list index
    DTO_MEMBER(webkit_back_forward_list_index, int64_t, 0);  // WebKit back-forward list index
    DTO_MEMBER(merged_back_forward_list_length, int64_t, 0); // length of the merged back-forward list (Orion + WebKit)
};

struct CreateUndoRedoTabReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, UndoRedoTabId);
};

struct UpdateUndoRedoTabQuery {
    DTO_MEMBER(id, UndoRedoTabId);

    DTO_MEMBER(original_tab_id, TabId);

    DTO_MEMBER(update_original_tab_id, bool, false);
};

//-------------------------------------------------------------------------
// UndoRedoTabBackForwardListEntry
//-------------------------------------------------------------------------

struct UndoRedoTabBackForwardListEntryDto {
    DTO_MEMBER(id, UndoRedoTabBackForwardListEntryId);
    DTO_MEMBER(undo_redo_tab_id, UndoRedoTabId);
    DTO_MEMBER(webkit_index, int64_t);
    DTO_MEMBER(uri, std::string);
};

struct CreateUndoRedoTabBackForwardListEntryQuery {
    DTO_MEMBER(undo_redo_tab_id, UndoRedoTabId);
    DTO_MEMBER(webkit_index, int64_t);
    DTO_MEMBER(uri, std::string);
};

struct CreateUndoRedoTabBackForwardListEntryReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(id, UndoRedoTabBackForwardListEntryId);
};

} // namespace orion