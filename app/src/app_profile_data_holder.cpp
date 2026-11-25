#define ENABLE_VERBOSE_LOGS 0

#include "app.h"

#include "app_profile_data_holder.h"

#include <gtk/gtk.h>

#include <core/settings.h>

#include "util/css_util.h"
#include "util/settings_util.h"
#include "util/action_util.h"
#include "util/variant_util.h"

#pragma region Declarations

namespace {
    struct CppWrapper {
        orion::ProfileId profile_id;
        orion::ObserverId settings_observer;



#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        //Page Tweaks
        orion::DomainFilter page_tweaks_disable_filter;

        orion::TypeToWebKitUserStyleSheetMap<orion::settings::PageZoom>* page_tweaks_font_size_map;
        orion::DomainFilter page_tweaks_force_dark_mode_filter;
        orion::TypeToWebKitUserStyleSheetMap<std::string>* page_tweaks_font_family_map;
        orion::DomainFilter page_tweaks_disable_sticky_filter;
        orion::WebKitUserStyleSheetMap* page_tweaks_element_blockers_map;
#endif
        std::map<std::string, std::string, std::less<>> settings_map;
    };
}



struct _ProfileDataHolder
{
	GObject parent;

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitUserStyleSheet* page_tweaks_dark_mode_style_sheet;
#endif
    CppWrapper wrapper;
};


static void profile_data_holder_action_group_interface_init(GActionGroupInterface* iface);

G_DEFINE_TYPE_WITH_CODE(ProfileDataHolder, profile_data_holder, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(G_TYPE_ACTION_GROUP, profile_data_holder_action_group_interface_init)
)

enum {
    PROP_0,
    PROP_PROFILE_ID,
    PROP_PAGE_TWEAKS_FONT_SIZE_STYLE_SHEETS,
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    PROP_PAGE_TWEAKS_DARK_MODE_STYLE_SHEET,
#endif
    PROP_PAGE_TWEAKS_DARK_MODE_FILTER,
    PROP_PAGE_TWEAKS_DISABLE_STICKY_FILTER,
    PROP_PAGE_TWEAKS_FONT_FAMILY_STYLE_SHEETS,
    PROP_PAGE_TWEAKS_ELEMENT_BLOCKERS,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS] = {NULL, };

enum {
    SIG_SETTING_CHANGED,
    N_SIGNALS
};

static guint obj_signals[N_SIGNALS] = {};

#define FORWARDED_REALMS(_operation) \
    _operation(orion::settings::general) \
    _operation(orion::settings::appearance) \
    _operation(orion::settings::tabs) \
    _operation(orion::settings::browsing) \
    _operation(orion::settings::sync) \
    _operation(orion::settings::passwords) \
    _operation(orion::settings::privacy) \
    _operation(orion::settings::search) \
    _operation(orion::settings::websites) \
    _operation(orion::settings::dialogs::add_bookmark) \
    _operation(orion::settings::advanced) \
    _operation(orion::settings::reset_orion) \
    _operation(orion::settings::filter_lists) \
    _operation(orion::settings::windows) \
    _operation(orion::settings::reader_mode) \
    _operation(orion::settings::test) \
    _operation(orion::settings::test::subrealm) \
    _operation(orion::settings::common::onboarding)


const std::vector<const orion::settings::ISetting*>& get_forwarded_settings_array()
{
    static std::vector<const orion::settings::ISetting*> result;

    if(result.empty())
    {
#define APPEND_REALM(__realm) \
        for(const auto* setting : __realm::all()) \
        { \
            result.push_back(setting); \
        }

        FORWARDED_REALMS(APPEND_REALM);

#undef APPEND_REALM
    }

    return result;
}

#pragma endregion Declarations




#pragma region InternalMethods

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static void
invalidate_page_tweaks_dark_mode_style_sheet(ProfileDataHolder* self)
{
    orion::log_verbose("main_window_invalidate_page_tweaks_dark_mode_style_sheet():");
    g_clear_pointer(&self->page_tweaks_dark_mode_style_sheet, webkit_user_style_sheet_unref);

    if(self->wrapper.page_tweaks_force_dark_mode_filter.has_contents())
    {
        std::vector<char*> allow_list = self->wrapper.page_tweaks_force_dark_mode_filter.get_webkit_allow_list(self->wrapper.page_tweaks_disable_filter);

        for(auto it = allow_list.cbegin(); it != allow_list.cend()-1; it++)
        {
            orion::log_verbose("%s", *it);
        }

        if(allow_list.size()>1)
        {
            self->page_tweaks_dark_mode_style_sheet = webkit_user_style_sheet_new_for_world(
                R"(
                    html {
                        background-color: #ffffff !important;
                        filter: invert(100%) hue-rotate(180deg) !important;
                    }

                    img, video, picture, canvas, iframe, svg {
                        filter: invert(100%) hue-rotate(180deg) !important;
                    }
                )",
                WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                WEBKIT_USER_STYLE_LEVEL_USER,
                PAGE_TWEAKS_SCRIPT_WORLD,
                allow_list.data(),
                NULL
            );
        }


        orion::webkit_allow_list_free(allow_list);
    }
    orion::log_verbose("done");
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PAGE_TWEAKS_DARK_MODE_STYLE_SHEET]);
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PAGE_TWEAKS_DARK_MODE_FILTER]);
}

static void
invalidate_page_tweaks_disable_sticky_filter(ProfileDataHolder* self)
{
    orion::log_verbose("main_window_invalidate_page_tweaks_disable_sticky_style_sheet():");
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PAGE_TWEAKS_DISABLE_STICKY_FILTER]);
}

