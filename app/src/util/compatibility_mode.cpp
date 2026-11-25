#include "app.h"

#include "compatibility_mode.h"
#include <core/settings.h>

namespace orion {

void
set_enable_compatibility_mode(MainWindow* win, std::string website, bool new_state)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    global_core_api->set_setting_raw(global_core_api->create_set_setting_query_for_website_bool({
        .profile_id = profile_id,
        .key_name = orion::settings::website::specific::enable_content_blockers.key_name(),
        .value = !new_state,
        .default_value = true,
        .website = website,
    }), [](orion::SetSettingReply<std::string> reply) {
        VERIFY_LOG(reply.err);
    });

    global_core_api->set_setting_raw(global_core_api->create_set_setting_query_for_website_bool({
        .profile_id = profile_id,
        .key_name = orion::settings::website::specific::enable_intelligent_tracking_prevention.key_name(),
        .value = !new_state,
        .default_value = true,
        .website = website,
    }), [](orion::SetSettingReply<std::string> reply) {
        VERIFY_LOG(reply.err);
    });

    global_core_api->set_setting_raw(global_core_api->create_set_setting_query_for_website_bool({
        .profile_id = profile_id,
        .key_name = orion::settings::website::specific::block_web_fonts.key_name(),
        .value = false, // always set to false when compatibility mode is changed
        .default_value = true,
        .website = website,
    }), [](orion::SetSettingReply<std::string> reply) {
        VERIFY_LOG(reply.err);
    });

    global_core_api->set_setting_raw(global_core_api->create_set_setting_query_for_website_bool({
        .profile_id = profile_id,
        .key_name = orion::settings::website::specific::enable_cookies.key_name(),
        .value = true, // always set to true when compatibility mode is changed
        .default_value = true,
        .website = website,
    }), [](orion::SetSettingReply<std::string> reply) {
        VERIFY_LOG(reply.err);
    });

    global_core_api->set_setting_raw(global_core_api->create_set_setting_query_for_website_bool({
        .profile_id = profile_id,
        .key_name = orion::settings::website::specific::enable_reader_mode.key_name(),
        .value = false, // always set to false when compatibility mode is changed
        .default_value = true,
        .website = website,
    }), [](orion::SetSettingReply<std::string> reply) {
        VERIFY_LOG(reply.err);
    });

    global_core_api->set_setting_raw(global_core_api->create_set_setting_query_for_website_bool({
        .profile_id = profile_id,
        .key_name = orion::settings::website::specific::enable_page_tweaks.key_name(),
        .value = !new_state,
        .default_value = true,
        .website = website,
    }), [](orion::SetSettingReply<std::string> reply) {
        VERIFY_LOG(reply.err);
    });

    guint user_agent_idx = static_cast<guint>(new_state
        ? orion::settings::UserAgentWebsiteSetting::firefox
        : orion::settings::UserAgentWebsiteSetting::automatic)
        - 1;

    global_core_api->set_setting_raw(global_core_api->create_set_setting_query_for_website_integer({
        .profile_id = profile_id,
        .key_name = orion::settings::website::specific::user_agent.key_name(),
        .value = user_agent_idx,
        .default_value = static_cast<int64_t>(orion::settings::UserAgentWebsiteSetting::automatic) - 1,
        .website = website,
    }), [](orion::SetSettingReply<std::string> reply) {
        VERIFY_LOG(reply.err);
    });

    main_window_update_enable_compatibility_mode_action_state(win, website);
}

} // namespace orion