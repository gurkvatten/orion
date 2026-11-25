#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>

#include <rfl.hpp>

#include <common/src/dto/dto_member.h>
#include <common/src/error.h>
#include <common/src/enum_reflect.h>

#include <core/dto/toolbar_item_dto.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(ProgrammableButtonId, int64_t);
enum class ProgrammableButtonActionType {
    not_set                             = 0,
    menu_action                         = 1,
    open_uri_in_current_tab             = 2,
    open_uri_in_new_tab                 = 3,
    open_uri_in_link_preview            = 4,
    open_uri_in_background              = 5,
    run_javascript_code                 = 6,
    run_javascript_code_open_sidebar    = 7,
};

struct ProgrammableButtonDto {
    DTO_MEMBER(id, rfl::Skip<ProgrammableButtonId>, ProgrammableButtonId(0));
    DTO_MEMBER(profile_id, rfl::Skip<ProfileId>, ProfileId(0));
    DTO_MEMBER(title, std::string, ""); //since we cannot really handle localisation in our core module, I recommend handling default title in a singleton observer, changes are uncommon anyway making for fairly low performance load
    DTO_MEMBER(icon_string, std::string, "");
    DTO_MEMBER(action_type, ProgrammableButtonActionType, );
    DTO_MEMBER(action_string, std::string, "");
    DTO_MEMBER(keyboard_shortcut, std::string, "");
};

//
struct GetProgrammableButtonsQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(ids, std::optional<std::vector<ProgrammableButtonId>>);
};

struct GetProgrammableButtonsReply {
    DTO_MEMBER(items, std::vector<ProgrammableButtonDto>);
    DTO_MEMBER(err, Error);
};

struct CreateProgrammableButtonQuery {
    DTO_MEMBER(id, ProgrammableButtonId, ProgrammableButtonId(0));
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(title, std::string, "");
    DTO_MEMBER(icon_string, std::string, "");
    DTO_MEMBER(action_type, ProgrammableButtonActionType, );
    DTO_MEMBER(action_string, std::string, "");
    DTO_MEMBER(keyboard_shortcut, std::string, "");

    DTO_MEMBER(next_item_id, ToolbarItemId); // used only in CoreApi calls, ignored in ProfilesDb calls
    DTO_MEMBER(in_overflow_menu, bool); // used only in CoreApi calls, ignored in ProfilesDb calls
};

struct UpdateProgrammableButtonQuery {
    DTO_MEMBER(id, ProgrammableButtonId);

    //profile_id should never be updated, otherwise will cause a lot of logic breakage
    DTO_MEMBER(title, std::string, "");
    DTO_MEMBER(update_title, bool, false);

    DTO_MEMBER(icon_string, std::string, "");
    DTO_MEMBER(update_icon_string, bool, false);

    DTO_MEMBER(action_type, ProgrammableButtonActionType, );
    DTO_MEMBER(update_action_type, bool, false);

    DTO_MEMBER(action_string, std::string, "");
    DTO_MEMBER(update_action_string, bool, false);

    DTO_MEMBER(keyboard_shortcut, std::string, "");
    DTO_MEMBER(update_keyboard_shortcut, bool, false);
};

struct ProgrammableButtonsChange {
    DTO_MEMBER(created, std::vector<ProgrammableButtonDto>);
    DTO_MEMBER(updated, std::vector<ProgrammableButtonDto>);
    DTO_MEMBER(deleted, std::vector<ProgrammableButtonId>);
};

constexpr ProgrammableButtonId PROGRAMMABLE_BUTTON_ID(const ToolbarItemId& id)
{
    return ProgrammableButtonId(id.value);
};
constexpr ToolbarItemId TOOLBAR_ITEM_ID(const ProgrammableButtonId& id)
{
    return ToolbarItemId(id.value);
};

} // namespace orion