static void
invalidate_page_tweaks_font_size_style_sheets(ProfileDataHolder* self, std::vector<orion::settings::PageZoom> invalidate_for)
{
    for(orion::settings::PageZoom value : invalidate_for)
    {
        orion::log_verbose("invalidating page_tweaks_font_size_style_sheets for %s, allow list:", enchantum::to_string(value).data());
        //allowing for default construction of map's value for our PageZoom key if it doesn't exist yet
        //probably unnecessary, caller should always have constructed this before us
        if(self->wrapper.page_tweaks_font_size_map->contains(value))
        {
            auto& pair = self->wrapper.page_tweaks_font_size_map->at(value);
            g_clear_pointer(&pair.second, webkit_user_style_sheet_unref);

            if(pair.first.has_contents())
            {
                std::vector<gchar*> allow_list = pair.first.get_webkit_allow_list(self->wrapper.page_tweaks_disable_filter);

                for(auto it = allow_list.cbegin(); it != allow_list.end()-1; ++it)
                {
                    orion::log_verbose("%s", *it);
                }

                if(allow_list.size() > 1)
                {
                    pair.second = webkit_user_style_sheet_new_for_world(
                        orion::page_tweaks_font_size_css_from_font_size_setting(value).data(),
                        WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                        WEBKIT_USER_STYLE_LEVEL_USER,
                        PAGE_TWEAKS_SCRIPT_WORLD,
                        allow_list.data(),
                        NULL
                    );
                }
                
                orion::webkit_allow_list_free(allow_list);
            }
            else
            {
                self->wrapper.page_tweaks_font_size_map->erase(value);
            }
        }

        orion::log_verbose("done");
    }
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PAGE_TWEAKS_FONT_SIZE_STYLE_SHEETS]);
}



static void
invalidate_page_tweaks_font_family_style_sheets(ProfileDataHolder* self, std::vector<std::string> invalidate_for)
{
    for(std::string& value : invalidate_for)
    {
        orion::log_verbose("invalidating page_tweaks_font_family_style_sheets for %s, allow list:", value.c_str());
        
        if(self->wrapper.page_tweaks_font_family_map->contains(value))
        {
            auto& pair = self->wrapper.page_tweaks_font_family_map->at(value);
            g_clear_pointer(&pair.second, webkit_user_style_sheet_unref);
            
            if(pair.first.has_contents())
            {
                std::vector<gchar*> allow_list = pair.first.get_webkit_allow_list(self->wrapper.page_tweaks_disable_filter);

                for(auto it = allow_list.cbegin(); it != allow_list.end()-1; ++it)
                {
                    orion::log_verbose("%s", *it);
                }

                if(allow_list.size() > 1)
                {
                    pair.second = webkit_user_style_sheet_new_for_world(
                        orion::page_tweaks_font_family_css_from_font_family_setting(value).c_str(),
                        WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                        WEBKIT_USER_STYLE_LEVEL_USER,
                        PAGE_TWEAKS_SCRIPT_WORLD,
                        allow_list.data(),
                        NULL
                    );
                }
                
                orion::webkit_allow_list_free(allow_list);
            }
            else
            {
                self->wrapper.page_tweaks_font_family_map->erase(value);
            }
        }

        orion::log_verbose("done");
    }
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PAGE_TWEAKS_FONT_FAMILY_STYLE_SHEETS]);
}
#endif

