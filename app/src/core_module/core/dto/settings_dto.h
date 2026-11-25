#pragma once

#include <string>
#include <vector>
#include <common/src/error.h>
#include <common/src/dto/dto_member.h>
#include <core/dto/profile_dto.h>
#include <common/src/strong_types.h>

namespace orion {

struct SetSettingQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(key, std::string);       // for example: "settings::font::size"
    DTO_MEMBER(realm, std::string);     // for example: "settings::font"
    DTO_MEMBER(key_name, std::string);  // for example: "size"
    DTO_MEMBER(type, std::string);
    DTO_MEMBER(value, std::string);
    DTO_MEMBER(default_value, std::string);
};

struct IncrementAndClampSettingQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(key, std::string);       // for example: "settings::font::size"
    DTO_MEMBER(realm, std::string);     // for example: "settings::font"
    DTO_MEMBER(key_name, std::string);  // for example: "size"
    DTO_MEMBER(by, int64_t);
    DTO_MEMBER(lower, int64_t);
    DTO_MEMBER(upper, int64_t);
    DTO_MEMBER(unset_value, int64_t);
    DTO_MEMBER(starting_value, int64_t);
    DTO_MEMBER(delete_if_eq_starting, bool);
    DTO_MEMBER(default_value, int64_t);
};

struct AppendToStringSettingQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(key, std::string);       // for example: "settings::font::size"
    DTO_MEMBER(realm, std::string);     // for example: "settings::font"
    DTO_MEMBER(key_name, std::string);  // for example: "size"
    DTO_MEMBER(value, std::string);
    DTO_MEMBER(default_value, std::string);
};

struct RemoveAllOccurrencesOfStringFromSettingQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(key, std::string);       // for example: "settings::font::size"
    DTO_MEMBER(realm, std::string);     // for example: "settings::font"
    DTO_MEMBER(key_name, std::string);  // for example: "size"
    DTO_MEMBER(substring, std::string);
    DTO_MEMBER(default_value, std::string);
};

struct SetCommonSettingQuery {
    DTO_MEMBER(key, std::string);       // for example: "settings::font::size"
    DTO_MEMBER(realm, std::string);     // for example: "settings::font"
    DTO_MEMBER(key_name, std::string);  // for example: "size"
    DTO_MEMBER(type, std::string);
    DTO_MEMBER(value, std::string);
    DTO_MEMBER(default_value, std::string);
};

struct GetSettingQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(key, std::string);       // for example: "settings::font::size"
    DTO_MEMBER(realm, std::string);     // for example: "settings::font"
    DTO_MEMBER(key_name, std::string);  // for example: "size"
    DTO_MEMBER(type, std::string);
    DTO_MEMBER(default_value, std::string);
};

struct GetSettingOrDefaultQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(key, std::string);
    DTO_MEMBER(realm, std::string);
    DTO_MEMBER(key_name, std::string);
    DTO_MEMBER(type, std::string);
    DTO_MEMBER(default_setting_key, std::string);
    DTO_MEMBER(default_setting_realm, std::string);
    DTO_MEMBER(default_setting_key_name, std::string);
    DTO_MEMBER(default_setting_type, std::string);
    DTO_MEMBER(default_value, std::string);
};

struct SetSettingOrDefaultQuery {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(key, std::string);
    DTO_MEMBER(realm, std::string);
    DTO_MEMBER(key_name, std::string);
    DTO_MEMBER(type, std::string);
    DTO_MEMBER(value, std::string);
    DTO_MEMBER(default_setting_key, std::string);
    DTO_MEMBER(default_setting_realm, std::string);
    DTO_MEMBER(default_setting_key_name, std::string);
    DTO_MEMBER(default_setting_type, std::string);
    DTO_MEMBER(default_value, std::string);
};

struct GetCommonSettingQuery {
    DTO_MEMBER(key, std::string);       // for example: "settings::font::size"
    DTO_MEMBER(realm, std::string);     // for example: "settings::font"
    DTO_MEMBER(key_name, std::string);  // for example: "size"
    DTO_MEMBER(type, std::string);
    DTO_MEMBER(default_value, std::string);
};

struct GetAllValuesForWebsiteSettingQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(key_name, std::string);
};

