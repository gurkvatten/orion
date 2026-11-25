#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include "permission_handler.h"
#include <memory>
#include <unordered_map>
#include "../util/text_util.h"
#include <common/src/log.h>
#include "../main_window/main_window.h"
#include <core/settings.h>

class ClipboardPermissionHandler : public PermissionHandler {
    typedef orion::settings::website::specific::internal_key_clipboard_default_setting InternalClipboardSetting;
    typedef orion::settings::Setting<InternalClipboardSetting> ClipboardSetting;
public:
    static std::unique_ptr<PermissionHandler>
    create()
    {
        return std::make_unique<ClipboardPermissionHandler>();
    }

    std::string
    primary_description(std::string website) override
    {
        return orion::text::translatef("The website “%s” would like to get CLIPBOARD_PERMISSION.", website.c_str());
    }

    std::string
    secondary_description() override
    {
        return _("You can change this setting in Orion Websites preferences.");
    }

    TypeOfDialog
    get_type_of_dialog() override
    {
        return TypeOfDialog::quadruplet;
    }

    bool
    is_auto_deny_on_http() override
    {
        return false;
    }
    
    void
    allow_persistently(const std::string& website) override
    {
        using orion::settings::ClipboardWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            ClipboardSetting clipboard_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, clipboard_setting,
                ClipboardWebsiteSetting::allow,
                [](orion::SetSettingReply<orion::settings::ClipboardWebsiteSetting>){});
       });        
    }

    void
    deny_persistently(const std::string& website) override
    {
        using orion::settings::ClipboardWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            ClipboardSetting clipboard_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, clipboard_setting,
                ClipboardWebsiteSetting::dont_allow,
                [](orion::SetSettingReply<orion::settings::ClipboardWebsiteSetting>){});
       });        
    }
 
    void
    start_request(
        const std::string& website, std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr,
        std::function<void(bool)> callback) override
    {

    }
};

class DeviceInfoPermissionHandler : public PermissionHandler {
        typedef orion::settings::website::specific::internal_key_device_info_default_setting InternalDeviceInfoSetting;
    typedef orion::settings::Setting<InternalDeviceInfoSetting> DeviceInfoSetting;

public:
    static std::unique_ptr<PermissionHandler>
    create()
    {
        return std::make_unique<DeviceInfoPermissionHandler>();
    }

    std::string
    primary_description(std::string website) override
    {
        return orion::text::translatef("The website “%s” would like to get DEVICE_INFO_PERMISSION.", website.c_str());
    }

    std::string
    secondary_description() override
    {
        return _("You can change this setting in Orion Websites preferences.");
    }

    TypeOfDialog
    get_type_of_dialog() override
    {
        return TypeOfDialog::quadruplet;
    }

    bool
    is_auto_deny_on_http() override
    {
        return true;
    }

    void
    allow_persistently(const std::string& website) override
    {
        using orion::settings::DeviceInfoWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            DeviceInfoSetting device_info_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, device_info_setting,
                DeviceInfoWebsiteSetting::allow,
                [](orion::SetSettingReply<orion::settings::DeviceInfoWebsiteSetting>){});
       });        
    }

    void
    deny_persistently(const std::string& website) override
    {
        using orion::settings::DeviceInfoWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            DeviceInfoSetting device_info_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, device_info_setting, 
                DeviceInfoWebsiteSetting::dont_allow,
                [](orion::SetSettingReply<orion::settings::DeviceInfoWebsiteSetting>){});
       });        
    }
 
    void
    start_request(const std::string& website,
        std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr, std::function<void(bool)> callback) override
    {
    }    
};

// Test website: https://mylocation.org/
class GeolocationPermissionHandler : public PermissionHandler {
private:
    typedef orion::settings::website::specific::internal_key_location_default_setting InternalLocationSetting;
    typedef orion::settings::Setting<InternalLocationSetting> LocationSetting;
public:
    static std::unique_ptr<PermissionHandler>
    create()
    {
        return std::make_unique<GeolocationPermissionHandler>();
    }

    std::string
    primary_description(std::string website) override
    {
        return orion::text::translatef("The website “%s” would like to use your current location.", website.c_str());
    }

