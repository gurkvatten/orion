#include "action_map.h"
#include "action.h"
#include "util/action_util.h"
#include "config/default_config.h"

#include <common/src/log.h>
#include <string>
#include <vector>

namespace orion::gtk::main_window {

// Note that stateful actions that should display a checked state ✓ in menus
// must be registered with boolean target, otherwise the checkmark will not appear.
enum class ActionType
{
    action,
    stateful_action,
    stateful_radio_string_action,
    stateful_radio_int32_action,
    stateful_radio_int64_action,
    stateless_int64_action,
    stateless_string_action,
    stateless_int64_pair_action,
    stateless_int64_array_action,
    stateless_string_double_double_action,
    stateless_tagged_union_action,
    stateless_string_string_bool_action,
};

struct ActionCallback
{
	std::string action_name{};
    GCallback callback{};
    ActionType action_type{};
    std::string initial_state{};
    bool initial_bool_state{};
};

//Gio Action should technically not have an underscore in its name, even if this does not fail in most places
//thus I introduced this to help 
namespace {
    template <std::size_t N>
    struct snake_to_kebab {
    
    public:
        char data[N]{};

        consteval snake_to_kebab(const char (&str)[N])
        {
            for(size_t i = 0; i<N; ++i)
            {
                data[i] = (str[i] == '_') ? '-' : str[i];
            }
        }