static void
bind_to_settings(ProfileDataHolder* self, orion::ProfileId profile_id)
{
    orion::gtk::WeakRefWrapper self_weak(self);

    {
        for(const auto* setting : get_forwarded_settings_array())
        {
            orion::GetSettingQuery query {
                .profile_id = profile_id,
                .key = setting->key(),
                .realm = setting->realm(),
                .key_name = setting->key_name(),
                .type = setting->type(),
                .default_value = setting->default_value_as_string()
            };

            std::string action_name = orion::gtk::setting_db_key_to_action_name(query.key);

            global_core_api->get_setting_raw(query, [self_weak, action_name](orion::GetSettingReply<std::string> reply){
                VERIFY_LOG(reply.err);

                auto self_strong = self_weak.lock();
                if(!self_strong) return;
                ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(self_strong.get());

                if(self->wrapper.settings_map.contains(action_name))
                {
                    orion::log_error("attempted to init setting but already had value for action name %s", action_name.c_str());
                    return;
                }

                self->wrapper.settings_map[action_name] = reply.value;

                g_action_group_action_added(G_ACTION_GROUP(self), action_name.c_str());
                
                g_signal_emit(self, obj_signals[SIG_SETTING_CHANGED], 0, orion::gtk::setting_action_name_to_db_key(action_name), reply.value.c_str());
            });
        }
    }

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    //NOTE: these enables can never really be thread safe, as there is a timeframe between the gets getting dispatched and actually executing
    //where these enables should already be set to true for things like a filter function
    //currently, both the observer cb and filter are guaranteed to run on ui thread, which makes this safe for both
    auto font_size_observer_enable = std::make_shared<bool>(false);
    {
        orion::GetAllValuesForWebsiteSettingQuery query{
            .profile_id = profile_id,
            .key_name = orion::settings::website::specific::page_tweaks_font_size.key_name()
        };

        global_core_api->get_all_values_for_website_setting(query, [self_weak, font_size_observer_enable](orion::GetAllValuesForWebsiteSettingReply reply){
            VERIFY_LOG(reply.err);

            *font_size_observer_enable = true;

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(self_strong.get());

            std::vector<orion::settings::PageZoom> to_invalidate{};
            
            for (const orion::RawSettingDto& setting : reply.settings) {
                std::string website;
                std::string key_name;
                if (!orion::parse_website_specific_setting_key(setting.key, &website, &key_name)) continue;
                if (key_name != orion::settings::website::specific::page_tweaks_font_size.key_name()) continue;

                orion::settings::PageZoom value = orion::settings::PageZoom::not_set;
                if(orion::Error err = orion::settings::unmarshal_value(setting.value, &value))
                {
                    LOG_ERROR(err);
                    continue;
                }

                if (value == orion::settings::PageZoom::not_set ||
                    value == orion::settings::PageZoom::zoom_100) continue; //skips default values to clutter things up less

                //presuming that this is the first place to interact with the map
                if(!(self->wrapper.page_tweaks_font_size_map->contains(value)))
                {
                    to_invalidate.push_back(value);
                }

                auto& pair = (*self->wrapper.page_tweaks_font_size_map)[value];

                pair.first.insert(website);
            }
            if(to_invalidate.size()) invalidate_page_tweaks_font_size_style_sheets(self, to_invalidate);
        });
    }
    


    auto dark_mode_observer_enable = std::make_shared<bool>(false);
    {
        orion::GetAllValuesForWebsiteSettingQuery query{
            .profile_id = profile_id,
            .key_name = orion::settings::website::specific::page_tweaks_force_dark_mode.key_name()
        };

        global_core_api->get_all_values_for_website_setting(query, [self_weak, dark_mode_observer_enable](orion::GetAllValuesForWebsiteSettingReply reply){
            VERIFY_LOG(reply.err);

            *dark_mode_observer_enable = true;

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(self_strong.get());

            bool invalidate = false;
            
            for (const orion::RawSettingDto& setting : reply.settings) {
                std::string website;
                std::string key_name;
                if (!orion::parse_website_specific_setting_key(setting.key, &website, &key_name)) continue;
                if (key_name != orion::settings::website::specific::page_tweaks_force_dark_mode.key_name()) continue;

                bool enabled = false;
                if(orion::Error err = orion::settings::unmarshal_value(setting.value, &enabled))
                {
                    LOG_ERROR(err);
                    continue;
                }

                if(enabled)
                {
                    invalidate = true;

                    self->wrapper.page_tweaks_force_dark_mode_filter.insert(website);
                }
            }
            if(invalidate) invalidate_page_tweaks_dark_mode_style_sheet(self);
        });
    }



    auto element_blocker_list_observer_enable = std::make_shared<bool>(false);
    {
        orion::GetAllValuesForWebsiteSettingQuery query{
            .profile_id = profile_id,
            .key_name = orion::settings::website::specific::page_tweaks_element_blocker_list.key_name()
        };

        global_core_api->get_all_values_for_website_setting(query, [self_weak, element_blocker_list_observer_enable](orion::GetAllValuesForWebsiteSettingReply reply){
            VERIFY_LOG(reply.err);

            *element_blocker_list_observer_enable = true;

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(self_strong.get());

            for (const orion::RawSettingDto& setting : reply.settings) {
                std::string website;
                std::string key_name;
                if (!orion::parse_website_specific_setting_key(setting.key, &website, &key_name)) continue;
                if (key_name != orion::settings::website::specific::page_tweaks_element_blocker_list.key_name()) continue;

                //fun fact: WebKitUserContentManager class documentation still lists:
                //"User style sheets can be added and removed at any time, but they will affect the web pages loaded afterwards."
                //if being pedantic about the linguistics, it is correct - but the style sheets will also affect the current page, tested empyricially, last line of
                //WebUserContentController.cpp::WebUserContentController::addUserStyleSheets() shows that this is a guarantee

                std::string exact_match = "*://" + website + "/*";
                std::string subdomain_match = "*://*." + website + "/*";
                std::vector<const gchar*> allow_list = {exact_match.c_str(), subdomain_match.c_str(), NULL};

                std::string source = orion::page_tweaks_element_blocker_css_from_selectors_setting(setting.value);
                WebKitUserStyleSheet* sheet = webkit_user_style_sheet_new_for_world(
                    source.c_str(),
                    WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, //TODO: do we perhaps want to inject all?
                    WEBKIT_USER_STYLE_LEVEL_USER,
                    PAGE_TWEAKS_SCRIPT_WORLD,
                    allow_list.data(),
                    NULL
                );
                self->wrapper.page_tweaks_element_blockers_map->emplace(website, sheet);
            }
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PAGE_TWEAKS_ELEMENT_BLOCKERS]);
        });
    }



    auto font_family_observer_enable = std::make_shared<bool>(false);
    {
        orion::GetAllValuesForWebsiteSettingQuery query{
            .profile_id = profile_id,
            .key_name = orion::settings::website::specific::page_tweaks_font_family.key_name()
        };

        global_core_api->get_all_values_for_website_setting(query, [self_weak, font_family_observer_enable](orion::GetAllValuesForWebsiteSettingReply reply){
            VERIFY_LOG(reply.err);

            *font_family_observer_enable = true;

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(self_strong.get());
            
            std::vector<std::string> to_invalidate = {};
            for (const orion::RawSettingDto& setting : reply.settings) {
                std::string website;
                std::string key_name;
                if (!orion::parse_website_specific_setting_key(setting.key, &website, &key_name)) continue;
                if (key_name != orion::settings::website::specific::page_tweaks_font_family.key_name()) continue;


                //presuming that this is the first place to interact with the map
                if(!(self->wrapper.page_tweaks_font_family_map->contains(setting.value)))
                {
                    to_invalidate.push_back(setting.value);
                }

                auto& pair = (*self->wrapper.page_tweaks_font_family_map)[setting.value];

                pair.first.insert(website);
            }
            invalidate_page_tweaks_font_family_style_sheets(self, to_invalidate);
        });
    }



    auto disable_sticky_observer_enable = std::make_shared<bool>(false);
    {
        orion::GetAllValuesForWebsiteSettingQuery query{
            .profile_id = profile_id,
            .key_name = orion::settings::website::specific::page_tweaks_disable_sticky.key_name()
        };

        global_core_api->get_all_values_for_website_setting(query, [self_weak, disable_sticky_observer_enable](orion::GetAllValuesForWebsiteSettingReply reply){
            VERIFY_LOG(reply.err);

            *disable_sticky_observer_enable = true;

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(self_strong.get());

            bool invalidate = false;
            
            for (const orion::RawSettingDto& setting : reply.settings) {
                std::string website;
                std::string key_name;
                if (!orion::parse_website_specific_setting_key(setting.key, &website, &key_name)) continue;
                if (key_name != orion::settings::website::specific::page_tweaks_disable_sticky.key_name()) continue;

                bool enabled = false;
                if(orion::Error err = orion::settings::unmarshal_value(setting.value, &enabled))
                {
                    LOG_ERROR(err);
                    continue;
                }

                if(enabled)
                {
                    invalidate = true;

                    self->wrapper.page_tweaks_disable_sticky_filter.insert(website);
                }
            }
            if(invalidate) invalidate_page_tweaks_disable_sticky_filter(self);
        });
    }



    auto enable_page_tweaks_observer_enable = std::make_shared<bool>(false);
    {
        orion::GetAllValuesForWebsiteSettingQuery query{
            .profile_id = profile_id,
            .key_name = orion::settings::website::specific::enable_page_tweaks.key_name()
        };

        global_core_api->get_all_values_for_website_setting(query, [self_weak, enable_page_tweaks_observer_enable](orion::GetAllValuesForWebsiteSettingReply reply){
            VERIFY_LOG(reply.err);

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(self_strong.get());
            
            for (const orion::RawSettingDto& setting : reply.settings) {
                std::string website;
                std::string key_name;
                if (!orion::parse_website_specific_setting_key(setting.key, &website, &key_name)) continue;
                if (key_name != orion::settings::website::specific::enable_page_tweaks.key_name()) continue;

                bool value = true;
                if(orion::Error err = orion::settings::unmarshal_value(setting.value, &value))
                {
                    LOG_ERROR(err);
                    continue;
                }

                if (value) continue;

                self->wrapper.page_tweaks_disable_filter.insert(website);
            }
            if(self->wrapper.page_tweaks_disable_filter.has_contents())
            {
                invalidate_page_tweaks_dark_mode_style_sheet(self);
                auto all_values = enchantum::values<orion::settings::PageZoom>;
                std::vector<orion::settings::PageZoom> to_invalidate(all_values.cbegin() + 1, all_values.cend());
                invalidate_page_tweaks_font_size_style_sheets(self, to_invalidate);

                g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PAGE_TWEAKS_ELEMENT_BLOCKERS]);
                g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PAGE_TWEAKS_DISABLE_STICKY_FILTER]);
            }

            *enable_page_tweaks_observer_enable = true;
        });
    }
