#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <common/src/dto/dto_member.h>
#include <common/src/strong_types.h>
#include <common/src/error.h>

namespace orion {

DECLARE_STRONG_TYPEDEF(ProfileId, int64_t);
DECLARE_STRONG_TYPEDEF(WindowId, int64_t);

struct ProfileDto {
    DTO_MEMBER(id, ProfileId, ProfileId(0));
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_name, std::string); // do not set a default icon name here
    DTO_MEMBER(show_in_address_bar, bool, true);
    DTO_MEMBER(show_badge_in_app_icon, bool, true);
    DTO_MEMBER(keyboard_shortcut, std::string);
    DTO_MEMBER(seq_num, double, 0.0);
    DTO_MEMBER(is_internal, bool, false); // internal profile cannot be deleted, it is the initial profile created by the app
    DTO_MEMBER(is_default, bool, false);  // there is always exactly one default profile
    DTO_MEMBER(selected_window_id, WindowId, WindowId(0)); // the last focused window in this profile
    DTO_MEMBER(shared_pinned_tabs_window_id, WindowId, WindowId(0)); // fake window holding pinned tabs shared between unsaved windows
    DTO_MEMBER(is_test, bool);
};

struct GetProfileIdsReply {
    DTO_MEMBER(ids, std::vector<ProfileId>);
    DTO_MEMBER(err, Error);
};

struct GetDefaultProfileIdReply {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(err, Error);
};

struct GetInternalProfileIdReply {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(err, Error);
};

struct GetProfilesReply {
    DTO_MEMBER(profiles, std::vector<ProfileDto>);
    DTO_MEMBER(err, Error);
};

struct CreateProfileQuery {
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(keyboard_shortcut, std::string);
    DTO_MEMBER(is_test, bool);
};

struct CreateProfileReply {
    DTO_MEMBER(id, ProfileId, ProfileId(0));
    DTO_MEMBER(err, Error);
};

struct UpdateProfileQuery {
    DTO_MEMBER(id, ProfileId, ProfileId(0));
    
    DTO_MEMBER(name, std::string);
    DTO_MEMBER(icon_name, std::string);
    DTO_MEMBER(show_in_address_bar, bool, true);
    DTO_MEMBER(show_badge_in_app_icon, bool, true);
    DTO_MEMBER(keyboard_shortcut, std::string);
    DTO_MEMBER(seq_num, double, 0.0);

    DTO_MEMBER(update_name, bool, false);
    DTO_MEMBER(update_icon_name, bool, false);
    DTO_MEMBER(update_show_in_address_bar, bool, false);
    DTO_MEMBER(update_show_badge_in_app_icon, bool, false);
    DTO_MEMBER(update_keyboard_shortcut, bool, false);
    DTO_MEMBER(update_seq_num, bool, false);
};

struct ProfilesChange {
    DTO_MEMBER(created, std::vector<ProfileDto>);
    DTO_MEMBER(updated, std::vector<ProfileDto>);
    DTO_MEMBER(deleted, std::vector<ProfileId>);
};

struct MoveProfileQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    // The profile that will follow the moved profile in sort order.
    // If 0, the profile is moved to the end of the list.
    DTO_MEMBER(next_profile_id, ProfileId, ProfileId(0));
};

struct DeleteProfilesQuery {
    DTO_MEMBER(ids, std::vector<ProfileId>);

    // If not nullptr, then foreach profile id, this function will be called. This is useful when the caller
    // needs to delete file system profile data (eg., cache, cookies, etc.) for each profile. This function
    // will be dispatched on disk thread before the each profile is deleted from the ProfilesDb.
    DTO_MEMBER(delete_file_system_data_fn, std::function<Error(ProfileId)>);
};

} // namespace orion