    std::string
    secondary_description() override
    {
        return _("You can change this setting in Orion Websites preferences.");
    }

    TypeOfDialog
    get_type_of_dialog() override
    {
        return TypeOfDialog::quadruplet;
    }

    bool
    is_auto_deny_on_http() override
    {
        return true;
    }
    
    void
    allow_persistently(const std::string& website) override
    {
        using orion::settings::LocationWebsiteSetting;
        orion::log_debug("website: %s", website.c_str());

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            LocationSetting my_location_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, my_location_setting,
                LocationWebsiteSetting::allow,
                [](orion::SetSettingReply<orion::settings::LocationWebsiteSetting>){});
       });        
    }

    void
    deny_persistently(const std::string& website) override
    {
        using orion::settings::LocationWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            LocationSetting my_location_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, my_location_setting,
                LocationWebsiteSetting::deny,
                [](orion::SetSettingReply<orion::settings::LocationWebsiteSetting>){});
       });    
    }

    //Could be optimised by templates? UwU
    void
    start_request(
        const std::string& website,
        std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr,
        std::function<void(bool)> callback) override
    {
        using orion::settings::LocationWebsiteSetting;
    
        std::string realm = std::format("settings::website::specific::{}", website);
        global_core_api->get_default_profile_id([realm, request_ultra_ptr, callback](orion::GetDefaultProfileIdReply profile_reply) {
            LocationSetting my_location_setting(strdup(realm.c_str())); // TODO: this leaks

            global_core_api->get_setting(profile_reply.profile_id, my_location_setting,
                [request_ultra_ptr, profile_reply, callback]
                (orion::GetSettingReply<orion::settings::LocationWebsiteSetting> specific_reply){              
                    VERIFY_LOG(specific_reply.err);

                    WebKitPermissionRequest* request = WEBKIT_PERMISSION_REQUEST(request_ultra_ptr.get()->get()); 
                    switch (specific_reply.value)
                    {
                    case LocationWebsiteSetting::not_set:
                        orion::log_debug("Website specific permission is not_set, trying defaults for all websites");
                        

                        global_core_api->get_setting(profile_reply.profile_id,
                            orion::settings::websites::location_default_setting_for_all_websites, 
                            [request_ultra_ptr, profile_reply, callback]
                            (orion::GetSettingReply<orion::settings::LocationWebsiteSetting> all_reply){
                                VERIFY_LOG(all_reply.err);
                                WebKitPermissionRequest* request = WEBKIT_PERMISSION_REQUEST(request_ultra_ptr.get()->get()); 
                                switch (all_reply.value)
                                {                      
                                case LocationWebsiteSetting::allow:
                                    webkit_permission_request_allow(request);
                                    callback(true);
                                    break;
                                case LocationWebsiteSetting::deny:
                                    webkit_permission_request_deny(request);
                                    callback(true);
                                    break;
                                case LocationWebsiteSetting::ask:
                                    callback(false);
                                    break;
                                case LocationWebsiteSetting::not_set:
                                    orion::log_warning("default_setting_for_all_websites enum value is not_set");
                                    callback(false);
                                    break;
                                }
                            });
                        break;
                    case LocationWebsiteSetting::allow:
                        webkit_permission_request_allow(request);
                        callback(true);
                        break;
                    case LocationWebsiteSetting::deny:
                        webkit_permission_request_deny(request);
                        callback(true);
                        break;
                    case LocationWebsiteSetting::ask:
                        callback(false);
                        break;
                    }
                    orion::log_debug("get setting: key=%s type=%s default_value=%d value=%d",
                        specific_reply.setting->key().c_str(), specific_reply.setting->type().c_str(),
                        specific_reply.default_value, specific_reply.value
                    );
                });
        });
    }
};

class MediaKeySystemPermissionHandler : public PermissionHandler {
private:
    typedef orion::settings::website::specific::internal_key_media_key_system_default_setting InternalMediaKeySystemSetting;
    typedef orion::settings::Setting<InternalMediaKeySystemSetting> MediaKeySystemSetting;
public:
    static std::unique_ptr<PermissionHandler>
    create()
    {
        return std::make_unique<MediaKeySystemPermissionHandler>();
    }