struct RawSettingDto {
    DTO_MEMBER(key, std::string);
    DTO_MEMBER(type, std::string);
    DTO_MEMBER(value, std::string);
};

struct GetSettingsByKeyPrefixReply {
    DTO_MEMBER(settings, std::vector<RawSettingDto>);
    DTO_MEMBER(err, Error);
};
struct GetAllValuesForWebsiteSettingReply {
    DTO_MEMBER(settings, std::vector<RawSettingDto>);
    DTO_MEMBER(err, Error);
};

struct SingleSettingAnyChange {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(key, std::string);
    DTO_MEMBER(realm, std::string);
    DTO_MEMBER(key_name, std::string);
    DTO_MEMBER(type, std::string);
    DTO_MEMBER(default_value, std::string);
    DTO_MEMBER(old_value, std::string);
    DTO_MEMBER(new_value, std::string);
};

struct SingleCommonSettingAnyChange {
    DTO_MEMBER(key, std::string);
    DTO_MEMBER(realm, std::string);
    DTO_MEMBER(key_name, std::string);
    DTO_MEMBER(type, std::string);
    DTO_MEMBER(default_value, std::string);
    DTO_MEMBER(old_value, std::string);
    DTO_MEMBER(new_value, std::string);
};

template <typename T>
struct SingleSettingChange {
    DTO_MEMBER(profile_id, ProfileId, ProfileId(0));
    DTO_MEMBER(key, std::string);
    DTO_MEMBER(realm, std::string);
    DTO_MEMBER(key_name, std::string);
    DTO_MEMBER(type, std::string);
    DTO_MEMBER(default_value, T);
    DTO_MEMBER(old_value, T);
    DTO_MEMBER(new_value, T);
};

struct SettingsChange {
    DTO_MEMBER(updated, std::vector<SingleSettingAnyChange>);
};

struct CommonSettingsChange {
    DTO_MEMBER(updated, std::vector<SingleCommonSettingAnyChange>);
};

struct GetSettingForWebsiteOrDefaultQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(website, std::string);
    DTO_MEMBER(key_name, std::string);
};

struct GetSettingForWebsiteOrDefaultReply {
    DTO_MEMBER(err, Error);
    DTO_MEMBER(value, std::string);
};

struct ResetWebsiteSettings {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(website, std::string);
};

struct DeleteSettingForWebsiteQuery {
    DTO_MEMBER(profile_id, ProfileId);
    DTO_MEMBER(website, std::string);
    DTO_MEMBER(key_name, std::string);
};

} //namespace orion

