#include <core/settings.h>
#include <common/src/defer.h>

#include "check_button_settings.h"
#include "app.h"
#include "window/main_window/main_window.h"

#define widget_get_profile_id(widget) main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(widget))))

#define on_check_button_map_define(widget, setting) \
void \
on_##widget##_map(GtkCheckButton* button, gpointer user_data) \
{ \
    orion::ProfileId profile_id = widget_get_profile_id(button); \
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(button); \
    global_core_api->get_setting(profile_id, setting, \
        [wrapped_ref](orion::GetSettingReply<bool> reply){ \
            VERIFY_LOG(reply.err); \
            GtkCheckButton* button = GTK_CHECK_BUTTON(wrapped_ref->get()); \
            if(button) \
            { \
                gtk_check_button_set_active(button, reply.value); \
                g_object_unref(button); \
            } \
        }); \
} \

#define on_check_button_toggled_define(widget, setting) \
void \
on_##widget##_toggled(GtkCheckButton* button, gpointer user_data) \
{ \
    orion::ProfileId profile_id = widget_get_profile_id(button); \
    bool val = gtk_check_button_get_active(button); \
    global_core_api->set_setting(profile_id, setting, val, \
        [](orion::SetSettingReply<bool> reply){VERIFY_LOG(reply.err);}); \
} \

#define on_check_button_map_define_on_default_profile(widget, setting) \
void \
on_##widget##_map_on_default_profile(GtkCheckButton* button, gpointer user_data) \
{ \
    orion::gtk::WeakRefWrapper button_weak(button); \
    global_core_api->get_default_profile_id([button_weak](orion::GetDefaultProfileIdReply reply){ \
        VERIFY_LOG(reply.err); \
        orion::ProfileId profile_id = reply.profile_id; \
        global_core_api->get_setting(profile_id, setting, \
            [button_weak](orion::GetSettingReply<bool> reply){ \
                VERIFY_LOG(reply.err); \
                auto button_strong = button_weak.lock(); \
                if (!button_strong) return; \
                GtkCheckButton* button = GTK_CHECK_BUTTON(button_strong.get()); \
                gtk_check_button_set_active(button, reply.value); \
            }); \
    }); \
}

#define on_check_button_toggled_define_on_default_profile(widget, setting) \
void \
on_##widget##_toggled_on_default_profile(GtkCheckButton* button, gpointer user_data) \
{ \
    orion::gtk::WeakRefWrapper button_weak(button); \
    global_core_api->get_default_profile_id([button_weak](orion::GetDefaultProfileIdReply reply){ \
        VERIFY_LOG(reply.err); \
        auto button_strong = button_weak.lock(); \
        if (!button_strong) return; \
        GtkCheckButton* button = GTK_CHECK_BUTTON(button_strong.get()); \
        orion::ProfileId profile_id = reply.profile_id; \
        bool val = gtk_check_button_get_active(button); \
        global_core_api->set_setting(profile_id, setting, val, \
            [](orion::SetSettingReply<bool> reply){VERIFY_LOG(reply.err);}); \
    }); \
}

#define check_button_funcs_define(widget, setting) \
on_check_button_map_define(widget, setting) \
on_check_button_toggled_define(widget, setting)

#define check_button_funcs_define_on_default_profile(widget, setting) \
on_check_button_map_define_on_default_profile(widget, setting) \
on_check_button_toggled_define_on_default_profile(widget, setting)

check_button_funcs_define(open_safe_after_download_check_button, orion::settings::general::open_safe_files_after_download)
check_button_funcs_define(automatically_keep_up_to_date_check_button, orion::settings::general::automatically_keep_app_up_to_date)
check_button_funcs_define(show_warning_before_quit_check_button, orion::settings::general::show_warning_before_quitting)

check_button_funcs_define(show_website_color_check_button, orion::settings::appearance::show_website_color_for_compact_and_vertical_tabs)
check_button_funcs_define(always_show_toolbar_in_fs_check_button, orion::settings::appearance::always_show_toolbar_in_fullscreen)
check_button_funcs_define(show_bookmarks_bar_check_button, orion::settings::appearance::show_bookmarks_bar)
check_button_funcs_define(show_profile_in_address_bar_check_button, orion::settings::appearance::show_profile_in_address_bar)
check_button_funcs_define(display_rss_feeds_indicator_check_button, orion::settings::appearance::display_rss_feeds_indicator)
check_button_funcs_define(display_web_app_indicator_check_button, orion::settings::appearance::display_web_application_indicator)
check_button_funcs_define(auto_show_sidebar_in_fs_check_button, orion::settings::appearance::auto_show_sidebar_in_fullscreen)
check_button_funcs_define(prefer_fast_rendering_page_updates_check_button, orion::settings::appearance::prefer_page_rendering_updates_near_120fps)
check_button_funcs_define(disable_transparency_for_fs_windows_check_button, orion::settings::appearance::disable_transparency_for_fullscreen_windows)