    std::string
    primary_description(std::string website) override
    {
        return orion::text::translatef(
            "The website “%s” would like to get MEDIA_KEY_SYSTEM_PERMISSION.",
            website.c_str()
        );
    }

    std::string
    secondary_description() override
    {
        return _("You can change this setting in Orion Websites preferences.");
    }

    TypeOfDialog
    get_type_of_dialog() override
    {
        return TypeOfDialog::quadruplet;
    }

    bool
    is_auto_deny_on_http() override
    {
        return true;
    }

    void
    allow_persistently(const std::string& website) override
    {
        using orion::settings::MediaKeySystemWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            MediaKeySystemSetting media_key_system_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, media_key_system_setting,
                MediaKeySystemWebsiteSetting::allow,
                [](orion::SetSettingReply<orion::settings::MediaKeySystemWebsiteSetting>){});
       });        
    }

    void
    deny_persistently(const std::string& website) override
    {
        using orion::settings::MediaKeySystemWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            MediaKeySystemSetting media_key_system_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, media_key_system_setting,
                MediaKeySystemWebsiteSetting::dont_allow,
                [](orion::SetSettingReply<orion::settings::MediaKeySystemWebsiteSetting>){});
       });        
    }
 
    void
    start_request(const std::string& website, std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr,
        std::function<void(bool)> callback) override
    {
    }    
};

class NotificationPermissionHandler : public PermissionHandler {
private:
    typedef orion::settings::website::specific::internal_key_notifications_default_setting InternalNotificationsSetting;
    typedef orion::settings::Setting<InternalNotificationsSetting> NotificationsSetting;

public:
    static std::unique_ptr<PermissionHandler>
    create()
    {
        return std::make_unique<NotificationPermissionHandler>();
    }

    std::string
    primary_description(std::string website) override
    {
        return orion::text::translatef(
            "“%s” is requesting permission to send desktop notifications to Orion.",
            website.c_str()
        );
    }

    std::string
    secondary_description() override
    {
        return _("Desktop notification settings can be managed in Settings > Websites > Notifications.");
    }

    TypeOfDialog
    get_type_of_dialog() override
    {
        return TypeOfDialog::binary;
    }

    bool
    is_auto_deny_on_http() override
    {
        return true;
    }
    
    void
    allow_persistently(const std::string& website) override
    {
        using orion::settings::NotificationsWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            NotificationsSetting notifications_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, notifications_setting,
                NotificationsWebsiteSetting::allow,
                [](orion::SetSettingReply<orion::settings::NotificationsWebsiteSetting>){});
       });   
    }

    void
    deny_persistently(const std::string& website) override
    {
        using orion::settings::NotificationsWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            NotificationsSetting notifications_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, notifications_setting,
                NotificationsWebsiteSetting::dont_allow,
                [](orion::SetSettingReply<orion::settings::NotificationsWebsiteSetting>){});
       });   
    }
 
    void
    start_request(
        const std::string& website, std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr,
        std::function<void(bool)> callback) override
    {
        using orion::settings::NotificationsWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        global_core_api->get_default_profile_id([realm, request_ultra_ptr, callback](orion::GetDefaultProfileIdReply profile_reply) {
            NotificationsSetting notifications_setting(strdup(realm.c_str())); // TODO: this leaks

            global_core_api->get_setting(profile_reply.profile_id, notifications_setting,
                [request_ultra_ptr, profile_reply, callback]
                (orion::GetSettingReply<orion::settings::NotificationsWebsiteSetting> specific_reply) {
                    VERIFY_LOG(specific_reply.err);

                    WebKitPermissionRequest* request = WEBKIT_PERMISSION_REQUEST(request_ultra_ptr.get()->get());
                    switch (specific_reply.value) {
                    case NotificationsWebsiteSetting::not_set:
                        global_core_api->get_setting(profile_reply.profile_id,
                            orion::settings::websites::notifications_default_setting_for_all_websites,
                            [request_ultra_ptr, callback]
                            (orion::GetSettingReply<orion::settings::NotificationsWebsiteSetting> all_reply) {
                                VERIFY_LOG(all_reply.err);
                                WebKitPermissionRequest* request = WEBKIT_PERMISSION_REQUEST(request_ultra_ptr.get()->get());
                                switch (all_reply.value) {
                                case NotificationsWebsiteSetting::allow:
                                    webkit_permission_request_allow(request);
                                    callback(true);
                                    break;
                                case NotificationsWebsiteSetting::dont_allow:
                                    webkit_permission_request_deny(request);
                                    callback(true);
                                    break;
                                case NotificationsWebsiteSetting::ask:
                                case NotificationsWebsiteSetting::not_set:
                                    callback(false);
                                    break;
                                }
                            });
                        break;
                    case NotificationsWebsiteSetting::allow:
                        webkit_permission_request_allow(request);
                        callback(true);
                        break;
                    case NotificationsWebsiteSetting::dont_allow:
                        webkit_permission_request_deny(request);
                        callback(true);
                        break;
                    case NotificationsWebsiteSetting::ask:
                        callback(false);
                        break;
                    }
                });
        });
    }
};