#endif

    //single largescale observer to reduce amount of copies of change object
    self->wrapper.settings_observer = global_core_api->settings_change().add_observer(
    [   self_weak
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        ,
        enable_page_tweaks_observer_enable,
        font_size_observer_enable,
        dark_mode_observer_enable,
        font_family_observer_enable,
        disable_sticky_observer_enable,
        element_blocker_list_observer_enable
#endif
    ]
    (const orion::SettingsChange& change){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(self_strong.get());

        orion::ProfileId profile_id = self->wrapper.profile_id;

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

        bool enable_page_tweaks_observer_enabled = *enable_page_tweaks_observer_enable;
        bool page_tweaks_font_size_enable = *font_size_observer_enable;
        bool page_tweaks_dark_mode_enable = *dark_mode_observer_enable;
        bool page_tweaks_font_family_enable = *font_family_observer_enable;
        bool page_tweaks_disable_sticky_enable = *disable_sticky_observer_enable;
        bool page_tweaks_element_blockers_enabled = *element_blocker_list_observer_enable;

        std::vector<orion::settings::PageZoom> invalidate_page_tweaks_font_size_style_sheets_for{};
        bool invalidate_page_tweaks_dark_mode_list = false;
        std::vector<std::string> invalidate_page_tweaks_font_family_style_sheets_for{};
        bool invalidate_page_tweaks_disable_sticky_list = false;
        bool invalidate_page_tweaks_content_blockers = false;

#endif

        for(auto& updated : change.updated)
        {
            if(updated.profile_id != profile_id) continue;

            std::string website;

            std::string action_name = orion::gtk::setting_db_key_to_action_name(updated.key);
            if(self->wrapper.settings_map.contains(action_name))
            {
                self->wrapper.settings_map.at(action_name) = updated.new_value;
                orion::gtk::VariantWrapper var(updated.new_value);
                g_action_group_action_state_changed(G_ACTION_GROUP(self), action_name.c_str(), var.get());
                
                g_signal_emit(self, obj_signals[SIG_SETTING_CHANGED], 0, updated.key.c_str(), updated.new_value.c_str());
            }
            else if(orion::parse_website_specific_setting_key(updated.key, &website, nullptr))
            {
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
                if(enable_page_tweaks_observer_enable && updated.key_name == orion::settings::website::specific::enable_page_tweaks.key_name())
                {
                    invalidate_page_tweaks_content_blockers = true;
                    invalidate_page_tweaks_dark_mode_list = true;
                    for(auto& pair : *self->wrapper.page_tweaks_font_size_map)
                    {
                        auto invalidate_it = std::lower_bound(invalidate_page_tweaks_font_size_style_sheets_for.begin(),
                            invalidate_page_tweaks_font_size_style_sheets_for.end(), pair.first);
                        if(invalidate_it == invalidate_page_tweaks_font_size_style_sheets_for.end() || *invalidate_it != pair.first)
                        {
                            invalidate_page_tweaks_font_size_style_sheets_for.insert(invalidate_it, pair.first);
                        }
                    }
                    for(auto& pair : *self->wrapper.page_tweaks_font_family_map)
                    {
                        auto invalidate_it = std::lower_bound(invalidate_page_tweaks_font_family_style_sheets_for.begin(),
                            invalidate_page_tweaks_font_family_style_sheets_for.end(), pair.first);
                        if(invalidate_it == invalidate_page_tweaks_font_family_style_sheets_for.end() || *invalidate_it != pair.first)
                        {
                            invalidate_page_tweaks_font_family_style_sheets_for.insert(invalidate_it, pair.first);
                        }
                    }
                    invalidate_page_tweaks_disable_sticky_list = true;


                    bool new_value = true;
                    if(orion::Error err = orion::settings::unmarshal_value(updated.new_value, &new_value))
                    {
                        LOG_ERROR(std::move(err));
                        continue;
                    }

                    if(new_value)
                    {
                        self->wrapper.page_tweaks_disable_filter.remove(website);
                    }
                    else
                    {
                        self->wrapper.page_tweaks_disable_filter.insert(website);
                    }
                }
                else if(page_tweaks_element_blockers_enabled && updated.key_name == orion::settings::website::specific::page_tweaks_element_blocker_list.key_name())
                {
                    invalidate_page_tweaks_content_blockers = true;
                    orion::log_verbose("updated page tweaks element blocker setting, handling style sheet for website: %s", website.c_str());
                    if(updated.new_value.length())
                    {
                        std::string exact_match = "*://" + website + "/*";
                        std::string subdomain_match = "*://*." + website + "/*";
                        std::vector<const gchar*> allow_list = {exact_match.c_str(), subdomain_match.c_str(), nullptr};

                        std::string source = orion::page_tweaks_element_blocker_css_from_selectors_setting(updated.new_value);
                        orion::log_verbose("adding new, source: %s", source.c_str());
                        WebKitUserStyleSheet* sheet = webkit_user_style_sheet_new_for_world(
                            source.c_str(),
                            WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, //TODO: do we perhaps want to inject all?
                            WEBKIT_USER_STYLE_LEVEL_USER,
                            PAGE_TWEAKS_SCRIPT_WORLD,
                            allow_list.data(),
                            NULL
                        );

                        auto it = self->wrapper.page_tweaks_element_blockers_map->find(website);
                        if(it != self->wrapper.page_tweaks_element_blockers_map->end())
                        {
                            orion::log_verbose("removing old");
                            webkit_user_style_sheet_unref(it->second);
                            it->second = sheet;
                        }
                        else
                        {
                            self->wrapper.page_tweaks_element_blockers_map->emplace(website, sheet);
                        }
                    }
                    else
                    {
                        orion::log_verbose("removing old");
                        auto it = self->wrapper.page_tweaks_element_blockers_map->find(website);
                        if(it != self->wrapper.page_tweaks_element_blockers_map->end())
                        {
                            webkit_user_style_sheet_unref(it->second);
                            self->wrapper.page_tweaks_element_blockers_map->erase(it);
                        }
                    }
                }
                else if(page_tweaks_dark_mode_enable && updated.key_name == orion::settings::website::specific::page_tweaks_force_dark_mode.key_name())
                {
                    bool enabled = false;
                    if(orion::Error err = orion::settings::unmarshal_value(updated.new_value, &enabled))
                    {
                        LOG_ERROR(std::move(err));
                        continue;
                    }

                    orion::log_verbose("ProfileDataHolder settings observer: page tweaks dark mode %s for website %s", enabled?"enabled":"disabled", website.c_str());

                    if(enabled)
                    {
                        invalidate_page_tweaks_dark_mode_list |= self->wrapper.page_tweaks_force_dark_mode_filter.insert(website);
                    }
                    else
                    {
                        invalidate_page_tweaks_dark_mode_list |= self->wrapper.page_tweaks_force_dark_mode_filter.remove(website);
                    }
                }
                else if(page_tweaks_font_size_enable && updated.key_name == orion::settings::website::specific::page_tweaks_font_size.key_name())
                {
                    orion::settings::PageZoom new_value = orion::settings::PageZoom::not_set;
                    if(orion::Error err = orion::settings::unmarshal_value(updated.new_value, &new_value))
                    {
                        LOG_ERROR(std::move(err));
                        continue;
                    }

                    orion::settings::PageZoom old_value = orion::settings::PageZoom::not_set;
                    if(orion::Error err = orion::settings::unmarshal_value(updated.old_value, &old_value))
                    {
                        LOG_ERROR(std::move(err));
                        continue;
                    }

                    if(new_value == old_value)
                    {
                        continue;
                    }

                    if(self->wrapper.page_tweaks_font_size_map->contains(old_value))
                    {
                        auto& value_pair = self->wrapper.page_tweaks_font_size_map->at(old_value);

                        if(value_pair.first.remove(website))
                        {
                            auto invalidate_it = std::lower_bound(invalidate_page_tweaks_font_size_style_sheets_for.begin(),
                                invalidate_page_tweaks_font_size_style_sheets_for.end(), old_value);
                            if(invalidate_it == invalidate_page_tweaks_font_size_style_sheets_for.end())
                            {
                                invalidate_page_tweaks_font_size_style_sheets_for.insert(invalidate_it, old_value);
                            }
                        }
                    }

                    if (new_value == orion::settings::PageZoom::not_set ||
                        new_value == orion::settings::PageZoom::zoom_100)
                    {
                        continue;
                    }

                    auto& value_pair = (*self->wrapper.page_tweaks_font_size_map)[new_value];
                    
                    if(value_pair.first.insert(website))
                    {
                        auto invalidate_it = std::lower_bound(invalidate_page_tweaks_font_size_style_sheets_for.begin(),
                            invalidate_page_tweaks_font_size_style_sheets_for.end(), new_value);
                        if(invalidate_it == invalidate_page_tweaks_font_size_style_sheets_for.end() || *invalidate_it != new_value)
                        {
                            invalidate_page_tweaks_font_size_style_sheets_for.insert(invalidate_it, new_value);
                        }
                    }
                }
                else if(page_tweaks_font_family_enable && updated.key_name == orion::settings::website::specific::page_tweaks_font_family.key_name())
                {
                    if(updated.new_value == updated.old_value)
                    {
                        continue;
                    }

                    if(self->wrapper.page_tweaks_font_family_map->contains(updated.old_value))
                    {
                        auto& value_pair = self->wrapper.page_tweaks_font_family_map->at(updated.old_value);

                        if(value_pair.first.remove(website))
                        {
                            auto invalidate_it = std::lower_bound(invalidate_page_tweaks_font_family_style_sheets_for.begin(),
                                invalidate_page_tweaks_font_family_style_sheets_for.end(), updated.old_value);
                            if(invalidate_it == invalidate_page_tweaks_font_family_style_sheets_for.end())
                            {
                                invalidate_page_tweaks_font_family_style_sheets_for.insert(invalidate_it, updated.old_value);
                            }
                        }
                    }

                    if(!updated.new_value.length())
                    {
                        continue;
                    }

                    auto& value_pair = (*self->wrapper.page_tweaks_font_family_map)[updated.new_value];
                    
                    if(value_pair.first.insert(website))
                    {
                        auto invalidate_it = std::lower_bound(invalidate_page_tweaks_font_family_style_sheets_for.begin(),
                            invalidate_page_tweaks_font_family_style_sheets_for.end(), updated.new_value);
                        if(invalidate_it == invalidate_page_tweaks_font_family_style_sheets_for.end() || *invalidate_it != updated.new_value)
                        {
                            invalidate_page_tweaks_font_family_style_sheets_for.insert(invalidate_it, updated.new_value);
                        }
                    }
                }
                else if(page_tweaks_disable_sticky_enable && updated.key_name == orion::settings::website::specific::page_tweaks_disable_sticky.key_name())
                {
                    bool enabled = false;
                    if(orion::Error err = orion::settings::unmarshal_value(updated.new_value, &enabled))
                    {
                        LOG_ERROR(std::move(err));
                        continue;
                    }

                    orion::log_verbose("ProfileDataHolder settings observer: page tweaks disable sticky %s for website %s", enabled?"enabled":"disabled", website.c_str());

                    if(enabled)
                    {
                        if(self->wrapper.page_tweaks_disable_sticky_filter.insert(website))
                        {
                            invalidate_page_tweaks_disable_sticky_list = true;
                        }
                    }
                    else
                    {
                        if(self->wrapper.page_tweaks_disable_sticky_filter.remove(website))
                        {
                            invalidate_page_tweaks_disable_sticky_list = true;
                        }
                    }
                }
#endif
            }

            
        }

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        if(invalidate_page_tweaks_dark_mode_list)
        {
            invalidate_page_tweaks_dark_mode_style_sheet(self);
        }
        if(invalidate_page_tweaks_font_size_style_sheets_for.size())
        {
            invalidate_page_tweaks_font_size_style_sheets(self, invalidate_page_tweaks_font_size_style_sheets_for);
        }
        if(invalidate_page_tweaks_font_family_style_sheets_for.size())
        {
            invalidate_page_tweaks_font_family_style_sheets(self, invalidate_page_tweaks_font_family_style_sheets_for);
        }
        if(invalidate_page_tweaks_disable_sticky_list)
        {
            invalidate_page_tweaks_disable_sticky_filter(self);
        }
        if(invalidate_page_tweaks_content_blockers)
        {
            g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PAGE_TWEAKS_ELEMENT_BLOCKERS]);
        }
