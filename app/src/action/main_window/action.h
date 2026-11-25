#pragma once

#include "window/main_window/main_window.h"

namespace orion::gtk {

enum class TabType { normal, tree };

} // namespace orion::gtk

namespace orion::gtk::main_window {

// FIXME: columnviews pass parameters through main window, check main_window_get_context_menu_data
// this should be eradicated and these should be changed to get correct parameter count
// to reflect reality

//-------------------------------------------------------------------------
// Profiles Button
//-------------------------------------------------------------------------

void change_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Main Menu: Orion
//-------------------------------------------------------------------------

void about_orion(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void get_orion_plus(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void popular_extensions(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void check_for_updates(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void settings(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void orion_settings_on_page(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void settings_for_current_page(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reset_orion(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void hide_orion(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void hide_others(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void quit_orion(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Main Menu: File
//-------------------------------------------------------------------------

void new_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void edit_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void delete_current_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void set_current_profile_as_default(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void new_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void new_private_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void new_focus_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void new_private_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_file(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void edit_window_group(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void edit_current_window_group(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void close_current_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void close_all_windows(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void close_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void close_tab_tree(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void close_tabs_above_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void mute_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void mute_other_tabs_than_current(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void save_page(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void export_as_pdf(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void import_bookmarks_html_file(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void import_passwords_csv_file(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void import_passwords_csv_file_main_menu(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void export_bookmarks(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void import_profile_data(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void export_profile_data(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void export_passwords(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void page_setup(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void print(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void manage_profiles(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Main Menu: Edit
//-------------------------------------------------------------------------

void undo_tab_close(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void redo_tab_close(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void cut(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void copy(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void copy_page_url(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void copy_url_as_markdown(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void paste_and_go(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void paste(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void paste_and_match_style(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void select_all(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void find(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void find_next(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void find_previous(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void find_ignore_case(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void find_wrap_around(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void find_match_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void use_selection_for_find(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void jump_to_selection(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_spelling_and_grammar(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void check_document_now(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void check_spelling_while_typing(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void check_grammar_with_spelling(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void correct_spelling_automatically(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void smart_copy_paste(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void smart_quotes(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void smart_dashes(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void smart_links(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void data_detectors(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void text_replacement(GSimpleAction* action, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Main Menu: View
//-------------------------------------------------------------------------

void always_show_toolbar_in_fullscreen(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void always_show_tab_bar(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void bookmarks_bar_visibility(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void toggle_show_status_bar_on_link_hover(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void toggle_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void toggle_windows_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void toggle_vertical_tabs_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void toggle_bookmarks_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void toggle_reading_list_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void toggle_reader(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void exit_reader_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_tab_overview(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_downloads(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void toggle_focus_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void stop(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reload_this_page(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reload_current_page_bypass_cache(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reload_all_tabs_in_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_picture_in_picture_video(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reset_zoom(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void zoom_in(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void zoom_out(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void toggle_full_screen(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_blocked_popups(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Main Menu: History
//-------------------------------------------------------------------------

void show_history(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void clear_history(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void back(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void forward(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void go_back_forward_relative_index(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void home(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reopen_last_closed_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reopen_last_closed_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Main Menu: Bookmarks
//-------------------------------------------------------------------------

void show_bookmarks(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_reading_list(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmarks_add_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void add_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void add_bookmark_folder(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void add_bookmarks_for_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void add_to_reading_list(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Main Menu: Tools
//-------------------------------------------------------------------------

void passwords(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void take_screenshot(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void take_screenshot_entire_page(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void edit_text_on_page(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void allow_copy_and_paste(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void install_from_disk(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void notes(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void enable_compatibility_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void install_site_as_app(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_fonts(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void low_power_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void manage_extensions(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_page_in_internet_archive(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Main Menu: Develop
//-------------------------------------------------------------------------

void show_web_inspector(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void toggle_web_inspector(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_design_tools(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void enter_responsive_design_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_console(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_network(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_storage(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_resources(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_layers(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_graphics(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void start_performance_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void edit_cookies(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void empty_cache(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void view_source(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void user_agent(GSimpleAction* action, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Main Menu: Window
//-------------------------------------------------------------------------

void minimize(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void fill(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void center(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_left(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_right(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_top(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_bottom(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_left_and_right(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_right_and_left(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_top_and_bottom(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_bottom_and_top(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_return_to_previous_size(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_previous_tab_accelerator(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_next_tab_accelerator(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_previous_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_next_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void go_to_previous_tab_group(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void go_to_next_tab_group(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void pin_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void suspend_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void suspend_current_tab_tree(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void duplicate_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void merge_all_windows(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void focus_target_window(GSimpleAction* action, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Main Menu: DEBUG (only in debug builds)
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Toolbar
//-------------------------------------------------------------------------

void use_mini_toolbar(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void toolbar_item_style(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reset_toolbar(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_toolbar_item_to_overflow_menu(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void remove_toolbar_item(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Toolbar Context Menu
// -------------------------------------------------------------------------

// Creates new programmable button and adds it to the toolbar
void import_button_from_url_create(GSimpleAction* self, GVariant* parameter, gpointer user_data);
// Creates new programmable button and adds it to the toolbar
void import_button_from_file_create(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// More Tools menu
//-------------------------------------------------------------------------

void customize_menu(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void edit_programmable_button(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void summarize_page(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Tabs
//-------------------------------------------------------------------------

void bookmark_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmark_tab_tree(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void duplicate_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void pin_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void mute_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void mute_other_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_tab_to_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_tab_tree_to_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_tab_to_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_tab_tree_to_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_tab_to_new_in_place_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_tab_tree_to_new_in_place_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_tab_to_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void move_tab_tree_to_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reload_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void suspend_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void suspend_tab_tree(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void suspend_other_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data);
// Suspends all non-active (not selected) and non-pinned tabs.
// If only_inactive_for_some_time is true, then only tabs that have not been active for some time are suspended.
void suspend_non_active_non_pinned_tabs(MainWindow* win, bool only_inactive_for_some_time);
void close_tab_tracking_recent(TabId tab_id, ProfileId profile_id, WindowId window_id, std::function<void(Error)> done_cb = nullptr);
void close_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void close_current_tab_tree(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void close_tabs_below(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void close_tabs_above(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void close_other_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_tab_preview_on_hover(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_tab_switcher(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void tab_switch_order(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void when_opening_link_in_new_tab_make_it_active(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Sidebar
//-------------------------------------------------------------------------

void sidebar_type(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void reading_list_clear_all_items(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_reading_list_bookmarks(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void sidebar_rename_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_nested_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_new_tab_next_to_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_favicons_inside_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void always_show_website_titles_in_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_recently_closed_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_recently_closed_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void auto_show_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void auto_show_sidebar_in_fullscreen(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Settings dialog
//-------------------------------------------------------------------------

void share_crash_reports_setting(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void app_icon_setting(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void tab_layout_setting(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void file_download_location(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void file_download_custom_path(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Bookmarks dialog
//-------------------------------------------------------------------------

void bookmarks_open_in_new_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmarks_open_in_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmarks_rename_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmarks_delete_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmarks_add_folder(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmarks_copy_bookmarks(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmarks_paste_bookmarks(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_history_address_in_new_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_history_address_in_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_history_address_in_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void copy_history_address(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void history_columnview_delete(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reading_list_columnview_delete(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reading_list_columnview_open_in_new_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reading_list_columnview_open_in_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void profiles_columnview_switch(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void profiles_columnview_edit(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void profiles_columnview_delete(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void profiles_columnview_set_default(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Bookmarks bar
//-------------------------------------------------------------------------

void hide_bookmarks_bar(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmarks_bar_rename_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmarks_bar_open_in_new_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmarks_bar_style(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Window List Button
//-------------------------------------------------------------------------

void window_list_button_style(GSimpleAction* action, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Window List Box
//-------------------------------------------------------------------------

void take_workspace_snapshot(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void restore_snapshot_menu(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void duplicate_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_recently_opened_on_top(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void switch_saved_windows_in_place(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void close_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void suspend_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void delete_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void copy_tab_links_of_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void bookmark_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void suspend_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void suspend_tabs_in_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_window_in_separate_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_window_in_separate_window_or_switch_to_existing(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_window_in_separate_window_or_switch_to_existing_and_edit(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void switch_to_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void switch_to_tab_in_any_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------

void reading_list_entry_delete(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reading_list_entry_mark_as_read(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reading_list_entry_mark_as_unread(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Downloads
//-------------------------------------------------------------------------

void open_file_by_path(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_download(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_file_in_file_manager(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void remove_download_item(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void stop_download(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void resume_download(GSimpleAction* self, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Reader Mode
//-------------------------------------------------------------------------

void reader_mode_font_size_index(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void reader_mode_background_type(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void reader_mode_font_name(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void reader_mode_document_width(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void reader_mode_text_alignment(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void reader_mode_border_shadow_enabled(GSimpleAction* action, GVariant* parameter, gpointer user_data);

//-------------------------------------------------------------------------
// Other
//-------------------------------------------------------------------------

void profiles_button_style(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void install_from_firefox_store(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void install_from_chrome_store(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void edit_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void edit_bookmark_folder(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void import_passwords_from_browser(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void import_passwords_from_extension(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void extension_permissions(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void do_not_update_extension(GSimpleAction* action, GVariant* parameter, gpointer user_data);
void copy_website(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void copy_username(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void copy_password(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void edit_password(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void edit_search_engine(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_uri_in_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_uri_in_new_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_uri_with_placeholders_in_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_uri_with_placeholders_in_new_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_uri_with_placeholders_in_background(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reading_list_open_entry_new_tab_mark_read(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void reading_list_open_entry_new_window_mark_read(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_uri_in_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_uri_in_new_private_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_uri_in_preview_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_uri_with_placeholders_in_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_uri_with_placeholders_in_preview_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void clipboard_copy(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void new_saved_window(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void handle_ctrl_num_1(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void handle_ctrl_num_2(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void handle_ctrl_num_3(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void handle_ctrl_num_4(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void handle_ctrl_num_5(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void handle_ctrl_num_6(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void handle_ctrl_num_7(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void handle_ctrl_num_8(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void handle_ctrl_num_9(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void select_address_entry(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void open_start_page(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_page_tweaks(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_privacy(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void show_kagi_privacy_pass(GSimpleAction* self, GVariant* parameter, gpointer user_data);

void import_button_from_url(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void import_button_from_file(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void export_button_to_file(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void programmable_button_action(GSimpleAction* self, GVariant* parameter, gpointer user_data);
void run_javascript_code(GSimpleAction* self, GVariant* parameter, gpointer user_data);

#ifndef NDEBUG
void test_action(GSimpleAction* self, GVariant* parameter, gpointer user_data);
#endif

} // namespace orion::gtk::main_window