class PointerLockPermissionHandler : public PermissionHandler {
private:
    typedef orion::settings::website::specific::internal_key_pointer_lock_default_setting InternalPointerLockSetting;
    typedef orion::settings::Setting<InternalPointerLockSetting> PointerLockSetting;

public:
    static std::unique_ptr<PermissionHandler>
    create()
    {
        return std::make_unique<PointerLockPermissionHandler>();
    }

    std::string
    primary_description(std::string website) override
    {
        return orion::text::translatef("The website “%s” would like to get POINTER_LOCK_PERMISSION", website.c_str());
    }

    std::string
    secondary_description() override
    {
        return _("You can change this setting in Orion Websites preferences.");
    }

    TypeOfDialog
    get_type_of_dialog() override
    {
        return TypeOfDialog::quadruplet;
    }

    bool
    is_auto_deny_on_http() override
    {
        return false;
    }
    
    void
    allow_persistently(const std::string& website) override
    {
        using orion::settings::PointerLockWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            PointerLockSetting pointer_lock_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, pointer_lock_setting,
                PointerLockWebsiteSetting::allow,
                [](orion::SetSettingReply<orion::settings::PointerLockWebsiteSetting>){});
       });   
    }

    void
    deny_persistently(const std::string& website) override
    {
        using orion::settings::PointerLockWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            PointerLockSetting pointer_lock_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, pointer_lock_setting,
                PointerLockWebsiteSetting::dont_allow,
                [](orion::SetSettingReply<orion::settings::PointerLockWebsiteSetting>){});
       });
    }
 
    void
    start_request(const std::string& website, std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr,
        std::function<void(bool)> callback) override
    {
    }
};

class CameraPermissionHandler : public PermissionHandler {
private:
    CameraPermissionHandler(orion::ProfileId profile_id)
        : m_profile_id(profile_id)
    {
    }

public:
    static std::unique_ptr<PermissionHandler>
    create(orion::ProfileId profile_id)
    {
        // Use 'new' to access the private constructor
        return std::unique_ptr<CameraPermissionHandler>(new CameraPermissionHandler(profile_id));
    }

    std::string
    primary_description(std::string website) override
    {
        return orion::text::translatef("Allow %s to use your camera?", website.c_str());
    }

    std::string
    secondary_description() override
    {
        return _("You can change this setting in Orion Websites preferences.");
    }

    TypeOfDialog
    get_type_of_dialog() override
    {
        return TypeOfDialog::quadruplet;
    }

    bool
    is_auto_deny_on_http() override
    {
        return true;
    }

    void
    persist_setting(orion::settings::CameraWebsiteSetting value, const std::string& website)
    {
        global_core_api->set_setting_raw(global_core_api->create_set_setting_query_for_website_integer({
            .profile_id = m_profile_id,
            .key_name = "camera_default_setting",
            .value = static_cast<int64_t>(value),
            .default_value = static_cast<int64_t>(orion::settings::CameraWebsiteSetting::not_set),
            .website = website,
        }), [website,value](orion::SetSettingReply<std::string> reply) {
            VERIFY_LOG(reply.err);
            orion::log_debug(
                "camera permission persistently set to %lld for website '%s'",
                static_cast<int64_t>(value), website.c_str()
            );
        });
    }

    void
    allow_persistently(const std::string& website) override
    {
        persist_setting(orion::settings::CameraWebsiteSetting::allow, website);
    }