#endif
        

    });
}

#pragma endregion InternalMethods



#pragma region GettersSetters

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
const orion::DomainFilter& profile_data_holder_get_page_tweaks_disabled_filter(ProfileDataHolder* self)
{
    return self->wrapper.page_tweaks_disable_filter;
}

const orion::WebKitUserStyleSheetMap&
profile_data_holder_get_page_tweaks_content_blockers_map(ProfileDataHolder* self)
{
    return *self->wrapper.page_tweaks_element_blockers_map;
}

const orion::TypeToWebKitUserStyleSheetMap<orion::settings::PageZoom>&
profile_data_holder_get_page_tweaks_font_size_map(ProfileDataHolder* self)
{
    return *self->wrapper.page_tweaks_font_size_map;
}

const orion::DomainFilter&
profile_data_holder_get_page_tweaks_force_dark_mode_filter(ProfileDataHolder* self)
{
    return self->wrapper.page_tweaks_force_dark_mode_filter;
}

const orion::TypeToWebKitUserStyleSheetMap<std::string>&
profile_data_holder_get_page_tweaks_font_family_map(ProfileDataHolder* self)
{
    return *self->wrapper.page_tweaks_font_family_map;
}

const orion::DomainFilter& profile_data_holder_get_page_tweaks_sticky_disabled_filter(ProfileDataHolder* self)
{
    return self->wrapper.page_tweaks_disable_sticky_filter;
}
#endif