namespace orion::settings {

enum class AppOpensWith {
    not_set                                   = 0,
    new_window                                = 1,
    new_private_window                        = 2,
    all_windows_from_last_session             = 3,
    all_non_private_windows_from_last_session = 4,
};

enum class NewWindowsOpenWith {
    not_set    = 0,
    start_page = 1,
    empty_page = 2,
    same_page  = 3,
    homepage   = 4,
    notes      = 5,
};

enum class NewTabsOpenWith {
    not_set    = 0,
    start_page = 1,
    empty_page = 2,
    same_page  = 3,
    homepage   = 4,
    notes      = 5,
};

enum class FileDownloadLocation {
    not_set               = 0,
    downloads             = 1,
    ask_for_each_download = 2,
    custom_path           = 3,
};

enum class RemoveDownloadItemsMode {
    not_set                  = 0,
    after_1_day              = 1,
    after_exit               = 2,
    upon_successful_download = 3,
    manually                 = 4,
};

enum class OpenExternalLinksIn {
    not_set             = 0,
    last_active_profile = 1,
    default_profile     = 2,
};

enum class Theme {
    not_set = 0,
    system  = 1,
    light   = 2,
    dark    = 3,
};

enum class BookmarksBarStyle {
    not_set       = 0,
    text_only     = 1,
    icon_and_text = 2,
    icon_only     = 3,
};

enum class WindowListButtonStyle {
    not_set       = 0,
    icon_only     = 1,
    text_only     = 2,
    icon_and_text = 3,
};

enum class TabLayout {
    not_set  = 0,
    standard = 1,
    compact  = 2,
    vertical = 3,
};

enum class TabSwitchOrder {
    not_set                  = 0,
    tab_order_in_window      = 1,
    most_recently_used_order = 2,
};

enum class PasswordProvider {
    not_set              = 0,
    orion_keychain       = 1,
    third_party_provider = 2,
    none                 = 3,
};

enum class RemoveTrackersFromUrlsMode {
    not_set                   = 0,
    for_private_browsing_only = 1,
    for_all_browsing          = 2,
};

enum class RemoveHistoryItemsMode {
    not_set         = 0,
    after_1_day     = 1,
    after_1_week    = 2,
    after_2_weeks   = 3,
    after_1_month   = 4,
    after_3_months  = 5,
    after_6_months  = 6,
    after_1_year    = 7,
    after_app_exits = 8,
    manually        = 9,
};

enum class DeleteCookiesMode {
    not_set         = 0,
    after_1_day     = 1,
    after_1_week    = 2,
    after_2_weeks   = 3,
    after_1_month   = 4,
    after_3_months  = 5,
    after_6_months  = 6,
    after_1_year    = 7,
    after_app_exits = 8,
    manually        = 9,
};

enum class ShareCrashReports {
    not_set                   = 0,
    after_asking_for_approval = 1,
    always_without_asking     = 2,
    never                     = 3,
};

namespace reset_orion {

enum class OlderThan {
    not_set   = 0,
    all_time  = 1,
    one_hour  = 2,
    one_day   = 3,
    one_week  = 4,
    one_month = 5,
};

} // namespace reset_orion

enum class ReaderModeWebsiteSetting {
    not_set = 0,
    on      = 1,
    off     = 2,
};

enum class AutoplayWebsiteSetting {
    not_set               = 0,
    allow_all_autoplay    = 1,
    stop_media_with_sound = 2,
    never_autoplay        = 3,
};

enum class PopupWindowsWebsiteSetting {
    not_set          = 0,
    allow            = 1,
    block            = 2,
    block_and_notify = 3,
};

enum class ClipboardWebsiteSetting{
    not_set    = 0,
    ask        = 1,
    allow      = 2,
    dont_allow = 3,
};

enum class DeviceInfoWebsiteSetting{
    not_set    = 0,
    ask        = 1,
    allow      = 2,
    dont_allow = 3,
};

enum class MediaKeySystemWebsiteSetting{
    not_set    = 0,
    ask        = 1,
    allow      = 2,
    dont_allow = 3,
};

enum class PointerLockWebsiteSetting{
    not_set    = 0,
    ask        = 1,
    allow      = 2,
    dont_allow = 3,
};

enum class WebsiteDataAccessWebsiteSetting{
    not_set    = 0,
    ask        = 1,
    allow      = 2,
    dont_allow = 3,
};

enum class NotificationsWebsiteSetting {
    not_set    = 0,
    ask        = 1,
    allow      = 2,
    dont_allow = 3,
};

enum class DownloadsWebsiteSetting {
    not_set    = 0,
    ask        = 1,
    deny       = 2,
    allow      = 3,
};

enum class CameraWebsiteSetting {
    not_set    = 0,
    ask        = 1,
    deny       = 2,
    allow      = 3,
};

enum class MicrophoneWebsiteSetting {
    not_set    = 0,
    ask        = 1,
    deny       = 2,
    allow      = 3,
};

enum class ScreenSharingWebsiteSetting {
    not_set    = 0,
    ask        = 1,
    deny       = 2,
    allow      = 3,
};

enum class LocationWebsiteSetting {
    not_set    = 0,
    ask        = 1,
    deny       = 2,
    allow      = 3,
};

enum class ContentBlockersWebsiteSetting {
    not_set = 0,
    on      = 1,
    off     = 2,
};

enum class TrackingPreventionWebsiteSetting {
    not_set = 0,
    on      = 1,
    off     = 2,
};

enum class CookiesWebsiteSetting {
    not_set = 0,
    on      = 1,
    off     = 2,
};

enum class JavaScriptWebsiteSetting {
    not_set = 0,
    on      = 1,
    off     = 2,
};

enum class WebFontsWebsiteSetting {
    not_set = 0,
    on      = 1,
    off     = 2,
};

enum class SslCertificateValidationWebsiteSetting {
    not_set = 0,
    strict  = 1,
    ignore  = 2,
};

enum class UserAgentWebsiteSetting {
    not_set       = 0,
    automatic     = 1,
    custom        = 2,
    chrome        = 3,
    firefox       = 4,
    safari        = 5,
    safari_iphone = 6,
    safari_ipad   = 7,
    edge          = 8,
};

enum class CompatibilityModeWebsiteSetting {
    not_set = 0,
    on      = 1,
    off     = 2,
};

enum class ExternalAppsWebsiteSetting {
    not_set = 0,
    ask     = 1,
    deny    = 2,
    allow   = 3,
};

enum class PictureInPictureWebsiteSetting {
    not_set   = 0,
    automatic = 1,
    manual    = 2,
};

enum class ExtensionWebsiteSetting {
    not_set                       = 0,
    ask                           = 1,
    allow_for_1_day               = 2,
    always_allow_on_every_website = 3,
    deny_for_all_websites         = 4,
};

enum class SidebarPage {
    not_set      = 0,
    windows      = 1, // also known as Tab Groups (in newer versions of Orion)
    tabs         = 2,
    bookmarks    = 3,
    reading_list = 4,
};

enum class ProfilesButtonStyle {
    not_set            = 0,
    show_icon_only     = 1,
    show_name_only     = 2,
    show_icon_and_name = 3,
};

enum class Language {
    not_set             = 0,
    german              = 1,
    arabic              = 2,
    greek               = 3,
    chinese_simplified  = 4,
    japanese            = 5,
    english             = 6,
    ukrainian           = 7,
    spanish             = 8,
    slovenian           = 9,
    galician            = 10,
    italian             = 11,
    swedish             = 12,
    czech               = 13,
    chinese_traditional = 14,
    turkish             = 15,
    polish              = 16,
    portuguese_brazil   = 17,
    russian             = 18,
    french              = 19,
    finnish             = 20,
    dutch               = 21,
    polish_poland       = 22,
};

enum class SearchEngine {
    not_set    = 0,
    kagi       = 1,
    none       = 2,
    google     = 3,
    bing       = 4,
    yandex     = 5,
    duckduckgo = 6,
    startpage  = 7,
    brave      = 8,
    ecosia     = 9,
    yahoo      = 10,
    baidu      = 11,
    qwant      = 12,
    mojeek     = 13,
    you_com    = 14,
};

//linux ui is reliant on this being correctly ordered, sequential, starting from 1 for valid values
enum class PageZoom {
    not_set  = 0,
    zoom_50  = 1,
    zoom_75  = 2,
    zoom_80  = 3,
    zoom_85  = 4,
    zoom_90  = 5,
    zoom_95  = 6,
    zoom_100 = 7,
    zoom_115 = 8,
    zoom_125 = 9,
    zoom_150 = 10,
    zoom_175 = 11,
    zoom_200 = 12,
    zoom_250 = 13,
    zoom_300 = 14,
};

#define TEXT_ALIGNMENT(X) \
    X(not_set,  0) \
    X(left,     1) \
    X(justify,  2) \

enum class TextAlignment {
#define ENTRY(name, value) name = value,
    TEXT_ALIGNMENT(ENTRY)
#undef ENTRY
};

const std::unordered_map<TextAlignment, std::string> text_alingment_strings = {
#define ENTRY(name, value) {TextAlignment::name, #name},
    TEXT_ALIGNMENT(ENTRY)
#undef ENTRY
};

#undef TEXT_ALIGNMENT

#define FONT(X) \
    X(not_set, 0) \
    X(System, 1) \
    X(Merriweather, 2) \
    X(Charter, 3) \
    X(Gelasio, 4) \
    X(TeXGyrePagella, 5) \
    X(Ubuntu, 6) \
    X(Vollkorn, 7) \
    X(NewYork, 8) \
    X(LiberationSerif, 9) \
    X(Dyslexic, 10)

enum class Font {
#define ENTRY(name, value) name = value,
    FONT(ENTRY)
#undef ENTRY
};

#undef FONT

enum class ReaderModeBackgroundType {
    not_set = 0,
    white   = 1,
    sepia   = 2,
    gray    = 3,
    night   = 4,
};

enum class ReaderModeBorderShadowEnabled {
    not_set = 0,
    on      = 1,
    off     = 2,
};

enum class ToolbarItemStyle {
    not_set       = 0,
    icon_and_text = 1,
    icon_only     = 2,
};

} // namespace orion::settings