    void
    deny_persistently(const std::string& website) override
    {
        persist_setting(orion::settings::CameraWebsiteSetting::deny, website);
    }

    void
    start_request(
        const std::string& website,
        std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr,
        std::function<void(bool)> callback) override
    {
    }

private:
    orion::ProfileId m_profile_id{};
};

class MicrophonePermissionHandler : public PermissionHandler {
private:
    MicrophonePermissionHandler(orion::ProfileId profile_id)
        : m_profile_id(profile_id)
    {
    }

public:
    static std::unique_ptr<PermissionHandler>
    create(orion::ProfileId profile_id)
    {
        // Use 'new' to access the private constructor
        return std::unique_ptr<MicrophonePermissionHandler>(new MicrophonePermissionHandler(profile_id));
    }

    std::string
    primary_description(std::string website) override
    {
        return orion::text::translatef("Allow %s to use your microphone?", website.c_str());
    }

    std::string
    secondary_description() override
    {
        return _("You can change this setting in Orion Websites preferences.");
    }

    TypeOfDialog
    get_type_of_dialog() override
    {
        return TypeOfDialog::quadruplet;
    }

    bool
    is_auto_deny_on_http() override
    {
        return true;
    }

    void
    persist_setting(orion::settings::MicrophoneWebsiteSetting value, const std::string& website)
    {
        global_core_api->set_setting_raw(global_core_api->create_set_setting_query_for_website_integer({
            .profile_id = m_profile_id,
            .key_name = "microphone_default_setting",
            .value = static_cast<int64_t>(value),
            .default_value = static_cast<int64_t>(orion::settings::MicrophoneWebsiteSetting::not_set),
            .website = website,
        }), [website,value](orion::SetSettingReply<std::string> reply) {
            VERIFY_LOG(reply.err);
            orion::log_debug(
                "microphone permission persistently set to %lld for website '%s'",
                static_cast<int64_t>(value), website.c_str()
            );
        });
    }

    void
    allow_persistently(const std::string& website) override
    {
        persist_setting(orion::settings::MicrophoneWebsiteSetting::allow, website);
    }

    void
    deny_persistently(const std::string& website) override
    {
        persist_setting(orion::settings::MicrophoneWebsiteSetting::deny, website);
    }

    void
    start_request(
        const std::string& website,
        std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr,
        std::function<void(bool)> callback) override
    {
    }

private:
    orion::ProfileId m_profile_id{};
};

class ScreenSharingPermissionHandler : public PermissionHandler {
private:
    ScreenSharingPermissionHandler(orion::ProfileId profile_id)
        : m_profile_id(profile_id)
    {
    }

public:
    static std::unique_ptr<PermissionHandler>
    create(orion::ProfileId profile_id)
    {
        // Use 'new' to access the private constructor
        return std::unique_ptr<ScreenSharingPermissionHandler>(new ScreenSharingPermissionHandler(profile_id));
    }

    std::string
    primary_description(std::string website) override
    {
        return orion::text::translatef("Allow %s to share your screen?", website.c_str());
    }

    std::string
    secondary_description() override
    {
        return _("You can change this setting in Orion Websites preferences.");
    }

    TypeOfDialog
    get_type_of_dialog() override
    {
        return TypeOfDialog::quadruplet;
    }

    bool
    is_auto_deny_on_http() override
    {
        return true;
    }

    void
    persist_setting(orion::settings::ScreenSharingWebsiteSetting value, const std::string& website)
    {
        global_core_api->set_setting_raw(global_core_api->create_set_setting_query_for_website_integer({
            .profile_id = m_profile_id,
            .key_name = "screen_sharing_default_setting",
            .value = static_cast<int64_t>(value),
            .default_value = static_cast<int64_t>(orion::settings::ScreenSharingWebsiteSetting::not_set),
            .website = website,
        }), [website,value](orion::SetSettingReply<std::string> reply) {
            VERIFY_LOG(reply.err);
            orion::log_debug(
                "screen sharing permission persistently set to %lld for website '%s'",
                static_cast<int64_t>(value), website.c_str()
            );
        });
    }

    void
    allow_persistently(const std::string& website) override
    {
        persist_setting(orion::settings::ScreenSharingWebsiteSetting::allow, website);
    }

