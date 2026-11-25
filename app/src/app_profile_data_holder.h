#pragma once

#include <map>
#include <string>
#include <vector>

#include <core/settings.h>

#include "data_item/style_sheet_list.h"

#include "util/uri_util.h"
#include "util/settings_util.h"

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

namespace orion {

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
using WebKitUserStyleSheetMap = std::map<std::string, WebKitUserStyleSheet*, std::less<>>;
template <typename T> using TypeToWebKitUserStyleSheetMap = std::map<T, std::pair<orion::DomainFilter, WebKitUserStyleSheet*>, std::less<>>;
#endif

}

//implements GActionGroup which exposes settings 
#define APP_TYPE_PROFILE_DATA_HOLDER (profile_data_holder_get_type())

G_DECLARE_FINAL_TYPE(ProfileDataHolder, profile_data_holder, APP, PROFILE_DATA_HOLDER, GObject);

ProfileDataHolder* profile_data_holder_new(orion::ProfileId profile_id);

//PageTweaks
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
const orion::DomainFilter& profile_data_holder_get_page_tweaks_disabled_filter(ProfileDataHolder* self);
const orion::TypeToWebKitUserStyleSheetMap<orion::settings::PageZoom>& profile_data_holder_get_page_tweaks_font_size_map(ProfileDataHolder* self);
const orion::DomainFilter& profile_data_holder_get_page_tweaks_force_dark_mode_filter(ProfileDataHolder* self);
const orion::TypeToWebKitUserStyleSheetMap<std::string>& profile_data_holder_get_page_tweaks_font_family_map(ProfileDataHolder* self);
const orion::DomainFilter& profile_data_holder_get_page_tweaks_sticky_disabled_filter(ProfileDataHolder* self);
const orion::WebKitUserStyleSheetMap& profile_data_holder_get_page_tweaks_content_blockers_map(ProfileDataHolder* self);
#endif

//settings
std::string profile_data_holder_get_setting_raw_val_by_db_key(ProfileDataHolder* self, std::string key_name);
std::string profile_data_holder_get_setting_raw_val_by_action_name(ProfileDataHolder* self, std::string action_name);

template <typename Tag>
inline Tag::value_type profile_data_holder_get_setting_val(ProfileDataHolder* self, const orion::settings::Setting<Tag>& setting)
{
    std::string action_name = orion::gtk::setting_db_key_to_action_name(setting.key());
    std::string raw_val = profile_data_holder_get_setting_raw_val_by_action_name(self, action_name);

    typename Tag::value_type val = {};
    if(orion::Error err = orion::settings::unmarshal_value(raw_val, &val))
    {
        LOG_ERROR(err);
    }

    return val;
}


#ifndef NDEBUG
void profile_data_holder_debug_print(ProfileDataHolder* self);
#endif