        operator std::string()
        {
            return {data, N-1};
        }
    };
}

#define ACTION(x) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::action }

#define STATEFUL_ACTION(x, initial_bool_state) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::stateful_action, "", initial_bool_state }

#define STATEFUL_RADIO_ACTION(x, initial_state) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::stateful_radio_string_action, initial_state }

#define STATEFUL_RADIO_INT32_ACTION(x) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::stateful_radio_int32_action }

#define STATEFUL_RADIO_INT64_ACTION(x) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::stateful_radio_int64_action }

#define STATELESS_INT64_ACTION(x) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::stateless_int64_action }

#define STATELESS_STRING_ACTION(x) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::stateless_string_action }

#define STATELESS_INT64_PAIR_ACTION(x) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::stateless_int64_pair_action }

#define STATELESS_INT64_ARRAY_ACTION(x) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::stateless_int64_array_action }

#define STATELESS_STRING_DOUBLE_DOUBLE_ACTION(x) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::stateless_string_double_double_action}

#define STATELESS_TAGGED_UNION_ACTION(x) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::stateless_tagged_union_action} //format: (iv)

#define STATELESS_STRING_STRING_BOOL_ACTION(x) \
    { snake_to_kebab(#x), G_CALLBACK(x), ActionType::stateless_string_string_bool_action} //format: (ssb)

static std::vector<ActionCallback> action_callbacks
{
    // Profiles Button
    STATELESS_INT64_ACTION(change_profile),
    STATELESS_INT64_ACTION(open_window_in_separate_window_or_switch_to_existing_and_edit),

	// Main Menu: Orion
	ACTION(about_orion),
	ACTION(get_orion_plus),
	ACTION(popular_extensions),
	// TODO: ACTION(check_for_updates),
	ACTION(settings),
	STATELESS_INT64_ACTION(orion_settings_on_page),
	ACTION(settings_for_current_page),
	ACTION(reset_orion),
	ACTION(hide_orion),
	// TODO: ACTION(hide_others),
	ACTION(quit_orion),

	// Main Menu: File
	ACTION(new_profile),
	ACTION(edit_profile),
	ACTION(delete_current_profile),
    ACTION(set_current_profile_as_default),
	ACTION(new_tab),
    ACTION(new_private_tab),
	ACTION(new_window),
	ACTION(new_focus_window),
	ACTION(new_private_window),
	ACTION(open_file),
	ACTION(edit_current_window_group),
    STATELESS_INT64_ACTION(edit_window_group),
	ACTION(close_current_window),
	ACTION(close_all_windows),
	ACTION(close_current_tab),
    ACTION(close_current_tab_tree),
	ACTION(close_tabs_above_current_tab),
	ACTION(mute_current_tab),
	ACTION(mute_other_tabs_than_current),
	ACTION(save_page),
	ACTION(export_as_pdf),
	ACTION(import_bookmarks_html_file),
	ACTION(import_passwords_csv_file),
    ACTION(import_passwords_csv_file_main_menu),
    ACTION(import_profile_data),
    ACTION(export_profile_data),
	ACTION(export_bookmarks),
	ACTION(export_passwords),
	// TODO: ACTION(page_setup),
	ACTION(print),
    ACTION(manage_profiles),

	// Main Menu: Edit
	ACTION(undo_tab_close),
	ACTION(redo_tab_close),
	// TODO: ACTION(cut),
	// TODO: ACTION(copy),
	ACTION(copy_page_url),
	ACTION(copy_url_as_markdown),
    ACTION(paste_and_go),
	// TODO: ACTION(paste),
	// TODO: ACTION(paste_and_match_style),
	// TODO: ACTION(select_all),
	ACTION(find),
	ACTION(find_next),
	ACTION(find_previous),
    STATEFUL_ACTION(find_ignore_case, true),
    STATEFUL_ACTION(find_wrap_around, true),
    STATEFUL_RADIO_ACTION(find_match_mode, "contains"),
	ACTION(use_selection_for_find),
	ACTION(jump_to_selection),
	// TODO: ACTION(show_spelling_and_grammar),
	// TODO: ACTION(check_document_now),
    // TODO: STATEFUL_ACTION(check_spelling_while_typing),
    // TODO: STATEFUL_ACTION(check_grammar_with_spelling),
    // TODO: STATEFUL_ACTION(correct_spelling_automatically),
    // TODO: STATEFUL_ACTION(smart_copy_paste),
    // TODO: STATEFUL_ACTION(smart_quotes),
    // TODO: STATEFUL_ACTION(smart_dashes),
    // TODO: STATEFUL_ACTION(smart_links),
    // TODO: STATEFUL_ACTION(data_detectors),
    // TODO: STATEFUL_ACTION(text_replacement),

	// Main Menu: View
    STATEFUL_ACTION(always_show_toolbar_in_fullscreen, true),
    // TODO: STATEFUL_ACTION(always_show_tab_bar),
	STATEFUL_ACTION(bookmarks_bar_visibility, true),
    STATEFUL_ACTION(toggle_show_status_bar_on_link_hover, true),
	STATEFUL_ACTION(toggle_sidebar, false),
	STATEFUL_ACTION(toggle_windows_sidebar, false),
	STATEFUL_ACTION(toggle_vertical_tabs_sidebar, false),
	STATEFUL_ACTION(toggle_bookmarks_sidebar, false),
	STATEFUL_ACTION(toggle_reading_list_sidebar, false),
	STATEFUL_ACTION(toggle_reader, false),
	ACTION(exit_reader_mode),
	ACTION(show_tab_overview),
	ACTION(show_downloads),
	STATEFUL_ACTION(toggle_focus_mode, false),
	ACTION(stop),
	ACTION(reload_this_page),
	ACTION(reload_current_page_bypass_cache),
	ACTION(reload_all_tabs_in_window),
	// TODO: ACTION(show_picture_in_picture_video),
	ACTION(reset_zoom),
	ACTION(zoom_in),
	ACTION(zoom_out),
	ACTION(toggle_full_screen),
	ACTION(show_blocked_popups),

	// Main Menu: History
	ACTION(show_history),
	ACTION(clear_history),
	ACTION(back),
	ACTION(forward),
    STATELESS_INT64_ACTION(go_back_forward_relative_index),
	ACTION(home),
	ACTION(reopen_last_closed_tab),
    ACTION(reopen_last_closed_window),
    // TODO: ACTION(reopen_all_windows_from_last_session),

	// Main Menu: Bookmarks
	ACTION(show_bookmarks),
	ACTION(show_reading_list),
	ACTION(bookmarks_add_bookmark),
	STATELESS_STRING_ACTION(open_bookmark),
	ACTION(add_bookmark),
	ACTION(add_bookmark_folder),
	ACTION(add_bookmarks_for_tabs),
	ACTION(add_to_reading_list),

	// Main Menu: Tools
	ACTION(passwords),
	ACTION(install_from_disk),
	// TODO: ACTION(notes),
	STATEFUL_ACTION(enable_compatibility_mode, false),
	ACTION(install_site_as_app),
	// TODO: ACTION(take_screenshot),
	// TODO: ACTION(take_screenshot_entire_page),
    STATEFUL_ACTION(edit_text_on_page, false),
    ACTION(allow_copy_and_paste),
	// TODO: ACTION(show_fonts),
    STATEFUL_ACTION(low_power_mode, false),
    ACTION(manage_extensions),
    ACTION(open_page_in_internet_archive),

	// Main Menu: Develop
    ACTION(toggle_web_inspector),
	// TODO: ACTION(show_design_tools),
	// TODO: ACTION(enter_responsive_design_mode),
	// TODO: ACTION(open_console),
	// TODO: ACTION(open_network),
	// TODO: ACTION(open_storage),
	// TODO: ACTION(open_resources),
    // TOOD: ACTION(open_layers),
	// TODO: ACTION(open_graphics),
	// TODO: ACTION(start_performance_profile),
	// TODO: ACTION(edit_cookies),
	// TODO: ACTION(empty_cache),
	// TODO: ACTION(view_source),
    // TODO: STATEFUL_RADIO_ACTION(user_agent, "automatic"),

	// Main Menu: Window
	ACTION(minimize),
	ACTION(fill),
	// TODO: ACTION(center),
	// TODO: ACTION(move_left),
	// TODO: ACTION(move_right),
	// TODO: ACTION(move_top),
	// TODO: ACTION(move_bottom),
	// TODO: ACTION(move_left_and_right),
	// TODO: ACTION(move_right_and_left),
	// TODO: ACTION(move_top_and_bottom),
	// TODO: ACTION(move_bottom_and_top),
	// TODO: ACTION(move_return_to_previous_size),
	ACTION(show_previous_tab_accelerator),
	ACTION(show_next_tab_accelerator),
	ACTION(show_previous_tab),
	ACTION(show_next_tab),
	ACTION(go_to_previous_tab_group),
	ACTION(go_to_next_tab_group),
	ACTION(pin_current_tab),
	ACTION(suspend_current_tab),
    ACTION(suspend_current_tab_tree),
    ACTION(duplicate_current_tab),
    ACTION(merge_all_windows),
    STATEFUL_RADIO_INT64_ACTION(focus_target_window),

    // Main Menu: DEBUG (only in debug builds)

    // Toolbar
    STATEFUL_ACTION(use_mini_toolbar, false),
    STATEFUL_RADIO_ACTION(toolbar_item_style, "1"),
    ACTION(reset_toolbar),
    STATELESS_INT64_ACTION(move_toolbar_item_to_overflow_menu),
    STATELESS_INT64_ACTION(remove_toolbar_item),    

    // Toolbar Context Menu
    ACTION(import_button_from_url_create),  // does not need existing programmable item ID
    ACTION(import_button_from_file_create), // does not need existing programmable item ID

    // More Tools menu
    ACTION(export_button_to_file),
    ACTION(customize_menu),
    STATELESS_INT64_ACTION(edit_programmable_button),
    ACTION(summarize_page),

    // Tabs
    STATELESS_INT64_ACTION(bookmark_tab),
    STATELESS_INT64_ACTION(bookmark_tab_tree),
    STATELESS_INT64_ACTION(duplicate_tab),
    STATELESS_INT64_ACTION(pin_tab),
    STATELESS_INT64_ACTION(mute_tab),
    STATELESS_INT64_ACTION(mute_other_tabs),
    STATELESS_INT64_PAIR_ACTION(move_tab_to_window),
    STATELESS_INT64_PAIR_ACTION(move_tab_tree_to_window),
    STATELESS_INT64_ACTION(move_tab_to_new_window),
    STATELESS_INT64_ACTION(move_tab_tree_to_new_window),
    STATELESS_INT64_ACTION(move_tab_to_new_in_place_window),
    STATELESS_INT64_ACTION(move_tab_tree_to_new_in_place_window),
    STATELESS_INT64_PAIR_ACTION(move_tab_to_profile),
    STATELESS_INT64_PAIR_ACTION(move_tab_tree_to_profile),
    STATELESS_INT64_ACTION(reload_tab),
    STATELESS_INT64_ACTION(suspend_tab),
    STATELESS_INT64_ACTION(suspend_tab_tree),
    STATELESS_INT64_ACTION(suspend_other_tabs),
    STATELESS_INT64_ACTION(close_tab),
    STATELESS_INT64_ACTION(close_tab_tree),
    STATELESS_INT64_ACTION(close_tabs_below),
    STATELESS_INT64_ACTION(close_tabs_above),
    STATELESS_INT64_ACTION(close_other_tabs),
    STATEFUL_ACTION(show_tab_preview_on_hover, true),
    STATEFUL_ACTION(show_tab_switcher, true),
    STATEFUL_RADIO_ACTION(tab_switch_order, "1"),
    STATEFUL_ACTION(when_opening_link_in_new_tab_make_it_active, true),

    // Sidebar
    STATEFUL_RADIO_ACTION(sidebar_type, orion::gtk::get_default_sidebar_page()),
    STATEFUL_ACTION(auto_show_sidebar, false),
    STATEFUL_ACTION(auto_show_sidebar_in_fullscreen, false),
    ACTION(reading_list_clear_all_items),
    ACTION(open_reading_list_bookmarks),
    ACTION(sidebar_rename_bookmark),
    STATEFUL_ACTION(show_nested_tabs, false),
    STATEFUL_ACTION(open_new_tab_next_to_current_tab, false),
    STATEFUL_ACTION(show_favicons_inside_tabs, true),
    STATEFUL_ACTION(always_show_website_titles_in_tabs, true),
    STATELESS_INT64_ACTION(open_recently_closed_tab),
    STATELESS_INT64_ACTION(open_recently_closed_window),

    // Settings dialog
    STATEFUL_RADIO_INT32_ACTION(share_crash_reports_setting),
    STATEFUL_RADIO_ACTION(app_icon_setting, "orion-logo-a"),
    STATEFUL_RADIO_ACTION(tab_layout_setting, "standard"),
    STATEFUL_RADIO_ACTION(file_download_location, "1"),
    STATEFUL_RADIO_ACTION(file_download_custom_path, "1"),

    // Bookmarks dialog
    ACTION(bookmarks_open_in_new_tabs),
    ACTION(bookmarks_open_in_new_window),
    ACTION(bookmarks_rename_bookmark),
    ACTION(bookmarks_delete_bookmark),
    ACTION(bookmarks_add_folder),
    ACTION(bookmarks_copy_bookmarks),
    ACTION(bookmarks_paste_bookmarks),

    ACTION(open_history_address_in_new_tab),
    ACTION(open_history_address_in_new_window),
    STATELESS_STRING_ACTION(open_history_address_in_current_tab),
    ACTION(copy_history_address),
    ACTION(history_columnview_delete),

    ACTION(reading_list_columnview_delete),
    ACTION(reading_list_columnview_open_in_new_tab),
    ACTION(reading_list_columnview_open_in_new_window),

    ACTION(profiles_columnview_switch),
    ACTION(profiles_columnview_edit),
    ACTION(profiles_columnview_delete),
    ACTION(profiles_columnview_set_default),

    //Bookmarks bar

    ACTION(hide_bookmarks_bar),
    ACTION(bookmarks_bar_rename_bookmark),
    STATELESS_INT64_ACTION(bookmarks_bar_open_in_new_tabs),
    STATEFUL_RADIO_ACTION(bookmarks_bar_style, "1"),

    // Window List Box
    ACTION(restore_snapshot_menu),
    ACTION(take_workspace_snapshot),
    STATEFUL_ACTION(show_recently_opened_on_top, false),
    STATEFUL_ACTION(switch_saved_windows_in_place, true),
    STATELESS_INT64_ACTION(duplicate_window),
    STATELESS_INT64_ACTION(open_window),
    STATELESS_INT64_ACTION(close_window),
    STATELESS_INT64_ACTION(suspend_window),
    STATELESS_INT64_ACTION(delete_window),
    STATELESS_INT64_ACTION(copy_tab_links_of_window),
    STATELESS_INT64_ACTION(bookmark_tabs),
    STATELESS_INT64_ARRAY_ACTION(suspend_tabs),
    STATELESS_INT64_ACTION(suspend_tabs_in_window),
    STATELESS_INT64_ACTION(open_window_in_separate_window),
    STATELESS_INT64_ACTION(open_window_in_separate_window_or_switch_to_existing),
    STATELESS_INT64_ACTION(switch_to_tab),

    // Window List Button
    STATEFUL_RADIO_ACTION(window_list_button_style, "1"),

    // Data
    STATELESS_INT64_ACTION(reading_list_entry_delete),
    STATELESS_INT64_ACTION(reading_list_entry_mark_as_read),
    STATELESS_INT64_ACTION(reading_list_entry_mark_as_unread),

    // Downloads
    STATELESS_STRING_ACTION(open_file_by_path),
    STATELESS_INT64_ACTION(open_download),
    STATELESS_STRING_ACTION(show_file_in_file_manager),
    STATELESS_INT64_ACTION(remove_download_item),
    STATELESS_INT64_ACTION(stop_download),
    STATELESS_INT64_ACTION(resume_download),

    // Reader Mode
    STATEFUL_RADIO_INT64_ACTION(reader_mode_font_size_index),
    STATEFUL_RADIO_INT64_ACTION(reader_mode_background_type),
    STATEFUL_RADIO_INT64_ACTION(reader_mode_font_name),
    STATEFUL_RADIO_INT64_ACTION(reader_mode_document_width),
    STATEFUL_RADIO_INT64_ACTION(reader_mode_text_alignment),
    STATEFUL_RADIO_INT64_ACTION(reader_mode_border_shadow_enabled),

    // Other
    STATEFUL_RADIO_ACTION(profiles_button_style, "1"),
    ACTION(install_from_firefox_store),
    ACTION(install_from_chrome_store),
    ACTION(edit_bookmark_folder),
    ACTION(edit_bookmark),
    ACTION(import_passwords_from_browser),
    ACTION(import_passwords_from_extension),
    ACTION(extension_permissions),
    STATEFUL_ACTION(do_not_update_extension, false),
    ACTION(import_passwords_from_browser),
    ACTION(import_passwords_from_extension),
    STATELESS_INT64_ACTION(edit_password),
    STATELESS_INT64_ACTION(edit_search_engine),
    STATELESS_STRING_ACTION(copy_website),
    STATELESS_STRING_ACTION(copy_username),
    STATELESS_STRING_ACTION(copy_password),
    STATELESS_STRING_ACTION(open_uri_in_current_tab),
    STATELESS_STRING_ACTION(open_uri_in_new_tab),
    STATELESS_STRING_ACTION(open_uri_in_new_window),
    STATELESS_STRING_ACTION(open_uri_in_new_private_window),
    STATELESS_STRING_DOUBLE_DOUBLE_ACTION(open_uri_in_preview_window),
    STATELESS_STRING_ACTION(open_uri_with_placeholders_in_current_tab),
    STATELESS_STRING_ACTION(open_uri_with_placeholders_in_new_tab),
    STATELESS_STRING_ACTION(open_uri_with_placeholders_in_new_window),
    STATELESS_STRING_ACTION(open_uri_with_placeholders_in_background),
    STATELESS_STRING_DOUBLE_DOUBLE_ACTION(open_uri_with_placeholders_in_preview_window),
    STATELESS_STRING_ACTION(clipboard_copy),
    STATELESS_INT64_ACTION(reading_list_open_entry_new_tab_mark_read),
    STATELESS_INT64_ACTION(reading_list_open_entry_new_window_mark_read),
    ACTION(new_saved_window),
    ACTION(handle_ctrl_num_1),
    ACTION(handle_ctrl_num_2),
    ACTION(handle_ctrl_num_3),
    ACTION(handle_ctrl_num_4),
    ACTION(handle_ctrl_num_5),
    ACTION(handle_ctrl_num_6),
    ACTION(handle_ctrl_num_7),
    ACTION(handle_ctrl_num_8),
    ACTION(handle_ctrl_num_9),
    ACTION(select_address_entry),
    ACTION(open_start_page),
    ACTION(show_page_tweaks),
    ACTION(show_privacy),
    //ACTION(show_kagi_privacy_pass),

    STATELESS_INT64_ACTION(import_button_from_url),
    STATELESS_INT64_ACTION(import_button_from_file),
    STATELESS_INT64_ACTION(export_button_to_file),
    STATELESS_INT64_ACTION(programmable_button_action),
    STATELESS_STRING_STRING_BOOL_ACTION(run_javascript_code),
#ifndef NDEBUG
    ACTION(test_action),
#endif
};

#undef ACTION
#undef STATEFUL_ACTION
#undef STATEFUL_RADIO_ACTION
#undef STATEFUL_RADIO_INT32_ACTION
#undef STATEFUL_RADIO_INT64_ACTION
#undef STATELESS_INT64_ACTION
#undef STATELESS_STRING_ACTION
#undef STATELESS_INT64_PAIR_ACTION
#undef STATELESS_INT64_ARRAY_ACTION
#undef STATELESS_STRING_DOUBLE_DOUBLE_ACTION

void
register_actions(MainWindow* win)
{
	for (const auto& item : action_callbacks) {
        GVariant* initial_state;
        switch (item.action_type) {
            case ActionType::action:
                register_g_action(GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback));
                break;

            case ActionType::stateful_action:
                register_g_action_stateful(
                    GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback), item.initial_bool_state
                );
                break;

            case ActionType::stateful_radio_string_action:
                initial_state = g_variant_new_string(item.initial_state.c_str());
                register_g_action_stateful_radio(
                    GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback), initial_state,
                    G_VARIANT_TYPE_STRING
                );
                break;

            case ActionType::stateful_radio_int32_action:
                initial_state = g_variant_new_int32(0);
                register_g_action_stateful_radio(
                    GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback), initial_state,
                    G_VARIANT_TYPE_INT32
                );
                break;

             case ActionType::stateful_radio_int64_action:
                initial_state = g_variant_new_int64(0);
                register_g_action_stateful_radio(
                    GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback), initial_state,
                    G_VARIANT_TYPE_INT64
                );
                break;