    void
    deny_persistently(const std::string& website) override
    {
        persist_setting(orion::settings::ScreenSharingWebsiteSetting::deny, website);
    }

    void
    start_request(
        const std::string& website,
        std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr,
        std::function<void(bool)> callback) override
    {
    }

private:
    orion::ProfileId m_profile_id{};
};

class UserMediaPermissionHandler : public PermissionHandler {
public:
    static std::unique_ptr<PermissionHandler>
    create()
    {
        return std::make_unique<UserMediaPermissionHandler>();
    }

    std::string
    primary_description(std::string website) override
    {
        return orion::text::translatef("The website “%s” would like to get USER_MEDIA_PERMISSION", website.c_str());
    }

    std::string
    secondary_description() override
    {
        return _("You can change this setting in Orion Websites preferences.");
    }

    TypeOfDialog
    get_type_of_dialog() override
    {
        return TypeOfDialog::quadruplet;
    }

    bool
    is_auto_deny_on_http() override
    {
        return true;
    }

    void
    allow_persistently(const std::string& website) override
    {
        //TODO camerea vs microphone
        orion::log_debug("Persistently allowing Notification");
        orion::log_warning("NOT IMPLEMENTED");
        //orion::settings::websites::notifications_default_setting_for_all_websites = NotificationsWebsiteSetting::allow;
    }

    void
    deny_persistently(const std::string& website) override
    {
        //TODO camerea vs microphone
        orion::log_debug("Persistently denying Notification");
        orion::log_warning("NOT IMPLEMENTED");
    }

    void
    start_request(
        const std::string& website,
        std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr,
        std::function<void(bool)> callback) override
    {
    }
};

class WebsiteDataAccessPermissionHandler : public PermissionHandler {
private:
    typedef orion::settings::website::specific::internal_key_website_data_access_default_setting InternalWebsiteDataAccessSetting;
    typedef orion::settings::Setting<InternalWebsiteDataAccessSetting> WebsiteDataAccessSetting;

public:
    static std::unique_ptr<PermissionHandler>
    create()
    {
        return std::make_unique<WebsiteDataAccessPermissionHandler>();
    }

    std::string
    primary_description(std::string website) override
    {
        return orion::text::translatef(
            "The website “%s” would like to get WEBSITE_DATA_ACCESS_PERMISSION", website.c_str()
        );
    }

    std::string
    secondary_description() override
    {
        return _("You can change this setting in Orion Websites preferences.");
    }

    TypeOfDialog
    get_type_of_dialog() override
    {
        return TypeOfDialog::quadruplet;
    }

    bool
    is_auto_deny_on_http() override
    {
        return true;
    }
    
    void
    allow_persistently(const std::string& website) override
    {
        using orion::settings::WebsiteDataAccessWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            WebsiteDataAccessSetting website_data_access_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, website_data_access_setting,
                WebsiteDataAccessWebsiteSetting::allow,
                [](orion::SetSettingReply<orion::settings::WebsiteDataAccessWebsiteSetting>){});
       });  
    }

    void
    deny_persistently(const std::string& website) override
    {
        using orion::settings::WebsiteDataAccessWebsiteSetting;

        std::string realm = std::format("settings::website::specific::{}", website);
        
        orion::log_debug(realm.c_str());
        global_core_api->get_default_profile_id([realm](orion::GetDefaultProfileIdReply reply) {
            WebsiteDataAccessSetting website_data_access_setting(realm.c_str());
            global_core_api->set_setting(reply.profile_id, website_data_access_setting,
                WebsiteDataAccessWebsiteSetting::dont_allow,
                [](orion::SetSettingReply<orion::settings::WebsiteDataAccessWebsiteSetting>){});
       });  
    }

    void start_request(const std::string& website, std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr,
        std::function<void(bool)> callback) override
    {
    }
};

std::unique_ptr<PermissionHandler>
create_clipboard_permission_handler()
{
    return ClipboardPermissionHandler::create();
}

std::unique_ptr<PermissionHandler>
create_device_info_permission_handler()
{
    return DeviceInfoPermissionHandler::create();
}

std::unique_ptr<PermissionHandler>
create_geolocation_permission_handler()
{
    return GeolocationPermissionHandler::create();
}