static void
profile_data_holder_set_profile_id(ProfileDataHolder* self, orion::ProfileId id)
{
    //profile-id is a CONSTRUCT_ONLY property, meaning it will only ever be set once
    self->wrapper.profile_id = id;

    bind_to_settings(self, id);

    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PROFILE_ID]);
}

static void
profile_data_holder_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(object);

    switch(property_id)
    {
        case PROP_PROFILE_ID:
            g_value_set_int64(value, self->wrapper.profile_id.value);
            break;
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        case PROP_PAGE_TWEAKS_FONT_SIZE_STYLE_SHEETS:
            g_value_set_pointer(value, self->wrapper.page_tweaks_font_size_map);
            break;
        case PROP_PAGE_TWEAKS_DARK_MODE_STYLE_SHEET:
            g_value_set_boxed(value, self->page_tweaks_dark_mode_style_sheet);
            break;
        case PROP_PAGE_TWEAKS_DARK_MODE_FILTER:
            g_value_set_pointer(value, &self->wrapper.page_tweaks_force_dark_mode_filter);
            break;
        case PROP_PAGE_TWEAKS_FONT_FAMILY_STYLE_SHEETS:
            g_value_set_pointer(value, self->wrapper.page_tweaks_font_family_map);
            break;
        case PROP_PAGE_TWEAKS_DISABLE_STICKY_FILTER:
            g_value_set_pointer(value, &self->wrapper.page_tweaks_disable_sticky_filter);
            break;
        case PROP_PAGE_TWEAKS_ELEMENT_BLOCKERS:
            g_value_set_pointer(value, self->wrapper.page_tweaks_element_blockers_map);
            break;
#else
        case PROP_PAGE_TWEAKS_FONT_SIZE_STYLE_SHEETS:
        case PROP_PAGE_TWEAKS_FONT_FAMILY_STYLE_SHEETS:
        case PROP_PAGE_TWEAKS_ELEMENT_BLOCKERS:
            g_value_set_pointer(value, NULL);
            break;
#endif
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
profile_data_holder_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(object);

    switch(property_id)
    {
        case PROP_PROFILE_ID:
            profile_data_holder_set_profile_id(self, orion::ProfileId(g_value_get_int64(value)));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

#pragma endregion GettersSetters



#pragma region InitAndDisposal

ProfileDataHolder*
profile_data_holder_new(orion::ProfileId profile_id)
{
	return APP_PROFILE_DATA_HOLDER(g_object_new(APP_TYPE_PROFILE_DATA_HOLDER, "profile-id", profile_id.value, NULL));
}

static void
bind_to_profile_id(ProfileDataHolder* self)
{

}

static void
profile_data_holder_init(ProfileDataHolder* self)
{
    std::construct_at(&self->wrapper);

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    self->wrapper.page_tweaks_font_size_map = new orion::TypeToWebKitUserStyleSheetMap<orion::settings::PageZoom>;
    self->wrapper.page_tweaks_font_family_map = new orion::TypeToWebKitUserStyleSheetMap<std::string>;
    self->wrapper.page_tweaks_element_blockers_map = new orion::WebKitUserStyleSheetMap;
#endif
}

static void
profile_data_holder_dispose(GObject* object)
{
    ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(object);
    global_core_api->settings_change().clear_observer(&self->wrapper.settings_observer);
    G_OBJECT_CLASS(profile_data_holder_parent_class)->dispose(object);
}

static void
profile_data_holder_finalize(GObject* object)
{
    ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(object);

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(self->wrapper.page_tweaks_font_size_map)
    {
        for(auto& pair : *self->wrapper.page_tweaks_font_size_map)
        {
            webkit_user_style_sheet_unref(pair.second.second);
        }
        delete self->wrapper.page_tweaks_font_size_map;
        self->wrapper.page_tweaks_font_size_map = NULL;
    }

    g_clear_pointer(&self->page_tweaks_dark_mode_style_sheet, webkit_user_style_sheet_unref);

    if(self->wrapper.page_tweaks_font_family_map)
    {
        for(auto& pair : *self->wrapper.page_tweaks_font_family_map)
        {
            webkit_user_style_sheet_unref(pair.second.second);
        }
        delete self->wrapper.page_tweaks_font_family_map;
        self->wrapper.page_tweaks_font_family_map = NULL;
    }

    if(self->wrapper.page_tweaks_element_blockers_map)
    {
        for(auto& pair : *self->wrapper.page_tweaks_element_blockers_map)
        {
            webkit_user_style_sheet_unref(pair.second);
        }
        delete self->wrapper.page_tweaks_font_size_map;
        self->wrapper.page_tweaks_element_blockers_map = NULL;
    }
#endif

    std::destroy_at(&self->wrapper);

    G_OBJECT_CLASS(profile_data_holder_parent_class)->finalize(object);
}

static void
profile_data_holder_class_init(ProfileDataHolderClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = profile_data_holder_dispose;
    object_class->finalize = profile_data_holder_finalize;
    object_class->get_property = profile_data_holder_get_property;
    object_class->set_property = profile_data_holder_set_property;

    
    obj_properties[PROP_PROFILE_ID] = g_param_spec_int64(
        "profile-id",
        NULL,
        NULL,
        G_MININT64,
        G_MAXINT64,
        0,
        GParamFlags(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY)
    );
    obj_properties[PROP_PAGE_TWEAKS_FONT_SIZE_STYLE_SHEETS] = g_param_spec_pointer(
        "page-tweaks-font-size-map",
        NULL,
        NULL,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY)
    );
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    obj_properties[PROP_PAGE_TWEAKS_DARK_MODE_STYLE_SHEET] = g_param_spec_boxed(
        "page-tweaks-dark-mode-style-sheet",
        NULL,
        NULL,
        webkit_user_style_sheet_get_type(),
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY)
    );
#endif
    obj_properties[PROP_PAGE_TWEAKS_DARK_MODE_FILTER] = g_param_spec_pointer(
        "page-tweaks-dark-mode-filter",
        NULL,
        NULL,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY)
    );
    obj_properties[PROP_PAGE_TWEAKS_DISABLE_STICKY_FILTER] = g_param_spec_pointer(
        "page-tweaks-disable-sticky-filter",
        NULL,
        NULL,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY)
    );
    obj_properties[PROP_PAGE_TWEAKS_FONT_FAMILY_STYLE_SHEETS] = g_param_spec_pointer(
        "page-tweaks-font-family-map",
        NULL,
        NULL,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY)
    );
    obj_properties[PROP_PAGE_TWEAKS_ELEMENT_BLOCKERS] = g_param_spec_pointer(
        "page-tweaks-element-blockers",
        NULL,
        NULL,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY)
    );

    g_object_class_install_properties(object_class, N_PROPS, obj_properties);


