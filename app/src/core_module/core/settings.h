#pragma once

#include <core/settings_impl.h>
#include <core/dto/settings_dto.h>
#include <common/src/strong_types.h>
#include <core/dto/bookmark_folder_dto.h>

namespace orion {

namespace settings {

Error get_isetting_for_setting_key(std::string key, const settings::ISetting** out_setting);

}

SETTING_REALM(settings::general);
SETTING_VALUE(settings::general, app_opens_with, AppOpensWith, AppOpensWith::all_non_private_windows_from_last_session);
SETTING_VALUE(settings::general, new_windows_open_with, NewWindowsOpenWith, NewWindowsOpenWith::start_page);
SETTING_VALUE(settings::general, new_tabs_open_with, NewTabsOpenWith, NewTabsOpenWith::start_page);
SETTING_VALUE(settings::general, homepage, std::string, "https://kagi.com/");
SETTING_VALUE(settings::general, file_download_location, FileDownloadLocation, FileDownloadLocation::ask_for_each_download);
SETTING_VALUE(settings::general, file_download_custom_path, std::string, "");
SETTING_VALUE(settings::general, remove_download_items_mode, RemoveDownloadItemsMode, RemoveDownloadItemsMode::manually);
SETTING_VALUE(settings::general, open_safe_files_after_download, bool, false);
SETTING_VALUE(settings::general, language, Language, Language::english);
SETTING_VALUE(settings::general, open_external_links_in, OpenExternalLinksIn, OpenExternalLinksIn::last_active_profile);
SETTING_VALUE(settings::general, automatically_keep_app_up_to_date, bool, false);
SETTING_VALUE(settings::general, show_warning_before_quitting, bool, true);
SETTING_VALUE(settings::general, selected_settings_tab_index, int64_t, 0); // 0: General, 10: Plus
SETTING_VALUE(settings::general, low_power_mode, bool, false);

SETTING_REALM(settings::appearance);
SETTING_VALUE(settings::appearance, theme, Theme, Theme::system);
SETTING_VALUE(settings::appearance, tabs_title_bar_tint, std::string, "");
SETTING_VALUE(settings::appearance, show_website_color_for_compact_and_vertical_tabs, bool, false);
SETTING_VALUE(settings::appearance, always_show_toolbar_in_fullscreen, bool, true);
SETTING_VALUE(settings::appearance, show_bookmarks_bar, bool, true);
SETTING_VALUE(settings::appearance, show_profile_in_address_bar, bool, false); // obsolete
SETTING_VALUE(settings::appearance, display_rss_feeds_indicator, bool, true);
SETTING_VALUE(settings::appearance, display_web_application_indicator, bool, true);
SETTING_VALUE(settings::appearance, bookmarks_bar_style, BookmarksBarStyle, BookmarksBarStyle::text_only);
SETTING_VALUE(settings::appearance, auto_show_sidebar, bool, false);
SETTING_VALUE(settings::appearance, auto_show_sidebar_in_fullscreen, bool, false);
SETTING_VALUE(settings::appearance, default_font_family, std::string, "Times New Roman");
SETTING_VALUE(settings::appearance, default_font_size, FontSize, 16);
SETTING_VALUE(settings::appearance, monospaced_font_family, std::string, "Courier New");
SETTING_VALUE(settings::appearance, monospaced_font_size, FontSize, 13);
SETTING_VALUE(settings::appearance, prefer_page_rendering_updates_near_120fps, bool, false);
SETTING_VALUE(settings::appearance, disable_transparency_for_fullscreen_windows, bool, true);
SETTING_VALUE(settings::appearance, app_icon, std::string, "default_app_icon.png");
SETTING_VALUE(settings::appearance, show_sidebar, bool, false);
SETTING_VALUE(settings::appearance, sidebar_page, SidebarPage, SidebarPage::windows);
SETTING_VALUE(settings::appearance, profiles_button_style, ProfilesButtonStyle, ProfilesButtonStyle::show_icon_only);
SETTING_VALUE(settings::appearance, start_maximized, bool, false);
// Note: focus_mode is a per-window setting (not per-profile)
SETTING_VALUE(settings::appearance, show_status_bar_on_link_hover, bool, true);
SETTING_VALUE(settings::appearance, window_list_button_style, WindowListButtonStyle, WindowListButtonStyle::text_only);
SETTING_VALUE(settings::appearance, toolbar_item_style, ToolbarItemStyle, ToolbarItemStyle::icon_only);
SETTING_VALUE(settings::appearance, web_preview_width, int64_t, 800);
SETTING_VALUE(settings::appearance, web_preview_height, int64_t, 700);

SETTING_REALM(settings::tabs);
SETTING_VALUE(settings::tabs, tab_layout, TabLayout, TabLayout::standard);
SETTING_VALUE(settings::tabs, use_mini_toolbar, bool, false);
SETTING_VALUE(settings::tabs, show_tab_preview_on_hover, bool, true);
SETTING_VALUE(settings::tabs, close_window_with_last_tab, bool, true);
SETTING_VALUE(settings::tabs, always_show_website_titles_in_tabs, bool, true);
SETTING_VALUE(settings::tabs, show_favicons_inside_tabs, bool, true);
SETTING_VALUE(settings::tabs, open_new_tab_next_to_current_tab, bool, false);
SETTING_VALUE(settings::tabs, when_opening_link_in_new_tab_make_it_active, bool, false);
SETTING_VALUE(settings::tabs, return_to_last_active_tab_when_closing_current_tab, bool, true);
SETTING_VALUE(settings::tabs, use_ctrl_z_to_undo_close_tab, bool, true);
SETTING_VALUE(settings::tabs, use_ctrl_1_to_9_to_switch_tabs, bool, true);
SETTING_VALUE(settings::tabs, show_nested_vertical_tabs, bool, false);
SETTING_VALUE(settings::tabs, use_tab_switcher_ui_for_ctrl_tab, bool, true);
SETTING_VALUE(settings::tabs, tab_switch_order, TabSwitchOrder, TabSwitchOrder::tab_order_in_window);

SETTING_REALM(settings::browsing);
SETTING_VALUE(settings::browsing, automatic_https_upgrade, bool, true);
SETTING_VALUE(settings::browsing, show_full_website_address, bool, true);
SETTING_VALUE(settings::browsing, open_bookmarks_in_new_tabs, bool, false);
SETTING_VALUE(settings::browsing, switch_saved_windows_in_place, bool, true);
SETTING_VALUE(settings::browsing, open_external_links_in_preview, bool, true);
SETTING_VALUE(settings::browsing, show_standard_window_buttons_in_focus_mode, bool, true);
SETTING_VALUE(settings::browsing, confirm_closing_tabs_when_picture_in_picture_is_video_is_playing, bool, true);
SETTING_VALUE(settings::browsing, check_spelling_while_typing, bool, true);
SETTING_VALUE(settings::browsing, correct_spelling_automatically, bool, false);
SETTING_VALUE(settings::browsing, enable_minimum_font_size, bool, true);
SETTING_VALUE(settings::browsing, minimum_font_size, FontSize, 9);
SETTING_VALUE(settings::browsing, press_tab_to_highlight_each_item_on_web_page, bool, false);
SETTING_VALUE(settings::browsing, prevent_escape_from_exiting_fullscreen, bool, false);

SETTING_REALM(settings::sync);
SETTING_VALUE(settings::sync, enable_cross_device_sync_of_tabs_bookmarks_and_reading_list, bool, true);

SETTING_REALM(settings::passwords);
SETTING_VALUE(settings::passwords, password_provider, PasswordProvider, PasswordProvider::orion_keychain);
SETTING_VALUE(settings::passwords, offer_to_autofill_passwords_from_orion_keychain, bool, true);
SETTING_VALUE(settings::passwords, offer_to_save_passwords_to_orion_keychain, bool, true);
SETTING_VALUE(settings::passwords, submit_form_automatically, bool, true);
SETTING_VALUE(settings::passwords, use_touch_id, bool, true); // probably Apple-specific

SETTING_REALM(settings::privacy);
SETTING_VALUE(settings::privacy, remove_trackers_from_urls_mode, RemoveTrackersFromUrlsMode, RemoveTrackersFromUrlsMode::for_private_browsing_only);
SETTING_VALUE(settings::privacy, remove_history_items_mode, RemoveHistoryItemsMode, RemoveHistoryItemsMode::manually);
SETTING_VALUE(settings::privacy, delete_cookies_mode, DeleteCookiesMode, DeleteCookiesMode::manually);
SETTING_VALUE(settings::privacy, disable_history, bool, false);
SETTING_VALUE(settings::privacy, auto_update_content_blockers, bool, false);
SETTING_VALUE(settings::privacy, share_crash_reports, ShareCrashReports, ShareCrashReports::after_asking_for_approval);
SETTING_VALUE(settings::privacy, custom_user_agent, std::string, "Mozilla/5.0 (X11; Linux x86_64; rv:149.0) Gecko/20100101 Firefox/149.0");

SETTING_REALM(settings::search);
SETTING_VALUE(settings::search, search_engine, SearchEngine, SearchEngine::kagi);
SETTING_VALUE(settings::search, also_use_search_engine_in_private_browsing, bool, false);
SETTING_VALUE(settings::search, private_browsing_search_engine, SearchEngine, SearchEngine::none);
SETTING_VALUE(settings::search, autocomplete_top_hits, bool, true);
SETTING_VALUE(settings::search, autocomplete_search_engines, bool, true);
SETTING_VALUE(settings::search, autocomplete_ask_fastgpt, bool, true);
SETTING_VALUE(settings::search, autocomplete_history, bool, true);
SETTING_VALUE(settings::search, autocomplete_bookmarks, bool, true);
SETTING_VALUE(settings::search, autocomplete_open_tabs, bool, true);
// custom search engines should be implemented as proper database entities

SETTING_REALM(settings::websites);
SETTING_VALUE(settings::websites, enable_reader_mode_default_setting_for_all_websites, ReaderModeWebsiteSetting, ReaderModeWebsiteSetting::off);
SETTING_VALUE(settings::websites, autoplay_default_setting_for_all_websites, AutoplayWebsiteSetting, AutoplayWebsiteSetting::stop_media_with_sound);
SETTING_VALUE(settings::websites, page_zoom_default_setting_for_all_websites, PageZoom, PageZoom::zoom_100);
SETTING_VALUE(settings::websites, popup_windows_default_setting_for_all_websites, PopupWindowsWebsiteSetting, PopupWindowsWebsiteSetting::block);
SETTING_VALUE(settings::websites, notifications_default_setting_for_all_websites, NotificationsWebsiteSetting, NotificationsWebsiteSetting::ask);
SETTING_VALUE(settings::websites, downloads_default_setting_for_all_websites, DownloadsWebsiteSetting, DownloadsWebsiteSetting::ask);
SETTING_VALUE(settings::websites, camera_default_setting_for_all_websites, CameraWebsiteSetting, CameraWebsiteSetting::ask);
SETTING_VALUE(settings::websites, microphone_default_setting_for_all_websites, MicrophoneWebsiteSetting, MicrophoneWebsiteSetting::ask);
SETTING_VALUE(settings::websites, screen_sharing_default_setting_for_all_websites, ScreenSharingWebsiteSetting, ScreenSharingWebsiteSetting::ask);
SETTING_VALUE(settings::websites, location_default_setting_for_all_websites, LocationWebsiteSetting, LocationWebsiteSetting::ask);
SETTING_VALUE(settings::websites, content_blockers_setting_for_all_websites, ContentBlockersWebsiteSetting, ContentBlockersWebsiteSetting::on);
SETTING_VALUE(settings::websites, tracking_prevention_setting_for_all_websites, TrackingPreventionWebsiteSetting, TrackingPreventionWebsiteSetting::on);
SETTING_VALUE(settings::websites, cookies_setting_for_all_websites, CookiesWebsiteSetting, CookiesWebsiteSetting::on);
SETTING_VALUE(settings::websites, javascript_setting_for_all_websites, JavaScriptWebsiteSetting, JavaScriptWebsiteSetting::on);
SETTING_VALUE(settings::websites, web_fonts_setting_for_all_websites, WebFontsWebsiteSetting, WebFontsWebsiteSetting::on);
SETTING_VALUE(settings::websites, ssl_certificate_validation_setting_for_all_websites, SslCertificateValidationWebsiteSetting, SslCertificateValidationWebsiteSetting::strict);
SETTING_VALUE(settings::websites, user_agent_default_setting_for_all_websites, UserAgentWebsiteSetting, UserAgentWebsiteSetting::automatic);
SETTING_VALUE(settings::websites, compatibility_mode_default_setting_for_all_websites, CompatibilityModeWebsiteSetting, CompatibilityModeWebsiteSetting::off);
SETTING_VALUE(settings::websites, external_apps_default_setting_for_all_websites, ExternalAppsWebsiteSetting, ExternalAppsWebsiteSetting::ask);
SETTING_VALUE(settings::websites, picture_in_picture_default_setting_for_all_websites, PictureInPictureWebsiteSetting, PictureInPictureWebsiteSetting::automatic);
//TODO determine whats the default value for the following
SETTING_VALUE(settings::websites, clipboard_default_setting_for_all_websites, ClipboardWebsiteSetting, ClipboardWebsiteSetting::ask);
SETTING_VALUE(settings::websites, device_info_default_setting_for_all_websites, DeviceInfoWebsiteSetting, DeviceInfoWebsiteSetting::ask);
SETTING_VALUE(settings::websites, media_key_system_default_setting_for_all_websites, MediaKeySystemWebsiteSetting, MediaKeySystemWebsiteSetting::ask);
SETTING_VALUE(settings::websites, pointer_lock_default_setting_for_all_websites, PointerLockWebsiteSetting, PointerLockWebsiteSetting::ask);
SETTING_VALUE(settings::websites, website_data_access_default_setting_for_all_websites, WebsiteDataAccessWebsiteSetting, WebsiteDataAccessWebsiteSetting::ask);
SETTING_VALUE(settings::websites, page_tweaks_default_setting_for_all_websites, bool, true); // this cannot be user-configured
SETTING_VALUE(settings::websites, page_tweaks_font_size_default_setting_for_all_websites, PageZoom, PageZoom::not_set); // this cannot be user-configured
SETTING_VALUE(settings::websites, page_tweaks_force_dark_mode_default_setting_for_all_websites, bool, false); // this cannot be user-configured
SETTING_VALUE(settings::websites, page_tweaks_font_family_default_setting_for_all_websites, std::string, ""); // this cannot be user-configured
SETTING_VALUE(settings::websites, page_tweaks_disable_sticky_default_setting_for_all_websites, bool, false); // this cannot be user-configured
SETTING_VALUE(settings::websites, page_tweaks_element_blocker_list_default_setting_for_all_websites, std::string, ""); // this cannot be user-configured

SETTING_REALM(settings::dialogs::add_bookmark);

// True means that Reading List item should be selected by default in the dialog, false
// means that a bookmark folder should be selected by default and its ID is stored in
// settings::dialogs::add_bookmark::default_bookmark_folder_id.
SETTING_VALUE(settings::dialogs::add_bookmark, is_reading_list, bool, false);
SETTING_VALUE(settings::dialogs::add_bookmark, default_bookmark_folder_id, BookmarkFolderId, BookmarkFolderId(0)); // zero means root level

// extension settings should be implemented as proper database entities

//Website specific settings
SETTING_REALM(settings::website::specific);
SETTING_VALUE(settings::website::specific, autoplay_default_setting, AutoplayWebsiteSetting, AutoplayWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, popup_windows_default_setting, PopupWindowsWebsiteSetting, PopupWindowsWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, clipboard_default_setting, ClipboardWebsiteSetting, ClipboardWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, device_info_default_setting, DeviceInfoWebsiteSetting, DeviceInfoWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, downloads_default_setting, DownloadsWebsiteSetting, DownloadsWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, camera_default_setting, CameraWebsiteSetting, CameraWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, microphone_default_setting, MicrophoneWebsiteSetting, MicrophoneWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, screen_sharing_default_setting, ScreenSharingWebsiteSetting, ScreenSharingWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, location_default_setting, LocationWebsiteSetting, LocationWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, media_key_system_default_setting, MediaKeySystemWebsiteSetting, MediaKeySystemWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, notifications_default_setting, NotificationsWebsiteSetting, NotificationsWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, pointer_lock_default_setting, PointerLockWebsiteSetting, PointerLockWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, website_data_access_default_setting, WebsiteDataAccessWebsiteSetting, WebsiteDataAccessWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, ssl_certificate_validation_setting, SslCertificateValidationWebsiteSetting, SslCertificateValidationWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, page_zoom, PageZoom, PageZoom::zoom_100);
SETTING_VALUE(settings::website::specific, user_agent, UserAgentWebsiteSetting, UserAgentWebsiteSetting::automatic);
SETTING_VALUE(settings::website::specific, external_apps_default_setting, ExternalAppsWebsiteSetting, ExternalAppsWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, picture_in_picture_default_setting, PictureInPictureWebsiteSetting, PictureInPictureWebsiteSetting::not_set);
SETTING_VALUE(settings::website::specific, enable_content_blockers, bool, true);
SETTING_VALUE(settings::website::specific, enable_intelligent_tracking_prevention, bool, true);
SETTING_VALUE(settings::website::specific, block_web_fonts, bool, false);
SETTING_VALUE(settings::website::specific, enable_javascript, bool, true);
SETTING_VALUE(settings::website::specific, enable_cookies, bool, true);
SETTING_VALUE(settings::website::specific, enable_compatibility_mode, bool, false);
SETTING_VALUE(settings::website::specific, enable_page_tweaks, bool, true);
SETTING_VALUE(settings::website::specific, page_tweaks_font_size , PageZoom, PageZoom::not_set);
SETTING_VALUE(settings::website::specific, page_tweaks_force_dark_mode, bool, false);
SETTING_VALUE(settings::website::specific, page_tweaks_font_family, std::string, "");
SETTING_VALUE(settings::website::specific, page_tweaks_disable_sticky, bool, false); // this cannot be user-configured
SETTING_VALUE(settings::website::specific, page_tweaks_element_blocker_list, std::string, "");
SETTING_VALUE(settings::website::specific, enable_reader_mode, ReaderModeWebsiteSetting, ReaderModeWebsiteSetting::off);
// Note: When adding settings::website::specific setting, add corresponding default settings::websites default setting,
// and add the setting to find_website_default_setting_info() in profiles_db/setting.cpp

SETTING_REALM(settings::advanced);
SETTING_VALUE(settings::advanced, enable_json_formatting, bool, true);
SETTING_VALUE(settings::advanced, allow_installation_of_3rd_party_chrome_extensions, bool, false);
SETTING_VALUE(settings::advanced, allow_installation_of_3rd_party_firefox_extensions, bool, false);

SETTING_REALM(settings::reset_orion);
SETTING_VALUE(settings::reset_orion, clear_history, bool, true);
SETTING_VALUE(settings::reset_orion, remove_bookmarks, bool, true);
SETTING_VALUE(settings::reset_orion, remove_reading_list, bool, true);
SETTING_VALUE(settings::reset_orion, remove_snapshots, bool, true);
SETTING_VALUE(settings::reset_orion, remove_downloads, bool, true);
SETTING_VALUE(settings::reset_orion, remove_icloud_app_data, bool, true); // Apple-specific
SETTING_VALUE(settings::reset_orion, older_than, reset_orion::OlderThan, reset_orion::OlderThan::one_month);
SETTING_VALUE(settings::reset_orion, reset_all_settings, bool, true);
SETTING_VALUE(settings::reset_orion, reset_all_website_settings, bool, true);
SETTING_VALUE(settings::reset_orion, remove_all_cookies, bool, true);
SETTING_VALUE(settings::reset_orion, empty_browser_cache, bool, true);
SETTING_VALUE(settings::reset_orion, empty_local_storage_and_databases, bool, true);
SETTING_VALUE(settings::reset_orion, remove_saved_passwords, bool, true);
SETTING_VALUE(settings::reset_orion, remove_all_extensions, bool, true);
SETTING_VALUE(settings::reset_orion, remove_all_page_tweaks, bool, true);
SETTING_VALUE(settings::reset_orion, close_all_windows, bool, true);

SETTING_REALM(settings::filter_lists);
SETTING_VALUE(settings::filter_lists, easy_list, bool, true);
SETTING_VALUE(settings::filter_lists, easy_privacy, bool, true);
SETTING_VALUE(settings::filter_lists, hagezi_light_dns_blocklist, bool, false);
SETTING_VALUE(settings::filter_lists, fanboys_annoyance, bool, false);
SETTING_VALUE(settings::filter_lists, hagezi_pro_plus_minimal, bool, false);
SETTING_VALUE(settings::filter_lists, privacy_essentials, bool, false);
SETTING_VALUE(settings::filter_lists, block_third_party_fonts, bool, false);
SETTING_VALUE(settings::filter_lists, enable_custom_filters, bool, true);
SETTING_VALUE(settings::filter_lists, custom_filters_text, std::string, "reddit.com##.promotedlink");
// custom filter list URLs should be implemented as proper database entities

SETTING_REALM(settings::windows);
SETTING_VALUE(settings::windows, show_recently_opened_on_top, bool, false);
SETTING_VALUE(settings::windows, switch_saved_windows_in_place, bool, true);

// Reader Mode
SETTING_REALM(settings::reader_mode);
SETTING_VALUE(settings::reader_mode, font_size_index, int64_t, 0);
SETTING_VALUE(settings::reader_mode, font_name, Font, Font::not_set);
SETTING_VALUE(settings::reader_mode, text_alignment, TextAlignment, TextAlignment::not_set);
SETTING_VALUE(settings::reader_mode, background_type, ReaderModeBackgroundType, ReaderModeBackgroundType::not_set);
SETTING_VALUE(settings::reader_mode, border_shadow_enabled, ReaderModeBorderShadowEnabled, ReaderModeBorderShadowEnabled::not_set);
SETTING_VALUE(settings::reader_mode, document_width, int64_t, 800);


// TEST:
SETTING_REALM(settings::test);
SETTING_VALUE(settings::test, language, std::string, "en-US");
SETTING_VALUE(settings::test, show_line_numbers, bool, true);
SETTING_VALUE(settings::test, progress, double, 0.0);
SETTING_VALUE(settings::test, updated_at, UnixTimeUtc64, 1735689600); // 2025-01-01 00:00:00 UTC
SETTING_VALUE(settings::test, font_size, FontSize, 16);

// TEST:
SETTING_REALM(settings::test::subrealm);
SETTING_VALUE(settings::test::subrealm, size, int64_t, 14);
SETTING_VALUE(settings::test::subrealm, family, std::string, "Arial");

// Common Settings (system-wide, not per-profile)

SETTING_REALM(settings::common::onboarding);
SETTING_VALUE(settings::common::onboarding, kagi_client_id, std::string, "");
SETTING_VALUE(settings::common::onboarding, is_onboarded, bool, false);
SETTING_VALUE(settings::common::onboarding, token_key, std::string, "");

} // namespace orion