            case ActionType::stateless_int64_action:
                register_g_action_stateless_parameterised(
                    GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback), G_VARIANT_TYPE_INT64
                );
                break;

            case ActionType::stateless_string_action:
                register_g_action_stateless_parameterised(
                    GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback), G_VARIANT_TYPE_STRING
                );
                break;

            case ActionType::stateless_int64_pair_action:
                register_g_action_stateless_parameterised(
                    GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback), (GVariantType*)"(xx)"
                );
                break;

            case ActionType::stateless_int64_array_action:
                register_g_action_stateless_parameterised(
                    GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback), G_VARIANT_TYPE("ax")
                );
                break;

            case ActionType::stateless_string_double_double_action:
                register_g_action_stateless_parameterised(
                    GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback), G_VARIANT_TYPE("(sdd)")
                );
                break;

            case ActionType::stateless_tagged_union_action:
                register_g_action_stateless_parameterised(
                    GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback), G_VARIANT_TYPE("(iv)")
                );
                break;

            case ActionType::stateless_string_string_bool_action:
                register_g_action_stateless_parameterised(
                    GTK_WIDGET(win), item.action_name.c_str(), G_CALLBACK(item.callback), G_VARIANT_TYPE("(ssb)")
                );
                break;

            //no default case to force a compile time check of all ActionType values being accounted for
        }
	}
}

} // namespace orion::gtk::main_window