/**
 * ProfileDataHolder::setting-changed:
 * @data_holder: the ProfileDataHolder that changed
 * @setting_key: the setting key for which the value has changed
 * @setting_value: the marshalled string new value the setting
 *
 * Signals that the cached value of a setting has changed
 *
 **/
    obj_signals[SIG_SETTING_CHANGED] = g_signal_new("setting-changed", APP_TYPE_PROFILE_DATA_HOLDER, G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
        g_cclosure_marshal_generic, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
}

#pragma endregion InitAndDisposal

#pragma region Settings

std::string
profile_data_holder_get_setting_raw_val_by_db_key(ProfileDataHolder* self, std::string key)
{
    std::string action_name = orion::gtk::setting_db_key_to_action_name(key);

    if(!self->wrapper.settings_map.contains(action_name))
    {
        orion::log_error("cannot resolve setting for action name %s in profile data holder", action_name.c_str());
        return "";
    }

    return self->wrapper.settings_map.at(action_name);
}

std::string
profile_data_holder_get_setting_raw_val_by_action_name(ProfileDataHolder* self, std::string action_name)
{
    if(!self->wrapper.settings_map.contains(action_name))
    {
        orion::log_error("cannot resolve setting for action name %s in profile data holder", action_name.c_str());
        return "";
    }

    return self->wrapper.settings_map.at(action_name);
}