std::unique_ptr<PermissionHandler>
create_media_key_system_permission_handler()
{
    return MediaKeySystemPermissionHandler::create();
}

std::unique_ptr<PermissionHandler>
create_notification_permission_handler()
{
    return NotificationPermissionHandler::create();
}

std::unique_ptr<PermissionHandler>
create_pointer_lock_permission_handler()
{
    return PointerLockPermissionHandler::create();
}

std::unique_ptr<PermissionHandler>
create_user_media_permission_handler()
{
    return UserMediaPermissionHandler::create();
}

std::unique_ptr<PermissionHandler>
create_camera_permission_handler(orion::ProfileId profile_id)
{
    return CameraPermissionHandler::create(profile_id);
}

std::unique_ptr<PermissionHandler>
create_microphone_permission_handler(orion::ProfileId profile_id)
{
    return MicrophonePermissionHandler::create(profile_id);
}

std::unique_ptr<PermissionHandler>
create_screen_sharing_permission_handler(orion::ProfileId profile_id)
{
    return ScreenSharingPermissionHandler::create(profile_id);
}

std::unique_ptr<PermissionHandler>
create_website_data_access_permission_handler()
{
    return WebsiteDataAccessPermissionHandler::create();
}

PermissionHandler*
PermissionHandler::get_static_permission_handler_for(WebKitPermissionRequest* request, orion::ProfileId profile_id)
{
    if (WEBKIT_IS_CLIPBOARD_PERMISSION_REQUEST(request)) {
        static auto instance = create_clipboard_permission_handler();
        return instance.get();
    } else if (WEBKIT_IS_DEVICE_INFO_PERMISSION_REQUEST(request)) {
        static auto instance = create_device_info_permission_handler();
        return instance.get();
    } else if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(request)) {
        static auto instance = create_geolocation_permission_handler();
        return instance.get();
    } else if (WEBKIT_IS_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(request)) {
        static auto instance = create_media_key_system_permission_handler();
        return instance.get();
    } else if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(request)) {
        static auto instance = create_notification_permission_handler();
        return instance.get();
    } else if (WEBKIT_IS_POINTER_LOCK_PERMISSION_REQUEST(request)) {
        static auto instance = create_pointer_lock_permission_handler();
        return instance.get();
    } else if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(request)) {
        WebKitUserMediaPermissionRequest* user_media_request = WEBKIT_USER_MEDIA_PERMISSION_REQUEST(request);
        if (webkit_user_media_permission_is_for_video_device(user_media_request)) {
            // Camera permission request
            static auto instance = create_camera_permission_handler(profile_id);
            return instance.get();
        } else if (webkit_user_media_permission_is_for_audio_device(user_media_request)) {
            // Microphone permission request
            static auto instance = create_microphone_permission_handler(profile_id);
            return instance.get();
        } else if (webkit_user_media_permission_is_for_display_device(user_media_request)) {
            // Display capture permission request
            static auto instance = create_screen_sharing_permission_handler(profile_id);
            return instance.get();
        }
        static auto instance = create_user_media_permission_handler();
        return instance.get();
    } else if (WEBKIT_IS_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(request)) {
        static auto instance = create_website_data_access_permission_handler();
        return instance.get();
    } else {
        orion::log_warning("get_static_permission_handler_for: unsupported permission request type");
        return nullptr;
    }
   
    /*
        static std::unordered_map<int, std::unique_ptr<PermissionHandler>> factory{
         {0 , NotificationPermissionHandler::create() },
         //{1 , GeolocationPermissionHandler::create() }
    };
    return factory[permisionId].get();
    */
}

/*
    if (WEBKIT_IS_CLIPBOARD_PERMISSION_REQUEST(request)) {
        
    } else if (WEBKIT_IS_DEVICE_INFO_PERMISSION_REQUEST(request)) {

    } else if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(request)) {

    } else if (WEBKIT_IS_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(request)) {

    } else if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(request)) {

    } else if (WEBKIT_IS_POINTER_LOCK_PERMISSION_REQUEST(request)) {

    } else if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(request)) {

    } else if (WEBKIT_IS_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(request)) {

    } else {

    }
*/

#endif