check_button_funcs_define(use_mini_toolbar_check_button, orion::settings::tabs::use_mini_toolbar)
check_button_funcs_define(show_tab_preview_on_hover_check_button, orion::settings::tabs::show_tab_preview_on_hover)
check_button_funcs_define(close_window_with_last_tab_check_button, orion::settings::tabs::close_window_with_last_tab)
check_button_funcs_define(always_show_website_titles_in_tabs_check_button, orion::settings::tabs::always_show_website_titles_in_tabs)
check_button_funcs_define(show_favicons_in_tabs_check_button, orion::settings::tabs::show_favicons_inside_tabs)
check_button_funcs_define(open_new_tab_next_to_current_check_button, orion::settings::tabs::open_new_tab_next_to_current_tab)
check_button_funcs_define(make_newly_opened_link_tab_active_check_button, orion::settings::tabs::when_opening_link_in_new_tab_make_it_active)
check_button_funcs_define(return_to_last_active_tab_when_closing_current_check_button, orion::settings::tabs::return_to_last_active_tab_when_closing_current_tab)
check_button_funcs_define_on_default_profile(use_ctrl_z_to_undo_close_tab_check_button, orion::settings::tabs::use_ctrl_z_to_undo_close_tab)
check_button_funcs_define(use_ctrl_num_to_switch_tabs_check_button, orion::settings::tabs::use_ctrl_1_to_9_to_switch_tabs)
check_button_funcs_define(show_nested_vertical_tabs_check_button, orion::settings::tabs::show_nested_vertical_tabs)
check_button_funcs_define(use_tab_switcher_ui_for_ctrl_tab_check_button, orion::settings::tabs::use_tab_switcher_ui_for_ctrl_tab)

check_button_funcs_define(automatic_https_upgrade_check_button, orion::settings::browsing::automatic_https_upgrade)
check_button_funcs_define(show_full_website_address_check_button, orion::settings::browsing::show_full_website_address)
check_button_funcs_define(open_bookmarks_in_new_tabs_check_button, orion::settings::browsing::open_bookmarks_in_new_tabs)
check_button_funcs_define(switch_saved_windows_in_place_check_button, orion::settings::browsing::switch_saved_windows_in_place)
check_button_funcs_define(open_external_links_in_preview_check_button, orion::settings::browsing::open_external_links_in_preview)
check_button_funcs_define(show_standard_window_buttons_in_focus_mode_check_button, orion::settings::browsing::show_standard_window_buttons_in_focus_mode)
check_button_funcs_define(confirm_closing_tab_with_pip_check_button, orion::settings::browsing::confirm_closing_tabs_when_picture_in_picture_is_video_is_playing)
check_button_funcs_define(check_spelling_while_typing_check_button, orion::settings::browsing::check_spelling_while_typing)
check_button_funcs_define(correct_spelling_automatically_check_button, orion::settings::browsing::correct_spelling_automatically)
check_button_funcs_define(enable_minimum_font_size_check_button, orion::settings::browsing::enable_minimum_font_size)
check_button_funcs_define(press_tab_to_highlight_each_item_on_web_page_check_button, orion::settings::browsing::press_tab_to_highlight_each_item_on_web_page)
check_button_funcs_define(prevent_esc_from_exiting_fs_check_button, orion::settings::browsing::prevent_escape_from_exiting_fullscreen)

check_button_funcs_define(enable_cross_device_sync_of_tabs_bookmarks_reading_list_check_button, orion::settings::sync::enable_cross_device_sync_of_tabs_bookmarks_and_reading_list)

check_button_funcs_define(offer_to_autofill_from_orion_keychain_check_button, orion::settings::passwords::offer_to_autofill_passwords_from_orion_keychain)
check_button_funcs_define(offer_to_save_to_orion_keychain_check_button, orion::settings::passwords::offer_to_save_passwords_to_orion_keychain)
check_button_funcs_define(submit_form_automatically_check_button, orion::settings::passwords::submit_form_automatically)

check_button_funcs_define(disable_history_check_button, orion::settings::privacy::disable_history)
check_button_funcs_define(auto_update_content_blockers_check_button, orion::settings::privacy::auto_update_content_blockers)

check_button_funcs_define(also_use_search_engine_in_private_browsing_check_button, orion::settings::search::also_use_search_engine_in_private_browsing)
check_button_funcs_define(autocomplete_top_hits_check_button, orion::settings::search::autocomplete_top_hits)
check_button_funcs_define(autocomplete_search_engines_check_button, orion::settings::search::autocomplete_search_engines)
check_button_funcs_define(autocomplete_ask_fastgpt_check_button, orion::settings::search::autocomplete_ask_fastgpt)
check_button_funcs_define(autocomplete_history_check_button, orion::settings::search::autocomplete_history)
check_button_funcs_define(autocomplete_bookmarks_check_button, orion::settings::search::autocomplete_bookmarks)
check_button_funcs_define(autocomplete_open_tabs_check_button, orion::settings::search::autocomplete_open_tabs)

check_button_funcs_define(enable_json_formatting_check_button, orion::settings::advanced::enable_json_formatting)
check_button_funcs_define(allow_installation_of_3rd_party_chrome_extensions_check_button, orion::settings::advanced::allow_installation_of_3rd_party_chrome_extensions)
check_button_funcs_define(allow_installation_of_3rd_party_firefox_extensions_check_button, orion::settings::advanced::allow_installation_of_3rd_party_firefox_extensions)