#pragma endregion Settings

#pragma region ActionGroup

static gchar**
list_actions(GActionGroup* action_group)
{
    ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(action_group);

    gchar** names = g_new0(char*, (self->wrapper.settings_map.size()+1)); //+1 for NULL term ensured by g_new0

    auto it = self->wrapper.settings_map.begin();
    size_t i = 0;
    while(i<self->wrapper.settings_map.size())
    {
        names[i] = g_strdup(it->first.c_str());

        ++it;
        ++i;
    }

    return names;
}

static gboolean
query_action(
  GActionGroup* action_group,
  const gchar* action_name,
  gboolean* enabled,
  const GVariantType** parameter_type,
  const GVariantType** state_type,
  GVariant** state_hint,
  GVariant** state
)
{
    ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(action_group);
    if(!action_name) return FALSE;

    if(!self->wrapper.settings_map.contains(action_name)) return FALSE;
    
    GVariant* var = orion::gtk::VariantTraits<std::string>::create(self->wrapper.settings_map.at(action_name));
   
    const GVariantType* type = orion::gtk::VariantTraits<std::string>::type();

    if(enabled) *enabled = TRUE;

    if(parameter_type) *parameter_type = type;
    if(state_type) *state_type = type;
    
    if(state_hint) *state_hint = NULL; //would be extremely difficult to implement
    if(state) *state = var;

    return TRUE;
}

static void
activate_action(
  GActionGroup* action_group,
  const gchar* action_name,
  GVariant* parameter
)
{
    ProfileDataHolder* self = APP_PROFILE_DATA_HOLDER(action_group);
    if(!action_name) return;

    if(!self->wrapper.settings_map.contains(action_name)) return;

    orion::gtk::set_setting_from_variant(orion::gtk::setting_action_name_to_db_key(action_name), parameter, self->wrapper.profile_id);
}

static void
change_action_state(GActionGroup* action_group, const gchar* action_name, GVariant* value)
{
    //intentional noop
}

static void
profile_data_holder_action_group_interface_init(GActionGroupInterface* iface)
{
    iface->list_actions = list_actions;
    iface->query_action = query_action;
    iface->activate_action = activate_action;
    iface->change_action_state = change_action_state;
}

#pragma endregion ActionGroup



#ifndef NDEBUG
void
profile_data_holder_debug_print(ProfileDataHolder* self)
{
    for(const auto& pair : self->wrapper.settings_map)
    {
        orion::log_debug("key: %s", orion::gtk::setting_action_name_to_db_key(pair.first).c_str());
        orion::log_debug("action name: %s", pair.first.c_str());
        orion::log_debug("raw value: %s", pair.second.c_str());

    }
}
#endif
