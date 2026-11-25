#define ENABLE_VERBOSE_LOGS 0

#include "action.h"

#include <iostream>
#include <filesystem>
#include <adwaita.h>
#include <glib/gi18n.h>
#include <regex>
#include <rfl.hpp>

#include <common/src/log.h>
#include <core/settings.h>
#include <common/src/string_util.h>
#include <core/downloader.h>
#include <common/src/time_util.h>
#include <common/src/uuid.h>
#include <common/src/ptr_util.h>

#include "util/data_util.h"
#include "util/ref_util.h"
#include "util/html_description_util.h"
#include "util/menu_util.h"
#include "util/dialog_util.h"
#include "util/action_util.h"
#include "util/file_util.h"
#include "util/text_util.h"
#include "util/compatibility_mode.h"
#include "util/profile_manager.h"

#include "window/main_window/main_window.h"
#include "window/main_window/js_sidebar/js_sidebar.h"
#include "window/main_window/header/bookmarks_bar.h"

#include "window/about_dialog.h"
#include "window/reset_orion_dialog.h"
#include "window/settings_dialog.h"
#include "window/check_for_updates_dialog.h"
#include "window/settings_for_website_dialog.h"
#include "window/bookmarks_dialog.h"
#include "window/get_orion_plus_dialog.h"
#include "window/profile_dialog.h"
#include "window/popular_extensions_dialog.h"
#include "window/install_site_as_app_dialog.h"
#include "window/add_bookmark_dialog.h"
#include "window/add_bookmarks_for_tabs_dialog.h"
#include "window/clear_history_dialog.h"
#include "window/import_from_url_dialog.h"
#include "window/customize_toolbar_dialog.h"
#include "window/low_power_mode_dialog.h"
#include "window/programmable_button_popover.h"
#include "window/edit_bookmark_dialog.h"
#include "window/import_from_browser_dialog.h"
#include "window/import_from_extension_dialog.h"
#include "window/edit_password_dialog.h"
#include "window/summary_unavailable_dialog.h"
#include "window/export_profile_data_dialog.h"
#include "window/import_profile_data_dialog.h"
#include "window/import_csv_passwords_dialog.h"
#include "window/delete_csv_passwords_dialog.h"
#include "window/add_search_engine_dialog.h"
#include "window/quit_orion_dialog.h"
#include "window/main_window/sidebar/sidebar.h"
#include "window/downloads_dialog.h"
#include "window/import_html_bookmarks_dialog.h"

#include "window/page_tweaks_popover.h"

#include "data_item/bookmarks_columnview_item.h"
#include "data_item/history_columnview_item.h"
#include "data_item/reading_list_columnview_item.h"
#include "data_item/profiles_columnview_item.h"

#include "window/editable_cell_widget/orion_editable_cell_entry.h"

#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_container.h"
#include "window/orion_webview_tabview_widgets/scripts/orion_internals_scripts.h"
#include "window/orion_webview_tabview_widgets/orion_webview_preview_window.h"

namespace orion::gtk::main_window {

//-------------------------------------------------------------------------
// Internal implementations
//-------------------------------------------------------------------------

static void
close_tab_tracking_recent_internal(TabId tab_id, ProfileId profile_id, WindowId window_id, TabType type, std::function<void(Error)> done_cb = nullptr)
{
    global_core_api->get_tabs({tab_id}, [profile_id,tab_id,window_id,type,done_cb](GetTabsReply reply){
        VERIFY_LOG_OR_DONE_CB(reply.err, done_cb);
        if (reply.tabs.empty()) {
            if (done_cb) done_cb(Error::ok());
            return;
        }
        const auto& tab = reply.tabs.at(0);
        
        CreateUndoRedoTabStepQuery query{
            .window_id = window_id,
            .tab_ids = {tab.id},
        };

        // create undo/redo step before deleting the tab
        global_core_api->create_undo_redo_tab_step(query, [profile_id,tab,type,done_cb](CreateUndoRedoTabStepReply reply){
            VERIFY_LOG_OR_DONE_CB(reply.err, done_cb);

            global_core_api->get_descendant_tab_ids(tab.id, [profile_id,tab,type,done_cb](GetDescendantTabIdsReply reply){
                VERIFY_LOG_OR_DONE_CB(reply.err, done_cb);

                std::vector<TabId> tab_ids_to_delete;
                tab_ids_to_delete.push_back(tab.id);

                if (type == TabType::tree) {
                    for (TabId descendant_id : reply.descendant_tab_ids) {
                        tab_ids_to_delete.push_back(descendant_id);
                    }
                }

                global_core_api->delete_tabs(tab_ids_to_delete, [profile_id,tab,done_cb](Error err){
                    VERIFY_LOG_OR_DONE_CB(err, done_cb);

                    // do not store Start Page tabs
                    if (tab.uri.empty() || tab.uri == "orion://newtab/") {
                        if (done_cb) done_cb(Error::ok());
                        return;
                    }

                    UnixTimeUtc64 now = static_cast<UnixTimeUtc64>(
                        std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()
                        ).count()
                    );

                    CreateRecentlyClosedTabQuery query{
                        .profile_id = profile_id,
                        .name = tab.name,
                        .uri = tab.uri,
                        .closed_at = now,
                        .icon_name = tab.icon_name,
                    };

                    global_core_api->create_recently_closed_tab(query, [tab,done_cb](CreateRecentlyClosedTabReply reply){
                        VERIFY_LOG_OR_DONE_CB(reply.err, done_cb);

                        // everything ok
                        if (done_cb) done_cb(Error::ok());
                    });
                });
            });
        });
    });
}

// Recursively deletes ~/.cache/oriongtk/PROFILE_ID/ directory.
// Deletes for example these subdirectories: cache, data, filter_lists
static Error
delete_profile_file_system_data(orion::ProfileId profile_id)
{
    std::string profile_id_str = std::to_string(profile_id.value);

    // Safety check
    if (profile_id_str.empty()) {
        RAISE(orion::ErrorCode::argument, "delete_profile_file_system_data: profile_id is empty");
    }

    const char* cache_dir = g_get_user_cache_dir();
    gchar* dir_path = g_build_filename(
        cache_dir,
        APPLICATION_COMMAND,
        profile_id_str.c_str(),
        NULL);
    defer([dir_path]{ g_free(dir_path); });

    GFile* dir = g_file_new_for_path(dir_path);
    defer([dir]{ g_object_unref(dir); });

    if (g_file_query_exists(dir, NULL)) {
        orion::log_debug(
            "delete_profile_file_system_data: profile_id=%lld path=%s",
            profile_id.value, dir_path);

        GError* error = NULL;
        if (!orion::delete_recursively(dir, &error)) {
            defer([error]{ g_error_free(error); });
            RAISE(
                orion::ErrorCode::io,
                "failed to delete file system data for profile_id=%lld: %s",
                profile_id.value,
                error ? error->message : "unknown error");
        }
    } else {
        orion::log_debug(
            "delete_profile_file_system_data: profile_id=%lld path=%s does not exist, nothing to delete",
            profile_id.value, dir_path);
    }
    SUCCEED();
}

static void
delete_current_profile_dialog_on_response(AdwAlertDialog* self, gchar* response, MainWindow* user_data)
{
    if(strcmp(response, "delete")==0)
    {
        orion::ProfileId id = *((orion::ProfileId*)g_object_get_data(G_OBJECT(self), "orion-profile-id"));

        global_core_api->get_default_profile_id([id](orion::GetDefaultProfileIdReply reply){
            if(reply.err)
            {
                VERIFY_LOG(reply.err);
            }
            orion::log_debug("deleting profile %lld, default profile is %lld", id.value, reply.profile_id.value);
            orion::DeleteProfilesQuery query{
                .ids = {id},
                .delete_file_system_data_fn = delete_profile_file_system_data,
            };
            global_core_api->delete_profiles(query, [id](orion::Error err){
                VERIFY_LOG(err);
            });
        });
    }
}

static void
delete_profile_internal(ProfileId profile_id, MainWindow* win)
{
    if (profile_id != main_window_get_current_profile_id(win)) {
        orion::DeleteProfilesQuery query{
            .ids = {profile_id},
            .delete_file_system_data_fn = delete_profile_file_system_data,
        };
        global_core_api->delete_profiles(query, [profile_id](orion::Error err){
            VERIFY_LOG(err);
        });
        return;
    }

    WeakRefWrapper win_weak(win);

    global_core_api->get_profiles({profile_id}, [profile_id,win_weak](GetProfilesReply reply){
        VERIFY_LOG(reply.err);
        if (reply.profiles.empty()) {
            orion::log_warning("delete_profile_internal: no such profile with id %lu", profile_id);
            return;
        }
        const orion::ProfileDto& profile = reply.profiles.at(0);

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());
        
        AdwDialog* dialog = adw_alert_dialog_new(NULL, NULL);
        orion::ProfileId* profile_id_ptr = new orion::ProfileId(profile_id);
        g_object_set_data_full(G_OBJECT(dialog), "orion-profile-id", profile_id_ptr, [](gpointer data){
            delete (orion::ProfileId*)(data);
        });
        //NOTE: long heading has issues with size allocation,
        //see console output when bringing this dialog up
        adw_alert_dialog_format_heading(ADW_ALERT_DIALOG(dialog),
            _("Are you sure you want to permanently delete “%s” profile?"), profile.name.c_str());
        
        adw_alert_dialog_set_body(ADW_ALERT_DIALOG(dialog),
            _("Browser will be restarted due to deleting current profile."));

        adw_alert_dialog_add_responses(
            ADW_ALERT_DIALOG(dialog),
            "cancel", _("_Cancel"),
            "delete", _("_Delete"),
            NULL);

        adw_alert_dialog_set_response_appearance(
            ADW_ALERT_DIALOG(dialog),
            "delete",
            ADW_RESPONSE_DESTRUCTIVE);

        adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "delete");
        adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");

        g_signal_connect(dialog, "response", G_CALLBACK(delete_current_profile_dialog_on_response), win);


        main_window_popdown_all_popovers(win);
        adw_dialog_present(dialog, GTK_WIDGET(win));
    });
}

// The window will always be opened in a separate window, even if it is already open
// in another UI window. The cb callback will be called with pointer to the MainWindow that was opened.
static void
open_window_in_separate_window_internal(WindowId window_id, std::function<void(MainWindow*)> cb = nullptr)
{
    global_core_api->get_windows({window_id}, [window_id,cb=std::move(cb)](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.windows.empty()) {
            orion::log_warning("open_window_in_separate_window: no such window with id %lu", window_id);
            return;
        }
        const orion::WindowDto& window = reply.windows.at(0);

        if (window.is_suspended) {
            orion::UpdateWindowQuery query{};
            query.id = window_id;
            query.is_suspended = false;
            query.update_is_suspended = true;
            global_core_api->update_window(query, [window_id](orion::Error err){
                VERIFY_LOG(err);
            });
        }

        main_window_new_from_backend(
            g_application_get_default(), window.profile_id, window_id, orion::PrivateMode::no, // never private
            [cb=std::move(cb)](MainWindow* new_win){
                gtk_window_present(GTK_WINDOW(new_win));
                if (cb) {
                    cb(new_win);
                }
            });
    });
}

static void
switch_to_tab_internal(MainWindow* win, TabId tab_id)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper win_weak(win);
    orion::WindowId window_id = main_window_get_window_id(win);

    global_core_api->get_windows({window_id}, [win_weak,tab_id,profile_id](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.windows.empty()) return;
        const orion::WindowDto& window = reply.windows.at(0);

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        OrionWebviewTabContainer* old_container = main_window_get_orion_webview_tab_container_by_tab_id(win, window.selected_tab_id);
        if (old_container) {
            orion_webview_tab_container_set_last_active_time(old_container, orion::tm::unix_time_utc64_now());
        }

        global_core_api->get_tabs({tab_id}, [win_weak,tab_id,profile_id](orion::GetTabsReply reply) {
            VERIFY_LOG(reply.err);
            if (reply.tabs.empty()) {
                orion::log_warning("switch_to_tab: no such tab with id %lu", tab_id);
                return;
            }
            orion::TabDto tab = reply.tabs.at(0);
            orion::WindowId window_id = tab.window_id;

            // try to find the first UI window for the corresponding window_id
            std::vector<UiWindowId> ui_ids = global_core_api->get_ui_windows_by_window_id(window_id);
            if (ui_ids.empty()) {
                // no UI window for the corresponding window_id exists, create it
                open_window_in_separate_window_internal(window_id);
                
                // we cannot switch to the tab immediately because the UI window is created asynchronously,
                // so instead we set selected tab_id in the backend
                orion::UpdateWindowQuery query{};
                query.id = window_id;
                query.selected_tab_id = tab_id;
                query.update_selected_tab_id = true;

                global_core_api->update_window(query, [win_weak,tab](orion::Error err){
                    VERIFY_LOG(err);
                    
                    // unsuspend the tab if it is suspended
                    if (tab.is_suspended) {
                        orion::UpdateTabQuery query{
                            .id = tab.id,
                            .is_suspended = false,
                            .update_is_suspended = true,
                        };
                        global_core_api->update_tab(query, [win_weak,tab_id=tab.id](orion::Error err){
                            VERIFY_LOG(err);
                            auto win_ptr = win_weak.lock();
                            if (!win_ptr) return;
                            MainWindow* win = MAIN_WINDOW(win_ptr.get());
                            main_window_reload_tab_by_tab_id(win, tab_id);
                        });
                    }
                });
            } else {
                MainWindow* win = MAIN_WINDOW(static_cast<void*>(ui_ids.front())); // take the first one
                gtk_window_present(GTK_WINDOW(win)); // focus the corresponding main window
                main_window_switch_to_tab(win, tab); // switch to specified tab
            }
        });
    });
}

static void
open_uri_in_new_window_internal(const std::string& uri, MainWindow* win)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    // This should create new non-private window even if the current window is private
    orion::CreateWindowQuery query{};
    query.icon_name = "window-flat-symbolic";
    query.profile_id = profile_id;
    query.is_tab_group = true; // this is never a saved window

    global_core_api->create_window(query, [uri,profile_id](orion::CreateWindowReply reply){
        VERIFY_LOG(reply.err);
        orion::WindowId window_id = reply.id;
        orion::CreateTabQuery query{
            .window_id = window_id,
            .uri = uri,
        };
        global_core_api->create_tab(query, [profile_id,window_id](orion::CreateTabReply reply){
            VERIFY_LOG(reply.err);
            MainWindow* new_win = main_window_new(
                g_application_get_default(), profile_id, window_id, orion::PrivateMode::no);
            gtk_window_present(GTK_WINDOW(new_win));
        });
    });
}

//negative native_x, native_y imply we should check for current mouse pos
static void
open_uri_in_preview_window_internal(const std::string& uri, double native_x, double native_y, MainWindow* win)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);


    
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_setting(profile_id, orion::settings::appearance::web_preview_width,
        [win_weak, profile_id, uri, native_x, native_y](orion::GetSettingReply<int64_t> reply){

            VERIFY_LOG(reply.err);

            global_core_api->get_setting(profile_id, orion::settings::appearance::web_preview_height,
            [width = reply.value, win_weak, uri, native_x, native_y](orion::GetSettingReply<int64_t> reply){
                VERIFY_LOG(reply.err);

                auto win_strong = win_weak.lock();
                if(!win_strong) return;
                MainWindow* win = MAIN_WINDOW(win_strong.get());

                GdkRectangle rect = {
                    .x = int((native_x>=0)?native_x:main_window_get_mouse_x(win)),
                    .y = int((native_y>=0)?native_y:main_window_get_mouse_y(win)),
                    .width = 0,
                    .height = 0,
                };
                auto* window = GTK_WINDOW(orion_webview_preview_window_new(uri, &rect, win));
                
                GQuark default_width_quark = g_quark_from_static_string("default-width");
                GQuark default_height_quark = g_quark_from_static_string("default-height");
                g_signal_handlers_block_matched(window, G_SIGNAL_MATCH_DETAIL, 0, default_width_quark, NULL, NULL, NULL);
                g_signal_handlers_block_matched(window, G_SIGNAL_MATCH_DETAIL, 0, default_height_quark, NULL, NULL, NULL);
                gtk_window_set_default_size(window, width, reply.value);
                g_signal_handlers_unblock_matched(window, G_SIGNAL_MATCH_DETAIL, 0, default_width_quark, NULL, NULL, NULL);
                g_signal_handlers_unblock_matched(window, G_SIGNAL_MATCH_DETAIL, 0, default_height_quark, NULL, NULL, NULL);
                gtk_window_present(window);
            });
        });
}

static void
suspend_tabs_internal(MainWindow* win, const std::vector<TabId>& tab_ids)
{
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_tabs(tab_ids, [win_weak](orion::GetTabsReply reply){
        VERIFY_LOG(reply.err);

        std::vector<orion::TabDto> filtered_tabs;
        std::copy_if(reply.tabs.begin(), reply.tabs.end(), std::back_inserter(filtered_tabs),
            [](const orion::TabDto& tab){
                bool is_start_page = (tab.uri.empty() || tab.uri == "orion://newtab/");
                return !tab.is_suspended && !is_start_page;
            });

        std::vector<TabId> filtered_ids;
        std::transform(filtered_tabs.begin(), filtered_tabs.end(), std::back_inserter(filtered_ids),
            [](const orion::TabDto& tab){
                return tab.id;
            });

        if (filtered_ids.empty()) {
            // this can happen for example when trying to suspend Start Page tab
            return;
        }

        global_core_api->suspend_tabs(filtered_ids, [win_weak,filtered_ids](orion::Error err){
            VERIFY_LOG(err);
            
            auto win_ptr = win_weak.lock();
            if (!win_ptr) return;
            MainWindow* win = MAIN_WINDOW(win_ptr.get());

            std::set<TabId> suspended_ids(filtered_ids.begin(), filtered_ids.end());

            if (suspended_ids.contains(main_window_get_current_tab_id(win))) {
                // find another unsuspended tab and switch to it
                WindowId window_id = main_window_get_window_id(win);

                global_core_api->get_tab_ids({window_id}, [win_weak,suspended_ids](orion::GetTabIdsReply reply){
                    VERIFY_LOG(reply.err);

                    global_core_api->get_tabs(reply.ids, [win_weak,suspended_ids](orion::GetTabsReply reply){
                        VERIFY_LOG(reply.err);

                        auto win_ptr = win_weak.lock();
                        if (!win_ptr) return;
                        MainWindow* win = MAIN_WINDOW(win_ptr.get());

                        bool tab_found = false;

                        for(const orion::TabDto& tab : reply.tabs)
                        {
                            if(suspended_ids.find(tab.id) == suspended_ids.end() && !tab.is_suspended)
                            {
                                switch_to_tab_internal(win, tab.id);
                                tab_found = true;
                                break;
                            }
                        }

                        if (!tab_found) {
                            // create a new tab in the same window and switch to it
                            orion_webview_tab_view_new_tab(main_window_get_webview_tab_view(win));
                        }
                    });
                });
            }

            AdwTabView* view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));
        
            int len = adw_tab_view_get_n_pages(view);
            for(int i = 0; i<len; i++)
            {
                AdwTabPage* page = adw_tab_view_get_nth_page(view, i);
                OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
                TabId tab_id = orion_webview_tab_container_get_id(container);
                if(suspended_ids.find(tab_id) != suspended_ids.end())
                {
                    // suspend tab's webkit webview
                    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
                    if(content) orion_webview_tab_content_suspend_webview(content);
                }
            }
        });
    });
}

// Closes all UI windows that are owned by the given window_id.
static void
close_window_internal(WindowId window_id)
{
    std::vector<UiWindowId> owned_ui_ids = global_core_api->get_ui_window_ids_owned_by_window_id(window_id);
    if (owned_ui_ids.empty()) {
        orion::log_warning("close_window: no associated UI window for window_id %lu", window_id);
        return;
    }

    for (UiWindowId ui_id : owned_ui_ids) {
        MainWindow* main_window = MAIN_WINDOW(ui_id.value);
        orion::log_verbose("close_window: closing main window %p for window_id %lu", main_window, window_id);
        // this also finalizes the window and that means backend close_window will be called as well
        main_window_close(main_window);
    }
}

// The cb callback will be called with pointer to the MainWindow that was opened or focused.
static void
open_window_in_separate_window_or_switch_to_existing_internal(WindowId window_id, std::function<void(MainWindow*)> cb = nullptr)
{
    // try to find the first UI window for the corresponding window_id
    std::vector<UiWindowId> ui_ids = global_core_api->get_ui_windows_by_window_id(window_id);
    if (ui_ids.empty()) {
        // no UI window for the corresponding window_id exists, create it
        open_window_in_separate_window_internal(window_id, std::move(cb));
    } else {
        MainWindow* win = MAIN_WINDOW(static_cast<void*>(ui_ids.front())); // take the first one
        WeakRefWrapper win_weak(win);

        main_window_focus(win, [win_weak,cb=std::move(cb)]{
            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            if (cb) {
                cb(win);
            }
        });
    }
}

static void
open_recently_closed_tab_internal(MainWindow* win, RecentlyClosedTabId tab_id)
{   
    WeakRefWrapper win_weak(win);

    global_core_api->get_recently_closed_tabs({tab_id}, [win_weak,tab_id](orion::GetRecentlyClosedTabsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.tabs.empty()) return;
        const orion::RecentlyClosedTabDto& tab = reply.tabs.at(0);

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
        orion_webview_tab_view_new_tab_with_uri(view, tab.uri.c_str());

        // remove the tab from recently closed tabs
        DeleteRecentlyClosedTabsQuery query{
            .ids = {tab_id},
            .reset_window_ids = true, // we want to dissolve potential associated recently closed unsaved tab groups
        };
        global_core_api->delete_recently_closed_tabs(query, [](orion::Error err){
            VERIFY_LOG(err);
        });
    });
}

static void
create_tabs_from_recently_closed_tabs(
    const std::vector<RecentlyClosedTabDto>& closed_tabs, WindowId window_id, std::function<void()> done_cb)
{
    if (closed_tabs.empty()) {
        if (done_cb) {
            done_cb();
        }
        return;
    }

    auto done_count = std::make_shared<size_t>(0);

    for (const RecentlyClosedTabDto& closed_tab : closed_tabs) {
        CreateTabQuery query{
            .window_id = window_id,
            .name = closed_tab.name,
            .icon_name = closed_tab.icon_name,
            .uri = closed_tab.uri,
        };

        // note: do not std::move done_cb here, we need it potentially in each create_tab callback
        global_core_api->create_tab(query, [done_count,done_cb,closed_tabs](CreateTabReply reply){
            VERIFY_LOG(reply.err);
            (*done_count)++;
            if (done_cb && *done_count == closed_tabs.size()) {
                done_cb();
            }
        });
    }
}

static void
open_recently_closed_window_internal(MainWindow* win, RecentlyClosedWindowId window_id)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    global_core_api->get_recently_closed_windows({window_id},
        [window_id,profile_id](orion::GetRecentlyClosedWindowsReply reply){
            VERIFY_LOG(reply.err);
            if (reply.windows.empty()) {
                log_warning("open_recently_closed_window: no such recently closed window with id %" PRId64, window_id.value);
                return;
            }

            RecentlyClosedWindowDto closed_window = reply.windows.at(0);

            GetRecentlyClosedTabIdsQuery query{
                .profile_id = profile_id,
                .window_id = window_id,
            };

            global_core_api->get_recently_closed_tab_ids(query,
                [profile_id,window_id](GetRecentlyClosedTabIdsReply reply){
                    VERIFY_LOG(reply.err);

                    global_core_api->get_recently_closed_tabs(reply.ids,
                        [profile_id,window_id](GetRecentlyClosedTabsReply reply){
                            VERIFY_LOG(reply.err);

                            std::vector<RecentlyClosedTabDto> closed_tabs = reply.tabs;

                            CreateWindowQuery query{
                                .profile_id = profile_id,
                                .icon_name = "window-flat-symbolic",
                                .is_tab_group = true,
                            };

                            global_core_api->create_window(query, [window_id,closed_tabs](CreateWindowReply reply){
                                VERIFY_LOG(reply.err);

                                WindowId new_window_id = reply.id;

                                create_tabs_from_recently_closed_tabs(closed_tabs, new_window_id, [window_id,new_window_id]{
                                    open_window_in_separate_window_internal(new_window_id, [window_id](MainWindow* new_win){
                                        global_core_api->delete_recently_closed_windows({window_id}, [](orion::Error err){
                                            VERIFY_LOG(err);
                                        });
                                    });
                                });
                            });
                    });
            });
        });
}

//-------------------------------------------------------------------------
// Profiles Button
//-------------------------------------------------------------------------

static std::vector<MainWindow*>
get_main_windows_for_profile(ProfileId profile_id)
{
    std::vector<MainWindow*> result;

    std::vector<UiWindowId> ui_window_ids = global_core_api->get_registered_ui_window_ids();

    for (const UiWindowId& ui_window_id : ui_window_ids) {
        MainWindow* main_window = MAIN_WINDOW(ui_window_id.value);

        if (main_window_get_current_profile_id(main_window) == profile_id) {
            result.push_back(main_window);
        }
    }

    return result;
}

static void
change_profile_internal(ProfileId profile_id)
{
    log_verbose("change_profile_internal: changing to profile %" PRId64, profile_id.value);

    global_core_api->get_profiles({profile_id}, [profile_id](GetProfilesReply reply){
        VERIFY_LOG(reply.err);
        if (reply.profiles.empty()) {
            log_warning("change_profile: profile not found: %" PRId64, profile_id.value);
            return;
        }
        const ProfileDto& profile = reply.profiles[0];

        // Get all windows for this profile
        global_core_api->get_window_ids({.profile_id=profile_id}, [profile](GetWindowIdsReply reply){
            VERIFY_LOG(reply.err);
            
            if (reply.ids.empty()) {
                profile::open(profile.id);
            } else {
                global_core_api->get_windows(reply.ids, [profile](GetWindowsReply reply){
                    VERIFY_LOG(reply.err);

                    std::vector<MainWindow*> main_windows = get_main_windows_for_profile(profile.id);

                    if (main_windows.empty()) {
                        // No main windows for this profile, open the profile
                        profile::open(profile.id);
                        return;
                    }

                    // Open the selected window first (if it exists), then others
                    WindowId selected_window_id = profile.selected_window_id;
                    bool selected_found = false;
                    
                    for (const WindowDto& window : reply.windows) {
                        if (window.id == selected_window_id) {
                            open_window_in_separate_window_or_switch_to_existing_internal(window.id);
                            selected_found = true;
                            break;
                        }
                    }
                    
                    // Open all other windows which have has_ui_window flag
                    for (const WindowDto& window : reply.windows) {
                        if (window.id != selected_window_id && window.has_ui_window) {
                            open_window_in_separate_window_or_switch_to_existing_internal(window.id);
                        }
                    }
                    
                    // If selected window wasn't found but we have windows, open the first one focused
                    if (!selected_found && !reply.windows.empty()) {
                        open_window_in_separate_window_or_switch_to_existing_internal(reply.windows[0].id);
                    }
                });
            }
        });
    });
}

void
change_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    ProfileId profile_id = static_cast<ProfileId>(g_variant_get_int64(parameter));
    change_profile_internal(profile_id);
}

//-------------------------------------------------------------------------
// Main Menu: Orion
//-------------------------------------------------------------------------

void
about_orion(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	adw_dialog_present(about_dialog_new_adw(), GTK_WIDGET(win));
}

void
get_orion_plus(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	adw_dialog_present(get_orion_plus_dialog_new(), GTK_WIDGET(win));
}

void
popular_extensions(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	adw_dialog_present(popular_extensions_dialog_new(), GTK_WIDGET(win));
}

void
check_for_updates(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	adw_dialog_present(check_for_updates_dialog_new(), GTK_WIDGET(win));
}

void
settings(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    settings_dialog_present(win, -1);
}

void
orion_settings_on_page(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //the real range of pages is much lower anyway, meaning narrowing cast doesn't introduce any more potential unsafe usage, than would happen otherwise
    settings_dialog_present(win, (int)g_variant_get_int64(parameter));
}

void
settings_for_current_page(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_popup_settings_for_website(win);
}

void
reset_orion(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	adw_dialog_present(reset_orion_dialog_new(), GTK_WIDGET(win));
}

void
hide_orion(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    gtk_window_minimize(GTK_WINDOW(win));
}

void
hide_others(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
}

void
quit_orion(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* main_window = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(main_window);
    WeakRefWrapper main_window_weak(main_window);

    global_core_api->get_setting(profile_id, orion::settings::general::show_warning_before_quitting,
        [main_window_weak,profile_id](GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);

            auto main_window_strong = main_window_weak.lock();
            if (!main_window_strong) return;
            MainWindow* main_window = MAIN_WINDOW(main_window_strong.get());

            if (reply.value) {
                // show quit confirmation dialog
                AdwDialog* dialog = quit_orion_dialog_new();
                adw_dialog_present(dialog, GTK_WIDGET(main_window));
            } else {
                // directly quit without confirmation
                //orion_app_quit();
                //TODO: quit only opened windows for current profile
                orion::profile::quit(profile_id);

            }
        }
    );
}

//-------------------------------------------------------------------------
// Main Menu: File
//-------------------------------------------------------------------------

void
new_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	adw_dialog_present(profile_dialog_new_create(), GTK_WIDGET(win));
}

void
edit_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    adw_dialog_present(profile_dialog_new_edit(main_window_get_current_profile_id(win)), GTK_WIDGET(win));
}

void
delete_current_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    delete_profile_internal(main_window_get_current_profile_id(win), win);
}

void
set_current_profile_as_default(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::ProfileId id = main_window_get_current_profile_id(win);
    global_core_api->set_default_profile(id, [](orion::Error err){VERIFY_LOG(err);});
}

//TODO: maybe change these to use context menu action target instead to pass id as the parameter
void
profiles_columnview_switch(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(main_window_get_context_menu_data(win)->user_data);
    ProfileId profile_id = profiles_columnview_item_get_id(item);
    GVariant* var = g_variant_new_int64(profile_id.value);
    change_profile(self, var, win);

    // GValue val = G_VALUE_INIT;
    // g_value_init(&val, G_TYPE_INT64);
    // g_value_set_int64(&val, profiles_columnview_item_get_id(item).value);
    // g_object_set_property(G_OBJECT(win), "profile-id", &val);
}

void
profiles_columnview_edit(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(main_window_get_context_menu_data(win)->user_data);
    adw_dialog_present(profile_dialog_new_edit(profiles_columnview_item_get_id(item)), GTK_WIDGET(win));
}

void
profiles_columnview_delete(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(main_window_get_context_menu_data(win)->user_data);
    orion::ProfileId id = profiles_columnview_item_get_id(item);

    delete_profile_internal(id, win);
}

void
profiles_columnview_set_default(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(main_window_get_context_menu_data(win)->user_data);
    global_core_api->set_default_profile(profiles_columnview_item_get_id(item), [](orion::Error err){VERIFY_LOG(err);});
}

void
new_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabView* tab_view = main_window_get_webview_tab_view(win);
    orion_webview_tab_view_new_tab(tab_view);
}

void
new_private_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::profile::open_new_window(profile_id, profile::FocusMode::no, orion::PrivateMode::yes);
}

void
new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::profile::open_new_window(profile_id, profile::FocusMode::no, orion::PrivateMode::no);
}

void
new_focus_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::profile::open_new_window(profile_id, profile::FocusMode::yes, orion::PrivateMode::no);
}

void
new_private_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::profile::open_new_window(profile_id, profile::FocusMode::no, orion::PrivateMode::yes);
}

static void
on_open_file_ready(GObject* source_object, GAsyncResult* result, gpointer data)
{
	orion::log_info("on_open_file_ready()");

	GError* error_ptr = NULL;

	GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source_object), result, &error_ptr);

	if (error_ptr != NULL) {
		if (error_ptr->code == GTK_DIALOG_ERROR_DISMISSED) {
			orion::log_info("on_open_file_ready: user dismissed the dialog by clicking Cancel button");
		}
		orion::log_error("on_open_file_ready: failed to open file: %s (GError code %d)", error_ptr->message, error_ptr->code);
		g_error_free(error_ptr);
	}

	if (file != NULL) {
		orion::log_info("on_open_file_ready: %s", g_file_peek_path(file));
		g_object_unref(file);
	}
}

void
open_file(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	auto file_dialog = gtk_file_dialog_new(); // file chooser
	gtk_file_dialog_open(file_dialog, GTK_WINDOW(win), NULL, on_open_file_ready, NULL);
	g_object_unref(file_dialog);
}

static void
edit_window_group_internal(orion::WindowId window_id, MainWindow* win, bool delete_window_on_cancel)
{
	// we have to popdown Window List Popover before popping up Save Window popover,
	// the UI would break otherwise
    main_window_popdown_all_popovers(win);

    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_windows({window_id},
        [win_weak,window_id,delete_window_on_cancel](orion::GetWindowsReply reply){

        VERIFY_LOG(reply.err);

        if (reply.windows.empty()) {
            orion::log_error("edit_current_window_group_internal: could not find window with id %ld", window_id.value);
            return;
        }
        orion::WindowDto window = reply.windows.at(0);

        if (window.is_private) {
            orion::log_warning("edit_current_window_group_internal: private window cannot be saved or edited");
            return;
        }
        
        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        SaveWindowPopover* popover = main_window_on_save_window(win);
        
        save_window_popover_set_name(popover, window.name.c_str());
        save_window_popover_select_name(popover);
        save_window_popover_set_icon_name(popover, window.icon_name.c_str());
        save_window_popover_set_keyboard_shortcut(popover, window.keyboard_shortcut.c_str());
        save_window_popover_set_window_id(popover, window_id);
        save_window_popover_set_delete_window_on_cancel(popover, delete_window_on_cancel);
    });
}

void
edit_window_group(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::WindowId window_id = static_cast<orion::WindowId>(g_variant_get_int64(parameter));
    edit_window_group_internal(window_id, win, false);
}

void
edit_current_window_group(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::WindowId window_id = main_window_get_window_id(win);
    edit_window_group_internal(window_id, win, false);
}

void
close_current_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    gtk_window_close(GTK_WINDOW(win));
}

void
close_all_windows(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_close_all_windows(win);
}

void
close_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    AdwTabView* view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));
    AdwTabPage* page = adw_tab_view_get_selected_page(view);
    if(!page) return;
    adw_tab_view_close_page(view, page);
}

void
close_current_tab_tree(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = main_window_get_current_tab_id(win);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    WindowId window_id = main_window_get_window_id(win);
    close_tab_tracking_recent_internal(tab_id, profile_id, window_id, TabType::tree);
}

static void
on_save_page_ready(GObject* source_object, GAsyncResult* result, gpointer data)
{
	orion::log_info("on_save_page_ready()");

	GError* error_ptr = NULL;

	GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source_object), result, &error_ptr);

	if (error_ptr != NULL) {
		if (error_ptr->code == GTK_DIALOG_ERROR_DISMISSED) {
			orion::log_info("on_save_page_ready: user dismissed the dialog by clicking Cancel button");
		}
		orion::log_error("on_save_page_ready: failed to save file: %s (GError code %d)", error_ptr->message, error_ptr->code);
		g_error_free(error_ptr);
	}

	if (file != NULL) {
		orion::log_info("on_save_page_ready: %s", g_file_peek_path(file));
		g_object_unref(file);
	}
}

void
save_page(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	auto file_dialog = gtk_file_dialog_new();
	gtk_file_dialog_save(file_dialog, GTK_WINDOW(win), NULL, on_save_page_ready, NULL);
	g_object_unref(file_dialog);
}

static void
on_export_as_pdf_ready(GObject* source_object, GAsyncResult* result, gpointer data)
{
	orion::log_info("on_export_as_pdf_ready()");

	GError* error_ptr = NULL;

	GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source_object), result, &error_ptr);

	if (error_ptr != NULL) {
		if (error_ptr->code == GTK_DIALOG_ERROR_DISMISSED) {
			orion::log_info("on_export_as_pdf_ready: user dismissed the dialog by clicking Cancel button");
		}
		orion::log_error("on_export_as_pdf_ready: failed to save file: %s (GError code %d)", error_ptr->message, error_ptr->code);
		g_error_free(error_ptr);
	}

	if (file != NULL) {
		orion::log_info("on_export_as_pdf_ready: %s", g_file_peek_path(file));
		g_object_unref(file);
	}
}

void
export_as_pdf(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	auto file_dialog = gtk_file_dialog_new();
	gtk_file_dialog_save(file_dialog, GTK_WINDOW(win), NULL, on_export_as_pdf_ready, NULL);
	g_object_unref(file_dialog);
}

static void
import_bookmarks_html_contents_ready_cb(GObject* source_object, GAsyncResult* result, gpointer data)
{
    orion::log_debug("import_bookmarks_html_contents_ready_cb");

    ImportHtmlBookmarksDialog* dialog = IMPORT_HTML_BOOKMARKS_DIALOG(data);
    bool should_close_dialog = true;

    // Setup deferred dialog close in case of an error
    defer([dialog,&should_close_dialog]{
        if (should_close_dialog) {
            adw_dialog_close(ADW_DIALOG(dialog));
        }
    });

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(dialog));
    if (!root) {
        orion::log_error("import_bookmarks_html_contents_ready_cb: failed to get root for the dialog");
        return;
    }

    MainWindow* win = MAIN_WINDOW(root);
    ProfileId profile_id = main_window_get_current_profile_id(win);

    GError* error = NULL;
    char* contents = NULL;
    gsize length = 0;

    g_file_load_contents_finish(
        G_FILE(source_object),
        result,
        &contents, // contents_out
        &length, // length_out
        NULL, // etag_out
        &error
    );

    if (error) {
        defer([error]{ g_error_free(error); });
        log_error("import_bookmarks_html_contents_ready_cb: failed to read file contents: %s (GError code %d)",
            error->message, error->code
        );
        return;
    }

    defer([contents]{ g_free(contents); });

    std::string html(contents);

    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::floor<std::chrono::days>(now);
    
    // name prefix for the imported bookmark folder
    std::string name_prefix = std::format("{} {:%F}", _("Imported"), today);

    // No error encountered, avoid closing the dialog immediately in defer above
    should_close_dialog = false; 
    orion::gtk::WeakRefWrapper dialog_weak(dialog);

    global_core_api->import_bookmarks_from_html(
        profile_id,
        html,
        name_prefix,
        [dialog_weak](int num_imported, orion::Error err){
            auto dialog_strong = dialog_weak.lock();
            if (!dialog_strong) return;
            ImportHtmlBookmarksDialog* dialog = IMPORT_HTML_BOOKMARKS_DIALOG(dialog_strong.get());

            import_html_bookmarks_dialog_set_result(dialog, num_imported, err);

            VERIFY_LOG(err);
        }
    );
}

static void
import_bookmarks_html_file_ready_cb(GObject* source_object, GAsyncResult* result, gpointer data)
{
	orion::log_debug("import_bookmarks_html_file_ready_cb");
    MainWindow* win = MAIN_WINDOW(data);

	GError* error = NULL;

	GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source_object), result, &error);

	if (error) {
        defer([error]{ g_error_free(error); });
		if (error->code == GTK_DIALOG_ERROR_DISMISSED) {
			orion::log_debug("import_bookmarks_html_file_ready_cb: user dismissed the dialog by clicking Cancel button");
		} else {
		    orion::log_error(
                "import_bookmarks_html_file_ready_cb: failed to open file: %s (GError code %d)",
                error->message, error->code
            );
        }
        return;
	}

    defer([file]{ g_object_unref(file); });

	if (!file) return;

    std::string path = g_file_peek_path(file);
    orion::log_debug("import_bookmarks_html_file_ready_cb: %s", path.c_str());

    // Modal dialog
    AdwDialog* dialog = import_html_bookmarks_dialog_new();
    adw_dialog_present(dialog, GTK_WIDGET(win));

    g_file_load_contents_async(
        file,
        NULL, // cancellable
        import_bookmarks_html_contents_ready_cb,
        dialog // user_data
    );
}

void
import_bookmarks_html_file(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	auto file_dialog = gtk_file_dialog_new();
	gtk_file_dialog_open(file_dialog, GTK_WINDOW(win), NULL, import_bookmarks_html_file_ready_cb, win);
	g_object_unref(file_dialog);
}

namespace {
    struct FileReadData {
        ProfileId profile_id;
        GtkWidget* parent{};
        std::string csv_file_path;
        bool show_open_passwords_button{};
    };
}

static void
on_import_passwords_csv_file_read(GObject* object, GAsyncResult* result, gpointer user_data)
{
    FileReadData* read_data = static_cast<FileReadData*>(user_data);
    defer([read_data]{ delete read_data; });
    
    ProfileId profile_id = read_data->profile_id;
    GtkWidget* parent = read_data->parent;
    std::string csv_file_path = read_data->csv_file_path;
    bool show_open_passwords_button = read_data->show_open_passwords_button;

    GError* error = NULL;
    gchar* contents = NULL;
    gsize length = 0;

    gboolean success = g_file_load_contents_finish(
        G_FILE(object),
        result,
        &contents,
        &length,
        NULL, // etag_out
        &error
    );
    defer([contents]{ g_free(contents); });
    if (error) {
        defer([error]{ g_error_free(error); });
        log_error("on_import_passwords_csv_file_read: failed to read file contents: %s (GError code %d)",
            error->message, error->code
        );
        return;
    }
    log_info("on_import_passwords_csv_file_read: read %zu bytes, success: %d", length, success);

    WeakRefWrapper parent_weak(parent);

    std::string csv_text(contents, length);
    ImportPasswordItemsFromCsvQuery query{
        .profile_id = profile_id,
        .csv_text = csv_text,
    };

    global_core_api->import_password_items_from_csv(query,
        [parent_weak,csv_file_path,show_open_passwords_button](ImportPasswordItemsFromCsvReply reply){
            VERIFY_LOG(reply.err);

            auto parent_strong = parent_weak.lock();
            if (!parent_strong) return;
            GtkWidget* parent = GTK_WIDGET(parent_strong.get());

            log_info("on_import_passwords_csv_file_read: imported=%zu not_imported=%zu",
                reply.imported.size(), reply.not_imported.size()
            );

            if (reply.not_imported.empty()) {
                uint n_imported = static_cast<uint>(reply.imported.size());
                AdwDialog* dialog = delete_csv_passwords_dialog_new_for_import_result(n_imported, csv_file_path.c_str());
                adw_dialog_present(dialog, parent);
            } else {
                // some items were not imported, show the dialog with details
                ImportCsvPasswordsDialogOptions options{
                    .n_imported = static_cast<uint>(reply.imported.size()),
                    .not_imported_items = reply.not_imported,
                    .show_open_passwords_button = show_open_passwords_button,
                };
                AdwDialog* dialog = import_csv_passwords_dialog_new_for_import_result(options);
                adw_dialog_present(dialog, parent);
            }
        });
}

struct FileChosenData {
    MainWindow* main_window;
    bool show_open_passwords_button;
};

static void
on_import_passwords_csv_file_chosen(GObject* object, GAsyncResult* result, gpointer user_data)
{
    FileChosenData* data = static_cast<FileChosenData*>(user_data);
    defer([data]{ delete data; });

    MainWindow* main_window = data->main_window;
    bool show_open_passwords_button = data->show_open_passwords_button;

	GError* error = NULL;
	GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(object), result, &error);
	if (error) {
        defer([error]{ g_error_free(error); });
		if (error->code == GTK_DIALOG_ERROR_DISMISSED) {
			log_info("on_import_passwords_csv_file_chosen: user dismissed the dialog by clicking Cancel button");
		}
		log_error("on_import_passwords_csv_file_chosen: failed to open file: %s (GError code %d)",
            error->message, error->code
        );
        return;
	}
    defer([file]{ g_object_unref(file); });
    
    log_info("on_import_passwords_csv_file_chosen: %s", g_file_peek_path(file));

    FileReadData* read_data = new FileReadData{
        .profile_id = main_window_get_current_profile_id(main_window),
        .parent = GTK_WIDGET(main_window),
        .csv_file_path = g_file_peek_path(file),
        .show_open_passwords_button = show_open_passwords_button,
    };
    
    g_file_load_contents_async(
        file,
        NULL, // cancellable
        on_import_passwords_csv_file_read,
        read_data // user_data
    );
}

static void
import_passwords_csv_file_internal(MainWindow* main_window, bool show_passwords_button)
{
    FileChosenData* data = new FileChosenData{
        .main_window = main_window,
        .show_open_passwords_button = show_passwords_button,
    };

	GtkFileDialog* file_dialog = gtk_file_dialog_new();

	gtk_file_dialog_open(
        file_dialog,
        GTK_WINDOW(main_window),
        NULL, // cancellable
        on_import_passwords_csv_file_chosen,
        data // user_data
    );
	defer([file_dialog]{ g_object_unref(file_dialog); });
}

void
import_passwords_csv_file_main_menu(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    import_passwords_csv_file_internal(MAIN_WINDOW(user_data), /* show_passwords_button */ true);
}

void
import_passwords_csv_file(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    import_passwords_csv_file_internal(MAIN_WINDOW(user_data), /* show_passwords_button */ false);
}

static void
present_import_profile_data_dialog(MainWindow* main_window, const std::string& source_directory, orion::ProfileId profile_id)
{
    AdwDialog* dialog = import_profile_data_dialog_new_for_file_name(source_directory.c_str(), profile_id);
    adw_dialog_present(dialog, GTK_WIDGET(main_window));
}

static void
present_create_import_profile_dialog(MainWindow* main_window, const std::string& source_directory)
{
    orion::gtk::WeakRefWrapper main_window_weak(main_window);
    AdwDialog* dialog = profile_dialog_new_import([main_window_weak, source_directory](orion::CreateProfileReply reply){
        if (reply.err) {
            VERIFY_LOG(reply.err);
            return;
        }

        auto main_window_ref = main_window_weak.lock();
        if (!main_window_ref) {
            return;
        }

        MainWindow* main_window = MAIN_WINDOW(main_window_ref.get());
        present_import_profile_data_dialog(main_window, source_directory, reply.id);
    });
    adw_dialog_present(dialog, GTK_WIDGET(main_window));
}

static void
on_import_profile_data_directory_chosen(GObject* object, GAsyncResult* result, gpointer user_data)
{
    MainWindow* main_window = MAIN_WINDOW(user_data);

    GError* error = NULL;
    GFile* file = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(object), result, &error);
    if (error) {
        defer([error]{ g_error_free(error); });
        if (error->code == GTK_DIALOG_ERROR_DISMISSED) {
            log_info("on_import_profile_data_directory_chosen: user dismissed the dialog by clicking Cancel button");
            return;
        }
        log_error("on_import_profile_data_directory_chosen: failed to select folder: %s (GError code %d)",
            error->message, error->code
        );
        return;
    }
    defer([file]{ g_object_unref(file); });

    gchar* source_directory = g_file_get_path(file);
    if (!source_directory) {
        source_directory = g_file_get_parse_name(file);
    }
    defer([source_directory]{ g_free(source_directory); });

    if (!source_directory || source_directory[0] == '\0') {
        log_error("on_import_profile_data_directory_chosen: failed to read selected folder path");
        return;
    }

    log_info("on_import_profile_data_directory_chosen: %s", source_directory);
    present_create_import_profile_dialog(main_window, source_directory);
}

void
import_profile_data(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GtkFileDialog* file_dialog = gtk_file_dialog_new();
    gtk_file_dialog_select_folder(
        file_dialog,
        GTK_WINDOW(win),
        NULL, // cancellable
        on_import_profile_data_directory_chosen,
        win // user_data
    );
    defer([file_dialog]{ g_object_unref(file_dialog); });
}

static void
on_export_profile_data_file_chosen(GObject* object, GAsyncResult* result, gpointer user_data)
{
    MainWindow* main_window = MAIN_WINDOW(user_data);

    GError* error = NULL;
    GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(object), result, &error);
    if (error) {
        defer([error]{ g_error_free(error); });
        if (error->code == GTK_DIALOG_ERROR_DISMISSED) {
            log_info("on_export_profile_data_file_chosen: user dismissed the dialog by clicking Cancel button");
            return;
        }
        log_error("on_export_profile_data_file_chosen: failed to save file: %s (GError code %d)",
            error->message, error->code
        );
        return;
    }
    defer([file]{ g_object_unref(file); });

    gchar* file_name = g_file_get_path(file);
    if (!file_name) {
        file_name = g_file_get_parse_name(file);
    }
    defer([file_name]{ g_free(file_name); });

    if (!file_name) {
        log_error("on_export_profile_data_file_chosen: failed to read file path from selected file");
        return;
    }

    log_info("on_export_profile_data_file_chosen: %s", file_name);

    AdwDialog* dialog = export_profile_data_dialog_new_for_file_name(
        file_name, main_window_get_current_profile_id(main_window)
    );
    adw_dialog_present(dialog, GTK_WIDGET(main_window));
}

void
export_profile_data(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	GtkFileDialog* file_dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_initial_name(file_dialog, "profile");
	gtk_file_dialog_save(
        file_dialog,
        GTK_WINDOW(win),
        NULL, // cancellable
        on_export_profile_data_file_chosen,
        win // user_data
    );
	defer([file_dialog]{ g_object_unref(file_dialog); });
}

static void
export_bookmarks_write_ready_cb(GObject* object, GAsyncResult* result, gpointer data)
{
    GError* error = NULL;
    gboolean success = g_file_replace_contents_finish(
        G_FILE(object),
        result,
        NULL, // new_etag
        &error
    );
    if (error) {
        defer([error]{ g_error_free(error); });
        orion::log_error("export_bookmarks_file_dialog_save_cb: error writing HTML to file: %s", error->message);
        return;
    }
    orion::log_debug("export_bookmarks_file_dialog_save_cb: write HTML to file result: %d", success);
}

static void
export_bookmarks_file_dialog_save_cb(GObject* source_object, GAsyncResult* result, gpointer data)
{
    orion::log_debug("export_bookmarks_file_dialog_save_cb");
    MainWindow* win = MAIN_WINDOW(data);
    ProfileId profile_id = main_window_get_current_profile_id(win);

    GError* error = NULL;

    GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source_object), result, &error);

    if (error) {
        defer([error]{ g_error_free(error); });
        if (error->code == GTK_DIALOG_ERROR_DISMISSED) {
            orion::log_debug("export_bookmarks_file_dialog_save_cb: user dismissed the dialog by clicking Cancel button");
        } else {
            orion::log_error(
                "export_bookmarks_file_dialog_save_cb: failed to save file: %s (GError code %d)",
                error->message, error->code
            );
        }
        return;
    }

    defer([file]{ g_object_unref(file); });
    if (!file) return;

    std::string path = g_file_peek_path(file);
    orion::log_info("export_bookmarks_file_dialog_save_cb: %s", path.c_str());

    global_core_api->export_bookmarks_to_html(
        profile_id,
        [profile_id,path](const std::string& html, orion::Error err){
            VERIFY_LOG(err);
            //printf("%s\n", html.c_str());
            GFile* file = g_file_new_for_path(path.c_str());
            defer([file]{ g_object_unref(file); });

            GBytes* contents = g_bytes_new(html.c_str(), html.size());
            defer([contents]{ g_bytes_unref(contents); });

            // Note that if we use g_file_replace_contents_async instead, then we have to make sure
            // that contents buffer is kept alive until the finish callback is called. So calling
            // g_file_replace_contents_bytes_async is more convenient.

            g_file_replace_contents_bytes_async(
                file,
                contents,
                NULL, // etag
                FALSE, // make_backup
                G_FILE_CREATE_REPLACE_DESTINATION,
                NULL, // cancellable
                export_bookmarks_write_ready_cb, // callback
                NULL // user_data
            );
        }
    );
}

void
export_bookmarks(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	GtkFileDialog* file_dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_initial_name(file_dialog, "orion-linux-bookmarks.html");
    defer([file_dialog] { g_object_unref(file_dialog); });
	gtk_file_dialog_save(file_dialog, GTK_WINDOW(win), NULL, export_bookmarks_file_dialog_save_cb, win);
}

static void
on_passwords_export_csv_bytes_written(GObject* object, GAsyncResult* result, gpointer user_data)
{
    GFile* file = G_FILE(object); // do not unref here
    GError* error = nullptr;
    gboolean success = g_file_replace_contents_finish(
        file,
        result,
        NULL, // new_etag
        &error
    );
    if (error) {
        defer([error]{ g_error_free(error); });
        orion::log_error("on_passwords_export_csv_bytes_written: error writing CSV bytes to file: %s", error->message);
        return;
    }
    orion::log_debug("on_passwords_export_csv_bytes_written: write CSV bytes to file result: %d", success);
}

static void
on_passwords_export_finished(GObject* object, GAsyncResult* result, gpointer user_data)
{
    GtkWindow* parent = GTK_WINDOW(user_data);
    GtkFileDialog* dialog = GTK_FILE_DIALOG(object);
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_save_finish(dialog, result, &error);
    if (error) {
        defer([error]{ g_error_free(error); });
        orion::log_error("on_passwords_export_finished: error saving passwords export file: %s", error->message);
        return;
    }
    // do not unref file in this scope, it is needed in callbacks

    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(parent)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);

    orion::GetPasswordItemIdsQuery query{
        .profile_id = profile_id,
    };
    global_core_api->get_password_item_ids(query, [file](orion::GetPasswordItemIdsReply reply){
        VERIFY_LOG_ERR_DEFER(reply.err, g_object_unref(file)); // defer unref only on error

        global_core_api->get_password_items_csv_export(reply.ids, [file](orion::GetPasswordItemsCsvExportReply reply){
            defer([file]{ g_object_unref(file); }); // defer unref unconditionally
            VERIFY_LOG(reply.err);

            char* path = g_file_get_path(file);
            defer([path]{ g_free(path); });
            
            orion::log_debug("on_passwords_export_finished: exporting to path: %s", path);

            // csv_bytes must be unreffed after the async operation is done
            GBytes* csv_bytes = g_bytes_new(
                reply.csv_text.data(),
                reply.csv_text.size()
            );
            defer([csv_bytes]{ g_bytes_unref(csv_bytes); });

            // Write content asynchronously
            g_file_replace_contents_bytes_async(
                file,
                csv_bytes,
                nullptr, /* etag */
                FALSE,   /* make_backup */
                G_FILE_CREATE_REPLACE_DESTINATION,
                nullptr, /* cancellable */
                on_passwords_export_csv_bytes_written, /* callback */
                nullptr  /* user_data */
            );
        });
    });
}

void
export_passwords(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
	GtkWindow* parent = GTK_WINDOW(MAIN_WINDOW(user_data));
    GtkFileDialog* file_dialog = gtk_file_dialog_new();
    defer([file_dialog]{ g_object_unref(file_dialog); });

    gtk_file_dialog_set_initial_name(file_dialog, "passwords.csv");

    gtk_file_dialog_save(
        file_dialog,
        parent,
        NULL, /* cancellable */
        on_passwords_export_finished,
        parent /* user_data */
    );
}

void
page_setup(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
	// TODO: use GtkPrintOperation maybe? Page Setup on Mac is a system dialog
}

void
print(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	auto print_dialog = gtk_print_dialog_new();
	gtk_print_dialog_setup(print_dialog, GTK_WINDOW(win), NULL, NULL, NULL); // TODO: fill callback
	g_object_unref(print_dialog);
}

void
manage_profiles(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    auto dialog = bookmarks_dialog_new();
    bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(dialog), 5);
    adw_dialog_present(dialog, GTK_WIDGET(win));
}

//-------------------------------------------------------------------------
// Main Menu: Edit
//-------------------------------------------------------------------------

void
undo_tab_close(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ChangeUndoRedoTabStepQuery query{
        .window_id = main_window_get_window_id(win),
        .is_undo = true,
    };
    global_core_api->change_undo_redo_tab_step(query, [](ChangeUndoRedoTabStepReply reply){
        VERIFY_LOG(reply.err);
    });
}

void
redo_tab_close(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ChangeUndoRedoTabStepQuery query{
        .window_id = main_window_get_window_id(win),
        .is_undo = false,
    };
    global_core_api->change_undo_redo_tab_step(query, [](ChangeUndoRedoTabStepReply reply){
        VERIFY_LOG(reply.err);
    });
}

void
cut(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
}

void
copy(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
}

static orion::Uri
get_current_page_uri(MainWindow* win)
{
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    orion::Uri uri = orion_webview_tab_view_get_uri(view);
    return uri;
}

static std::string
get_current_page_title(MainWindow* win)
{
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    const char* title = orion_webview_tab_view_get_title(view);
    return title ? std::string(title) : std::string();
}

void
copy_page_url(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::Uri uri = get_current_page_uri(win);
    
    GdkDisplay* display = gdk_display_get_default();
    GdkClipboard* clipboard = gdk_display_get_clipboard(display);
    gdk_clipboard_set_text(clipboard, uri.uri().c_str());
}

void
copy_url_as_markdown(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::Uri uri = get_current_page_uri(win);
    std::string title = get_current_page_title(win);
    std::string markdown = std::format("[{}]({})", title, uri.uri());

    GdkDisplay* display = gdk_display_get_default();
    GdkClipboard* clipboard = gdk_display_get_clipboard(display);
    gdk_clipboard_set_text(clipboard, markdown.c_str());
}

void
paste_and_go(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GdkDisplay* display = gdk_display_get_default();
    GdkClipboard* clipboard = gdk_display_get_clipboard(display);

    gdk_clipboard_read_text_async(
        clipboard, nullptr, [](GObject* source_object, GAsyncResult* res, gpointer user_data) {
            GdkClipboard* clipboard = GDK_CLIPBOARD(source_object);
            char* text = gdk_clipboard_read_text_finish(clipboard, res, nullptr);
            defer([text] { g_free(text); });
            if (text) {
                MainWindow* win = MAIN_WINDOW(user_data);
                auto content = main_window_get_current_tab_content(win);
                orion::Uri uri = orion::Uri::parse(text);
                orion_webview_tab_content_load_uri(content, uri);
            }
        }, user_data);
}

void
paste(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
}

void
paste_and_match_style(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
}

void
select_all(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
}

void
find(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_show_find_bar(win);
}

void
find_next(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    if (!main_window_get_find_bar_visible(win))
    {
        main_window_show_find_bar(win);
        return;
    }
    OrionWebviewTabContent* content = main_window_get_current_tab_content(win);
    if (content) orion_webview_tab_content_find_next(content);
}

void
find_previous(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    if (!main_window_get_find_bar_visible(win))
    {
        main_window_show_find_bar(win);
        return;
    }
    OrionWebviewTabContent* content = main_window_get_current_tab_content(win);
    if (content) orion_webview_tab_content_find_previous(content);
}

void
find_ignore_case(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    GVariant* state = g_action_get_state(G_ACTION(self));
    gboolean new_state = !g_variant_get_boolean(state);
    g_variant_unref(state);
    g_simple_action_set_state(self, g_variant_new_boolean(new_state));

    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_refresh_find_search(win);
}

void
find_wrap_around(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    GVariant* state = g_action_get_state(G_ACTION(self));
    gboolean new_state = !g_variant_get_boolean(state);
    g_variant_unref(state);
    g_simple_action_set_state(self, g_variant_new_boolean(new_state));

    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_refresh_find_search(win);
}

void
find_match_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    const char* selected = g_variant_get_string(parameter, NULL);
    g_simple_action_set_state(self, g_variant_new_string(selected));

    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_refresh_find_search(win);
}

void
use_selection_for_find(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabContent* content = main_window_get_current_tab_content(win);
    if (!content) return;

    WeakRefWrapper win_weak(win);
    orion_webview_tab_content_get_selected_text(content, [win_weak](std::string text){
        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        if (text.empty()) return;

        main_window_show_find_bar_with_text(MAIN_WINDOW(win_strong.get()), text);
    });
}

void
jump_to_selection(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabContent* content = main_window_get_current_tab_content(win);
    if (!content) return;

    orion_webview_tab_content_scroll_to_selection(content);
}

void
show_spelling_and_grammar(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
	// TODO: Mac has a system dialog for this
}

void
check_document_now(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
	// TODO: Mac-specific
}

void
check_spelling_while_typing(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
}

void
check_grammar_with_spelling(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{    
}

void
correct_spelling_automatically(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
}

void
smart_copy_paste(GSimpleAction* action, GVariant* parameter, gpointer user_data) {}

void
smart_quotes(GSimpleAction* action, GVariant* parameter, gpointer user_data) {}

void
smart_dashes(GSimpleAction* action, GVariant* parameter, gpointer user_data) {}

void
smart_links(GSimpleAction* action, GVariant* parameter, gpointer user_data) {}

void
data_detectors(GSimpleAction* action, GVariant* parameter, gpointer user_data) {}

void
text_replacement(GSimpleAction* action, GVariant* parameter, gpointer user_data) {}

//-------------------------------------------------------------------------
// Main Menu: View
//-------------------------------------------------------------------------

void
always_show_toolbar_in_fullscreen(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);

    global_core_api->toggle_setting(profile_id, settings::appearance::always_show_toolbar_in_fullscreen,
        [](SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        }
    );
}

void
always_show_tab_bar(GSimpleAction* action, GVariant* parameter, gpointer user_data) {}

void
bookmarks_bar_visibility(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GVariant* state = g_action_get_state(G_ACTION(self));
    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::appearance::show_bookmarks_bar, !g_variant_get_boolean(state),
        [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
    g_clear_pointer(&state, g_variant_unref);
}

void
toggle_show_status_bar_on_link_hover(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);

    global_core_api->toggle_setting(profile_id, settings::appearance::show_status_bar_on_link_hover,
        [](SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        }
    );
}

void
toggle_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    gboolean show_sidebar = get_boolean_action_state(G_ACTION_MAP(win), "toggle-sidebar");
    main_window_toggle_show_sidebar_setting(win);

    std::string page_name = get_string_action_state(G_ACTION_MAP(win), "sidebar-type");
    settings::SidebarPage page_type = sidebar_page_name_to_page_type(page_name);
    std::string toggle_action = sidebar_page_type_to_toggle_action(page_type);

    Sidebar* sidebar = main_window_get_sidebar(win);
    sidebar_deactivate_other_page_toggles(sidebar, toggle_action.c_str());
    orion::gtk::set_boolean_action_state(G_ACTION_MAP(win), toggle_action.c_str(), !show_sidebar);
}

static void
toggle_sidebar_page(MainWindow* win, const char* target_page, const char* action_name)
{
    toggle_action_state(G_ACTION_MAP(win), action_name);
    Sidebar* sidebar = main_window_get_sidebar(win);
    sidebar_deactivate_other_page_toggles(sidebar, action_name);

    gboolean show_sidebar = get_boolean_action_state(G_ACTION_MAP(win), "toggle-sidebar");
    std::string current_page = get_string_action_state(G_ACTION_MAP(win), "sidebar-type");

    log_debug("toggle_sidebar_page: show_sidebar=%d page=%s->%s", show_sidebar, current_page.c_str(), target_page);

    if (show_sidebar && current_page == target_page) {
        // We should hide the sidebar
        main_window_toggle_show_sidebar_setting(win);
    } else {
        // We should ensure the sidebar is shown and switch to the target page
        main_window_switch_to_sidebar_page(win, target_page);
        if (!show_sidebar) {
            main_window_toggle_show_sidebar_setting(win);
        }
    }
}

void
toggle_windows_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    toggle_sidebar_page(win, "windows", "toggle-windows-sidebar");
}

void
toggle_vertical_tabs_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    toggle_sidebar_page(win, "tabs", "toggle-vertical-tabs-sidebar");
}

void
toggle_bookmarks_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    toggle_sidebar_page(win, "bookmarks", "toggle-bookmarks-sidebar");
}

void
toggle_reading_list_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    toggle_sidebar_page(win, "reading_list", "toggle-reading-list-sidebar");
}

void
toggle_reader(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_toggle_reader(win);
}

void
exit_reader_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_exit_reader_mode(win);
}

void
show_tab_overview(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_show_tab_overview(win);
}

void
show_downloads(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    adw_dialog_present(downloads_dialog_new(), GTK_WIDGET(user_data));
}

void
open_file_by_path(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    size_t length = 0;
    const char* path = g_variant_get_string(parameter, &length);
    GFile* file = g_file_new_for_path(path);
    defer([file]{ g_object_unref(file); });
    GtkFileLauncher* launcher = gtk_file_launcher_new(file);
    defer([launcher]{ g_object_unref(launcher); });
    gtk_file_launcher_launch(launcher, NULL, NULL, NULL, NULL);
}

void
open_download(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::DownloadId download_id(g_variant_get_int64(parameter));
    global_core_api->get_downloads({download_id}, [](orion::GetDownloadsReply reply){
        VERIFY_LOG(reply.err);
        if(!reply.downloads.size())
        {
            orion::log_error("open_download(): failed to find download");
        }
        else
        {
            auto& download = reply.downloads.at(0);
            if(download.operation_state == orion::DownloadOperationState::finished)
            {
                std::filesystem::path path = std::filesystem::path(download.path)/download.name;
                GFile* file = g_file_new_for_path(path.c_str());
                defer([file]{ g_object_unref(file); });
                GtkFileLauncher* launcher = gtk_file_launcher_new(file);
                defer([launcher]{ g_object_unref(launcher); });
                gtk_file_launcher_launch(launcher, NULL, NULL, NULL, NULL);
            }
            else if(download.operation_state == orion::DownloadOperationState::downloading)
            {
                OrionApp* app = ORION_APP(g_application_get_default());
                orion_app_schedule_download_for_opening(app, download.id);
            }
        }
    });
}

void
show_file_in_file_manager(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    size_t length = 0;
    const char* full_path = g_variant_get_string(parameter, &length);
    if(!full_path)
    {
        orion::log_error("show_file_in_file_manager: triggered with a NULL string");
    }

    std::filesystem::path path = full_path;
    orion::log_verbose("show_in_file_manager: attempting to show for path %s", path.c_str());
    std::error_code ec{};
    //TODO: redo this using async operations to avoid blocking ui thread
    bool file_exists = std::filesystem::exists(path, ec);
    if(ec)
    {
        orion::log_error("show_in_file_manager(): failed to check if file exists, message: %s", ec.message().c_str());
        return;
    }
    if(file_exists)
    {
        GFile* file = g_file_new_for_path(path.c_str());
        defer([file]{ g_object_unref(file); });
        GtkFileLauncher* launcher = gtk_file_launcher_new(file);
        defer([launcher]{ g_object_unref(launcher); });
        gtk_file_launcher_open_containing_folder(launcher, NULL, NULL, NULL, NULL);
    }
    else
    {
        bool is_directory = std::filesystem::is_directory(path.parent_path(), ec);
        if(!is_directory || ec)
        {
            orion::log_debug("show_in_file_manager(): failed parent path is directory check");
            if(ec) orion::log_debug("std::filesystem::is_directory error message: %s", ec.message().c_str());
            return;
        }
        GFile* file = g_file_new_for_path(path.parent_path().c_str());
        defer([file]{ g_object_unref(file); });
        GtkFileLauncher* launcher = gtk_file_launcher_new(file);
        defer([launcher]{ g_object_unref(launcher); });
        gtk_file_launcher_launch(launcher, NULL, NULL, NULL, NULL);
    }
}

void
remove_download_item(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::DownloadId id(g_variant_get_int64(parameter));
    orion::DeleteDownloadsQuery query{
        .ids = {id},
    };
    global_core_api->delete_downloads(query, [](orion::DeleteDownloadsReply reply){ VERIFY_LOG(reply.err); });
}

void
stop_download(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::DownloadId id(g_variant_get_int64(parameter));
    OrionApp* app = ORION_APP(g_application_get_default());
    auto download = orion_app_get_downloader_for_id(app, id);
    if(const std::shared_ptr<orion::Downloader>* downloader = std::get_if<std::shared_ptr<orion::Downloader>>(&download))
    {
        (*downloader)->cancel();
    }
    else if(const orion::gtk::WeakRefWrapper* download_weak = std::get_if<orion::gtk::WeakRefWrapper>(&download))
    {
        auto download_strong = download_weak->lock();
        if(download_strong)
        {
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
            orion::UpdateDownloadQuery query{};
            query.id = id;
            query.update_operation_state = true;
            query.operation_state = orion::DownloadOperationState::cancelling;
            global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
            webkit_download_cancel(WEBKIT_DOWNLOAD(download_strong.get()));
#endif
        }
    }
}

void
resume_download(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //TODO: resume instead of redoing download - complex, somewhat low priority
    orion::DownloadId id(g_variant_get_int64(parameter));

    global_core_api->get_downloads({id}, [](orion::GetDownloadsReply reply){
        VERIFY_LOG(reply.err);

        if(!reply.downloads.size())
        {
            return;
        }
        orion::DownloadDto& download = reply.downloads.at(0);

        std::shared_ptr<orion::Downloader> downloader = global_core_api->create_downloader();
        downloader->set_uri(download.origin_uri);
        auto progress_cb = [id = download.id](const orion::Downloader::Progress& progress){
            orion::UpdateDownloadQuery query{};
            query.id = id;
            query.update_total_size = true;
            query.total_size = std::max(progress.content_length, progress.total_received);
            query.update_current_size = true;
            query.current_size = progress.total_received;
            query.update_operation_state = true;
            query.operation_state = orion::DownloadOperationState::downloading;
            orion::log_verbose("downloader progress cb: cr %zu, cl %zu, ep %lf, tr %zu", progress.chunk_received, progress.content_length, progress.estimated_progress, progress.total_received);
            global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
        };
        downloader->set_progress_cb(progress_cb);
        auto finished_cb = [id = download.id](orion::Error err){

            auto* id_ptr = new orion::DownloadId(id);
            //deferring destruction of downloader so that it does not happen while inside its callback
            g_idle_add_full(G_PRIORITY_DEFAULT, [](gpointer user_data){
                
                auto* id_ptr = static_cast<orion::DownloadId*>(user_data);
                OrionApp* app = ORION_APP(g_application_get_default());
                orion_app_remove_download_for_id(app, *id_ptr);
                return G_SOURCE_REMOVE;

            }, id_ptr, [](gpointer data){

                delete static_cast<orion::DownloadId*>(data);
            });

            orion::UpdateDownloadQuery query{};
            query.id = id;
            if(err)
            {
                query.operation_state = orion::DownloadOperationState::failed;
                query.update_operation_state = true;
            }
            else
            {
                query.operation_state = orion::DownloadOperationState::finished;
                query.update_operation_state = true;
            }
            global_core_api->update_downloads({query}, [](orion::Error err){ VERIFY_LOG(err); });
            VERIFY_LOG(err);
        };
        downloader->set_finished_cb(finished_cb);


        downloader->set_target_path(std::filesystem::path(download.path) / download.name); //redo this
        downloader->set_overwrite_existing(true);
        downloader->start_download();
        OrionApp* app = ORION_APP(g_application_get_default());
        orion_app_add_download_with_id(app, downloader, download.id);
    });
}

// Reader Mode


void
reader_mode_font_size_index(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);

    int64_t val = g_variant_get_int64(parameter);
    val = std::clamp(val, 0l, 11l);

    global_core_api->set_setting(
        main_window_get_current_profile_id(win),
        orion::settings::reader_mode::font_size_index,
        val,
        [](orion::SetSettingReply<int64_t> reply){
            VERIFY_LOG(reply.err);
        }
    );
}

void
reader_mode_background_type(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);

    auto val = static_cast<orion::settings::ReaderModeBackgroundType>(g_variant_get_int64(parameter));

    global_core_api->set_setting(
        main_window_get_current_profile_id(win),
        orion::settings::reader_mode::background_type,
        val,
        [](orion::SetSettingReply<orion::settings::ReaderModeBackgroundType> reply){
            VERIFY_LOG(reply.err);
        }
    );
}

void
reader_mode_font_name(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);

    int64_t val = g_variant_get_int64(parameter);

    global_core_api->set_setting(
        main_window_get_current_profile_id(win),
        orion::settings::reader_mode::font_name,
        orion::settings::Font(val),
        [](orion::SetSettingReply<orion::settings::Font> reply){
            VERIFY_LOG(reply.err);
        }
    );
}

void
reader_mode_document_width(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);

    int64_t val = g_variant_get_int64(parameter);

    global_core_api->set_setting(
        main_window_get_current_profile_id(win),
        orion::settings::reader_mode::font_size_index,
        val,
        [](orion::SetSettingReply<int64_t> reply){
            VERIFY_LOG(reply.err);
        }
    );
}

void
reader_mode_text_alignment(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);

    auto val = static_cast<orion::settings::TextAlignment>(g_variant_get_int64(parameter));

    global_core_api->set_setting(
        main_window_get_current_profile_id(win),
        orion::settings::reader_mode::text_alignment,
        val,
        [](orion::SetSettingReply<orion::settings::TextAlignment> reply){
            VERIFY_LOG(reply.err);
        }
    );
}

void
reader_mode_border_shadow_enabled(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);

    auto val = static_cast<orion::settings::ReaderModeBorderShadowEnabled>(g_variant_get_int64(parameter));

    global_core_api->set_setting(
        main_window_get_current_profile_id(win),
        orion::settings::reader_mode::border_shadow_enabled,
        val,
        [](orion::SetSettingReply<orion::settings::ReaderModeBorderShadowEnabled> reply){
            VERIFY_LOG(reply.err);
        }
    );
}


void
toggle_focus_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);

    GVariant* state = g_action_get_state(G_ACTION(self));
    defer([state]{ g_variant_unref(state); });
    gboolean is_focus_mode = g_variant_get_boolean(state);

    UpdateWindowQuery query{
        .id = main_window_get_window_id(win),
        .is_focus_mode = !is_focus_mode,
        .update_is_focus_mode = true,
    };

    global_core_api->update_window(query, [](Error err){
        VERIFY_LOG(err);
    });
}

// This handler is used when Escape accelerator is triggered
void
stop(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);

    if (main_window_get_find_bar_visible(win))
    {
        main_window_hide_find_bar(win);
        return;
    }

    main_window_stop_loading_current_tab(win);

    // When AdwTabOverview is open and user presses Escape, we want to close it
    main_window_hide_tab_overview(win);
    // forced to put this here because the global accel consumes all Escape keypresses seemingly
    // seems like gtk_application_set_accels_for_action just puts a capture phase GtkShortcutController on every GtkApplicationWindow
    // TODO: part this whole callback out 
    OrionWebviewTabView* tab_view = main_window_get_webview_tab_view(win);
    if(tab_view && gtk_widget_get_focus_child(GTK_WIDGET(tab_view)))
    {
        orion_webview_tab_view_exit_reader_mode(tab_view);
    }
}

void
reload_this_page(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::TabId tab_id = main_window_get_current_tab_id(win);
    main_window_reload_tab_by_tab_id(win, tab_id);
}

void
reload_current_page_bypass_cache(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_reload_current_tab_bypass_cache(win);
}

void
reload_all_tabs_in_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_reload_all_tabs(win);
}

void
show_picture_in_picture_video(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
}

void
reset_zoom(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_reset_zoom(win);
}

void
show_blocked_popups(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabContent* content = orion_webview_tab_view_get_current_tab_content(
        main_window_get_webview_tab_view(win));
    if (!content) return;

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    std::vector<std::string> urls = orion_webview_tab_content_get_blocked_popup_urls(content);
    if (urls.empty()) return;

    orion_webview_tab_content_clear_blocked_popups(content);
    main_window_update_popup_blocked_button(win, 0);

    for (const auto& url : urls)
        open_uri_in_new_window_internal(url, win);
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
}

void
zoom_in(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_zoom_in(win);
}

void
zoom_out(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_zoom_out(win);
}

void
toggle_full_screen(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::log_debug("toggle_full_screen: action triggered from menu (or by F11)");
    if (main_window_get_is_webkit_fullscreen(win)) {
        orion::log_debug("toggle_full_screen: webkit fullscreen is active, nothing to do");
        return;
    }
    gboolean borderless = main_window_get_is_borderless(win);
    main_window_set_is_borderless(win, !borderless, FALSE);
}

//-------------------------------------------------------------------------
// Main Menu: History
//-------------------------------------------------------------------------

void
show_history(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    AdwDialog* bookmarks_dialog = bookmarks_dialog_new();
	bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(bookmarks_dialog), 0);
	adw_dialog_present(bookmarks_dialog, GTK_WIDGET(win));
}

void
clear_history(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	adw_dialog_present(clear_history_dialog_new(), GTK_WIDGET(win));
}

void
back(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabView* tab_view = main_window_get_webview_tab_view(win);
    orion_webview_tab_view_go_back(tab_view);
}

void
forward(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabView* tab_view = main_window_get_webview_tab_view(win);
    orion_webview_tab_view_go_forward(tab_view);
}

void
go_back_forward_relative_index(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabView* tab_view = main_window_get_webview_tab_view(win);
    orion_webview_tab_view_go_back_forward_relative_index(tab_view, g_variant_get_int64(parameter));
}

// Opens the homepage in the current tab.
void
home(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    WeakRefWrapper win_weak(win);

    global_core_api->get_setting(
        profile_id,
        settings::general::homepage,
        [win_weak](GetSettingReply<std::string> reply){
            VERIFY_LOG(reply.err);
            std::string homepage_uri = reply.value;
            if (homepage_uri.empty()) {
                homepage_uri = "orion://newtab/"; // fall back to Start Page
            }

            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            OrionWebviewTabView* tab_view = main_window_get_webview_tab_view(win);
            AdwTabView* adw_tab_view = orion_webview_tab_view_get_actual_view(tab_view);
            AdwTabPage* page = adw_tab_view_get_selected_page(adw_tab_view);
            if (!page) {
                log_warning("home: no selected tab page");
                return;
            }
            OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
            OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
            if(content)
            {
                Uri uri = Uri::parse(homepage_uri.c_str());
                orion_webview_tab_content_load_uri(content, uri);
            }
        }
    );
}

void
reopen_last_closed_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    WeakRefWrapper win_weak(win);
    GetRecentlyClosedTabIdsQuery query{
        .profile_id = profile_id,
        .window_id = std::nullopt, // do not filter by window
    };

    global_core_api->get_recently_closed_tab_ids(query, [win_weak](GetRecentlyClosedTabIdsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.ids.empty()) {
            log_info("reopen_last_closed_tab: no recently closed tabs yet");
            return;
        }

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        RecentlyClosedTabId tab_id = reply.ids.front();
        open_recently_closed_tab_internal(win, tab_id);
    });
}

void
reopen_last_closed_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    WeakRefWrapper win_weak(win);

    global_core_api->get_recently_closed_window_ids(profile_id, [win_weak](GetRecentlyClosedWindowIdsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.ids.empty()) {
            log_info("reopen_last_closed_window: no recently closed windows yet");
            return;
        }

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        RecentlyClosedWindowId window_id = reply.ids.front();
        open_recently_closed_window_internal(win, window_id);
    });
}

//-------------------------------------------------------------------------
// Main Menu: Bookmarks
//-------------------------------------------------------------------------

void
show_bookmarks(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	AdwDialog* bookmarks_dialog = bookmarks_dialog_new();
	bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(bookmarks_dialog), 1);
	adw_dialog_present(bookmarks_dialog, GTK_WIDGET(win));
}

void
show_reading_list(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    AdwDialog* dialog = bookmarks_dialog_new();
    bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(dialog), 2);
    adw_dialog_present(dialog, GTK_WIDGET(win));
}

void
add_bookmarks_for_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    WindowId window_id = main_window_get_window_id(win);
    GetTabIdsQuery query{};
    query.window_id = window_id;
    orion::gtk::WeakRefWrapper win_weak(win);
    global_core_api->get_tab_ids(query, [win_weak](GetTabIdsReply reply){
        VERIFY_LOG(reply.err);

        auto win_ptr = win_weak.lock();
        if (!win_ptr) return;
        MainWindow* win = MAIN_WINDOW(win_ptr.get());

        // properly init add bookmarks dialog
        AddBookmarksForTabsDialog* dialog = ADD_BOOKMARKS_FOR_TABS_DIALOG(add_bookmarks_for_tabs_dialog_new());
        add_bookmarks_for_tabs_dialog_set_source_tab_ids(dialog, reply.ids);
        adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(win));
    });
}

void
add_to_reading_list(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    
    Uri current_uri = orion_webview_tab_view_get_uri(view);
    if(current_uri.uri().length()>0)
    {
        orion::CreateReadingListEntryQuery query{};
        GDateTime* now = g_date_time_new_now_utc();
        query.added_on = orion::UnixTimeUtc64(g_date_time_to_unix(now));
        g_date_time_unref(now);
        std::string uri = orion_webview_tab_view_get_uri(view).uri();
        query.address = uri;
        //TODO: icon
        //query.icon_name
        query.profile_id = main_window_get_current_profile_id(win);
        //TODO: not sure what this even is
        //query.referrer
        const char* title = orion_webview_tab_view_get_title(view);
        if(title != NULL)
        {
            if(strlen(title)>0)
            {
                query.title = orion_webview_tab_view_get_title(view);
            }
        }
        
        global_core_api->create_reading_list_entry(query, [uri](orion::CreateReadingListEntryReply reply){
            VERIFY_LOG(reply.err);
            orion::ReadingListEntryId* id = new orion::ReadingListEntryId(reply.id);
            orion::gtk::execute_with_uri_description<orion::ReadingListEntryId>([](const char* desc, orion::ReadingListEntryId* id){
                if(desc)
                {
                    if(g_utf8_validate(desc, -1, NULL))
                    {
                        const int max_utf8_str_len = 80;
                        //NOTE: max len chosen to not fill database with unnecessarily long data,
                        //refer to g_utf8_strncpy for documentation about the size of the allocated char*
                        char* shrunk_desc = g_new(char, max_utf8_str_len*4+1);
                        g_utf8_strncpy(shrunk_desc, desc, max_utf8_str_len);

                        orion::UpdateReadingListEntryQuery query{};
                        query.id = *id;
                        query.description = shrunk_desc;
                        query.update_description = true;
                        global_core_api->update_reading_list_entry(query, [](orion::Error err){VERIFY_LOG(err);});

                        g_free(shrunk_desc);
                    }
                }
                delete id;
            }, uri.c_str(), id);
        });
    }
}

//-------------------------------------------------------------------------
// Main Menu: Tools
//-------------------------------------------------------------------------

void
passwords(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    AdwDialog* dialog = bookmarks_dialog_new();
	adw_dialog_present(dialog, GTK_WIDGET(win));
	bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(dialog), 3);
}

void
take_screenshot(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	auto file_dialog = gtk_file_dialog_new();
	gtk_file_dialog_save(file_dialog, GTK_WINDOW(win), NULL, NULL, NULL); // TODO: fill callback
	g_object_unref(file_dialog);
}

void
take_screenshot_entire_page(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	auto file_dialog = gtk_file_dialog_new();
	gtk_file_dialog_save(file_dialog, GTK_WINDOW(win), NULL, NULL, NULL); // TODO: fill callback
	g_object_unref(file_dialog);
}

void
edit_text_on_page(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::TabId tab_id = main_window_get_current_tab_id(win);

    if (tab_id == orion::TabId{0}) {
        orion::log_warning("edit_text_on_page: no current tab");
        return;
    }

    OrionWebviewTabContent* content = main_window_get_orion_webview_tab_content_by_tab_id(win, tab_id);

    if (!content) {
        orion::log_warning("edit_text_on_page: no content for current tab");
        return;
    }

    bool old_value = orion_webview_tab_content_get_edit_text_on_page(content);
    orion_webview_tab_content_set_edit_text_on_page(content, !old_value);
    g_simple_action_set_state(self, g_variant_new_boolean(!old_value));
}

// Test page: https://www.html-code-generator.com/javascript/disable-copy-paste-cut-input
void
allow_copy_and_paste(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::TabId tab_id = main_window_get_current_tab_id(win);

    if (tab_id == orion::TabId{0}) {
        orion::log_warning("allow_copy_and_paste: no current tab");
        return;
    }

    OrionWebviewTabContent* content = main_window_get_orion_webview_tab_content_by_tab_id(win, tab_id);

    if (!content) {
        orion::log_warning("allow_copy_and_paste: no content for current tab");
        return;
    }

    orion_webview_tab_content_force_allow_copy_and_paste(content);
}

static void
on_install_extension_file_selected(GObject* source, GAsyncResult* result, gpointer user_data)
{
    std::unique_ptr<orion::gtk::WeakRefWrapper> win_weak(
        static_cast<orion::gtk::WeakRefWrapper*>(user_data)
    );
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), result, &error);
    if(!file) {
        if(error && error->code != GTK_DIALOG_ERROR_DISMISSED) {
            orion::log_error("Failed to select extension package: %s", error->message);
        }
        if(error) {
            g_error_free(error);
        }
        return;
    }
    defer([file]{ g_object_unref(file); });

    auto win_ref = win_weak->lock();
    if(!win_ref) {
        return;
    }

    gchar* file_path = g_file_get_path(file);
    if(!file_path) {
        orion::log_error("Selected extension package is not a local file");
        return;
    }
    defer([file_path]{ g_free(file_path); });

    std::filesystem::path path(file_path);
    std::string extension = path.extension().string();
    orion::str::to_lower(&extension);

    orion::ExtensionType type = orion::ExtensionType::not_set;
    if(extension == ".xpi") {
        type = orion::ExtensionType::firefox;
    } else if(extension == ".crx" || extension == ".zip") {
        type = orion::ExtensionType::chrome;
    }
    if(type == orion::ExtensionType::not_set) {
        orion::log_error("Unsupported extension package: %s", file_path);
        return;
    }

    MainWindow* win = MAIN_WINDOW(win_ref.get());
    orion::InstallExtensionQuery query{};
    query.profile_id = main_window_get_current_profile_id(win);
    query.sku = orion::ExtensionSku(orion::generate_uuid_v4("file:" + path.string()));
    query.type = type;
    query.file_path = path.string();
    query.name = path.stem().string();

    global_core_api->install_extension(std::move(query), [path](orion::InstallExtensionReply reply) {
        if(reply.err) {
            orion::log_error("Failed to install extension from %s", path.string().c_str());
            VERIFY_LOG(reply.err);
            return;
        }
        orion::log_debug(
            "Installed extension {%s} from %s",
            reply.id.value.c_str(),
            path.string().c_str()
        );
    });
}

void
install_from_disk(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	GtkFileDialog* file_dialog = gtk_file_dialog_new();
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, _("Chrome and Firefox extensions"));
    gtk_file_filter_add_suffix(filter, "crx");
    gtk_file_filter_add_suffix(filter, "xpi");
    gtk_file_filter_add_suffix(filter, "zip");
    gtk_file_dialog_set_default_filter(file_dialog, filter);
    g_object_unref(filter);

	gtk_file_dialog_open(
        file_dialog,
        GTK_WINDOW(win),
        nullptr,
        on_install_extension_file_selected,
        new orion::gtk::WeakRefWrapper(win)
    );
	g_object_unref(file_dialog);
}

void
notes(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
enable_compatibility_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    std::string website = main_window_get_current_website(win);
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->toggle_setting(
        profile_id,
        WEBSITE_SETTING(enable_compatibility_mode, website),
        [website,profile_id,win_weak](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            orion::log_debug(
                "enable_compatibility_mode toggled: new_value=%d website=%s",
                reply.new_value, website.c_str()
            );

            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            orion::set_enable_compatibility_mode(win, website, reply.new_value);
        }
    );
}

void
install_site_as_app(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
	adw_dialog_present(install_site_as_app_dialog_new(), GTK_WIDGET(win));
}

void
low_power_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_setting(profile_id, orion::settings::general::low_power_mode,
    [profile_id,win_weak](orion::GetSettingReply<bool> reply){
        VERIFY_LOG(reply.err);

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        orion::log_debug("low_power_mode: current_value=%d", reply.value);

        if (reply.value) {
            // Disable low power mode
            global_core_api->set_setting(profile_id, orion::settings::general::low_power_mode, false,
            [](orion::SetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);
                orion::log_debug("low_power_mode: new_value=%d", reply.new_value);
            });
        } else {
            // Ask the user if they want to enable low power mode
            adw_dialog_present(low_power_mode_dialog_new(), GTK_WIDGET(win));
        }
    });
}

void
manage_extensions(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    AdwDialog* dialog = bookmarks_dialog_new();
	adw_dialog_present(dialog, GTK_WIDGET(win));
	bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(dialog), 4);
}

void
show_fonts(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
open_page_in_internet_archive(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    Uri current_uri = orion_webview_tab_view_get_uri(view);
    if (current_uri.uri().length() == 0) {
        orion::log_warning("open_page_in_internet_archive: current uri is empty, aborting");
        return;
    }

    std::string uri = current_uri.uri();

    // Patterns:
    // - https://web.archive.org/web/<url>
    //     opens the latest record in the archive
    // - https://web.archive.org/web/0/<url>
    //     opens the earliest record in the archive
    std::string latest_archive_uri = "https://web.archive.org/web/" + current_uri.uri();
    
    // Open the archive page in the current tab
    orion_webview_tab_view_load_uri(view, Uri::parse_strict(latest_archive_uri.c_str()));
}

//-------------------------------------------------------------------------
// Main Menu: Develop
//-------------------------------------------------------------------------

void
show_web_inspector(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabView* tab_view = main_window_get_webview_tab_view(win);
    orion_webview_tab_view_show_web_inspector(tab_view);
#endif
}

void
toggle_web_inspector(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabView* tab_view = main_window_get_webview_tab_view(win);
    orion_webview_tab_view_toggle_web_inspector(tab_view);
#endif
}

void
show_design_tools(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
enter_responsive_design_mode(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
open_console(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
open_network(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
open_storage(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
open_resources(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
open_layers(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
open_graphics(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
start_performance_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
edit_cookies(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
empty_cache(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
view_source(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
user_agent(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
}

//-------------------------------------------------------------------------
// Main Menu: Window
//-------------------------------------------------------------------------

void
minimize(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    gtk_window_minimize(GTK_WINDOW(win));
}

void
fill(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    gtk_window_maximize(GTK_WINDOW(win));
}

void
center(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
move_left(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
move_right(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
move_top(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
move_bottom(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
move_left_and_right(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
move_right_and_left(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
move_top_and_bottom(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
move_bottom_and_top(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
move_return_to_previous_size(GSimpleAction* self, GVariant* parameter, gpointer user_data) {}

void
show_previous_tab_accelerator(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_show_previous_tab_accel(win);
}

void
show_next_tab_accelerator(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_show_next_tab_accel(win);
}


void
show_previous_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_switch_to_previous_tab(win);
}

void
show_next_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_switch_to_next_tab(win);
}

enum class AdjacentTabGroupDirection { previous, next };

void
go_to_adjacent_tab_group(MainWindow* win, AdjacentTabGroupDirection direction)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::WindowId window_id = main_window_get_window_id(win);
    orion::gtk::WeakRefWrapper weak_win(win);

    global_core_api->get_window_ids({.profile_id=profile_id},
        [window_id,direction,weak_win](orion::GetWindowIdsReply reply){

        VERIFY_LOG(reply.err);
        auto win_ref = weak_win.lock();
        if(!win_ref) return;
        MainWindow* win = MAIN_WINDOW(win_ref.get());

        if (reply.ids.size() <= 1) return; // No other windows to switch to
        auto it = std::find(reply.ids.begin(), reply.ids.end(), window_id);
        if (it == reply.ids.end()) {
            orion::log_error("Current window ID not found in the list of window IDs");
            return;
        }
        auto current_index = static_cast<size_t>(std::distance(reply.ids.begin(), it));
        if (current_index >= reply.ids.size()) {
            orion::log_error("Current index is out of bounds");
            return;
        }
        size_t target_index;
        if (direction == AdjacentTabGroupDirection::next) {
            target_index = (current_index + 1) % reply.ids.size();
        } else {
            target_index = (current_index == 0) ? reply.ids.size() - 1 : current_index - 1;
        }
        if (target_index == current_index || target_index >= reply.ids.size()) {
            orion::log_warning("Target index is out of bounds or the same as current index");
            return;
        }
        
        main_window_switch_to_window_id_in_place(win, reply.ids.at(target_index));
    });
}

void
go_to_previous_tab_group(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    go_to_adjacent_tab_group(win, AdjacentTabGroupDirection::previous);
}

void
go_to_next_tab_group(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    go_to_adjacent_tab_group(win, AdjacentTabGroupDirection::next);
}

void
pin_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    AdwTabView* view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));
    AdwTabPage* page = adw_tab_view_get_selected_page(view);
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    orion::UpdateTabQuery query{};
    query.id = orion_webview_tab_container_get_id(container);
    query.window_id = main_window_get_window_id(win);
    query.is_pinned = !adw_tab_page_get_pinned(page);
    query.update_is_pinned = true;
    global_core_api->update_tab(query, [](orion::Error err){VERIFY_LOG(err);});
}

void
suspend_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = main_window_get_current_tab_id(win);
    suspend_tabs_internal(win, {tab_id});
}

//-------------------------------------------------------------------------
// Toolbar
//-------------------------------------------------------------------------

void
use_mini_toolbar(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);

    GVariant* state = g_action_get_state(G_ACTION(action));
    defer([state]{ g_variant_unref(state); });

    global_core_api->set_setting(
        profile_id,
        orion::settings::tabs::use_mini_toolbar,
        !g_variant_get_boolean(state),
        [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
}

void
toolbar_item_style(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    const char* parameter_string = g_variant_get_string(parameter, NULL);
    char* end;
    int val = strtol(parameter_string, &end, 10);
    if((size_t)(end-parameter_string)!=strlen(parameter_string))
    {
        orion::log_debug("programming error: toolbar_item_style action got passed a bad string: \"%s\"", parameter_string);
        return;
    }
    orion::settings::ToolbarItemStyle style = orion::settings::ToolbarItemStyle(val);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    global_core_api->set_setting(profile_id, orion::settings::appearance::toolbar_item_style, style,
        [](orion::SetSettingReply<orion::settings::ToolbarItemStyle> reply){
            VERIFY_LOG(reply.err);
        });
}

void
reset_toolbar(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);

    // Reset both Standard and Compact Tab layout
    orion::ResetToolbarItemsQuery query{
        .profile_id = profile_id,
        .is_compact_tab_layout = false,
        .in_toolbar = true,
        .in_overflow_menu = true, // reset overflow menu items too
    };

    global_core_api->reset_toolbar_items(query, [profile_id](orion::Error err){
        VERIFY_LOG(err);

        orion::log_debug("reset_toolbar: tab_layout=standard");

        orion::ResetToolbarItemsQuery query{
            .profile_id = profile_id,
            .is_compact_tab_layout = true,
            .in_toolbar = true,
            .in_overflow_menu = true, // reset overflow menu items too
        };

        global_core_api->reset_toolbar_items(query, [](orion::Error err){
            VERIFY_LOG(err);

            orion::log_debug("reset_toolbar: tab_layout=compact");
        });
    });
}

void
move_toolbar_item_to_overflow_menu(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    auto id = static_cast<orion::ToolbarItemId>(g_variant_get_int64(parameter));
    orion::UpdateToolbarItemQuery query{
        .id = id,
        .in_overflow_menu = true,
        .update_in_overflow_menu = true,
    };
    global_core_api->update_toolbar_item(query, [](orion::Error err){
        VERIFY_LOG(err);
    });
}

void
remove_toolbar_item(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    auto id = static_cast<orion::ToolbarItemId>(g_variant_get_int64(parameter));
    global_core_api->delete_toolbar_items({id}, [id](orion::Error err){
        VERIFY_LOG(err);
        orion::log_debug("remove_toolbar_item: id=%lld", id.value);
    });
}

//-------------------------------------------------------------------------
// More Tools menu
//-------------------------------------------------------------------------

namespace {
    struct ImportButtonContextData {
        orion::ProgrammableButtonId id{};
        orion::ProfileId profile_id{};
        // char* buffer;
        // gssize buffer_size;
    };
};

static void
load_contents_cb(GObject* source, GAsyncResult* res, gpointer data)
{
    ImportButtonContextData* context_data = static_cast<ImportButtonContextData*>(data);
    defer([context_data]{ delete context_data; });
    
    char* contents = NULL;
    gsize length = 0;
    GError* err = NULL;
    if(!g_file_load_contents_finish(G_FILE(source), res, &contents, &length, NULL, &err))
    {
        orion::log_error("%s", err->message);
        g_error_free(err);
        return;
    }
    defer([contents]{ g_free(contents); });

    auto result = rfl::json::read<orion::ProgrammableButtonDto>(contents);
    if(!result.has_value()) {
        return;
    }
    
    if (context_data->id.value == 0) {
        // Create new button
        orion::ProgrammableButtonDto& dto = result.value();

        orion::CreateProgrammableButtonQuery query{
            .id = context_data->id,
            .profile_id = context_data->profile_id,
            .title = dto.title,
            .icon_string = dto.icon_string,
            .action_type = dto.action_type,
            .action_string = dto.action_string,
            .keyboard_shortcut = dto.keyboard_shortcut,
            // Add to the end of the toolbar, this can be improved in the future mimicking Mac behavior
            .next_item_id = ToolbarItemId{0},
            .in_overflow_menu = false,
        };

        global_core_api->create_programmable_button(query, [](orion::Error err){ VERIFY_LOG(err); });
    } else {
        // Update existing button
        orion::ProgrammableButtonDto& dto = result.value();

        orion::UpdateProgrammableButtonQuery query{};
        query.id = context_data->id;

        query.update_title = true;
        query.title = dto.title;
        query.update_icon_string = true;
        query.icon_string = dto.icon_string;
        query.update_action_type = true;
        query.action_type = dto.action_type;
        query.update_action_string = true;
        query.action_string = dto.action_string;
        query.update_keyboard_shortcut = true;
        query.keyboard_shortcut = dto.keyboard_shortcut;

        global_core_api->update_programmable_button(query, [](orion::Error err){ VERIFY_LOG(err); });
    }
}

static void
on_import_button_from_url_dialog_finished(
    ImportFromUrlDialog* dialog, const gchar* url, ImportButtonContextData* context_data)
{
    if(url)
    {
        GFile* file = g_file_new_for_uri(url);
        // Create a copy to avoid lifetime issues
        ImportButtonContextData* new_context_data = new ImportButtonContextData{
            .id = context_data->id,
            .profile_id = context_data->profile_id,
        };
        g_file_load_contents_async(file, NULL, load_contents_cb, new_context_data);
    }
}

void
import_button_from_url_impl(MainWindow* win, orion::ProgrammableButtonId button_id)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    AdwDialog* dialog = import_from_url_dialog_new();
    ImportButtonContextData* context_data = new ImportButtonContextData{
        .id = button_id,
        .profile_id = profile_id
    };
    g_signal_connect_data(dialog, "finished", G_CALLBACK(on_import_button_from_url_dialog_finished), context_data,
        [](gpointer data, GClosure* closure){
            delete static_cast<ImportButtonContextData*>(data);
        }, G_CONNECT_DEFAULT);

    adw_dialog_present(dialog, GTK_WIDGET(win));
}

void
import_button_from_url(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    // Popdown ProgrammableButtonPopover (if any)
    main_window_popdown_all_popovers(MAIN_WINDOW(user_data));
    import_button_from_url_impl(MAIN_WINDOW(user_data), orion::ProgrammableButtonId(g_variant_get_int64(parameter)));
}

void
import_button_from_url_create(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    import_button_from_url_impl(MAIN_WINDOW(user_data), orion::ProgrammableButtonId(0)); // empty ID for new button
}

static void
open_cb(GObject* source, GAsyncResult* res, gpointer data)
{
    ImportButtonContextData* context_data = static_cast<ImportButtonContextData*>(data);

    GError* err = NULL;
    GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), res, &err);
    if(!file)
    {
        orion::log_error("%s", err->message);
        g_error_free(err);
        delete context_data;
        return;
    }
    defer([file]{ g_object_unref(file); });

    g_file_load_contents_async(file, NULL, load_contents_cb, context_data);
}

static void
import_button_from_file_impl(MainWindow* win, orion::ProgrammableButtonId button_id)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    GtkFileDialog* dialog = gtk_file_dialog_new();

    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_add_suffix(filter, "json");
    gtk_file_dialog_set_default_filter(dialog, filter);
    g_object_unref(filter);

    ImportButtonContextData* data = new ImportButtonContextData({
        .id = button_id,
        .profile_id = profile_id,
    });

    gtk_file_dialog_open(dialog, GTK_WINDOW(win), NULL, open_cb, data);
}

void
import_button_from_file(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    // Popdown ProgrammableButtonPopover (if any)
    main_window_popdown_all_popovers(MAIN_WINDOW(user_data));
    import_button_from_file_impl(MAIN_WINDOW(user_data), orion::ProgrammableButtonId(g_variant_get_int64(parameter)));
}

void
import_button_from_file_create(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    import_button_from_file_impl(MAIN_WINDOW(user_data), orion::ProgrammableButtonId(0)); // empty ID for new button
}

static void
write_cb(GObject* source, GAsyncResult* res, gpointer data)
{
    std::string* string = static_cast<std::string*>(data);
    defer([string]{ delete string; });

    GError* err = NULL;
    if(!g_file_replace_contents_finish(G_FILE(source), res, NULL, &err))
    {
        orion::log_error("%s", err->message);
        g_error_free(err);
    }
}

static void
export_button_to_file_dialog_finished(GObject* source, GAsyncResult* res, gpointer data)
{
    std::string* string = static_cast<std::string*>(data);

    GError* err = NULL;
    GFile* file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), res, &err);
    if(err)
    {
        orion::log_error("%s", err->message);
        g_error_free(err);
        delete string;
        return;
    }
    defer([file]{ g_object_unref(file); });

    g_file_replace_contents_async(file, string->c_str(), string->length(), NULL, FALSE,
        G_FILE_CREATE_REPLACE_DESTINATION, NULL, write_cb, string);
}

void
export_button_to_file(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    // Popdown ProgrammableButtonPopover (if any)
    main_window_popdown_all_popovers(MAIN_WINDOW(user_data));
    orion::gtk::WeakRefWrapper win_weak(user_data);

    orion::GetProgrammableButtonsQuery query;
    query.ids = {orion::ProgrammableButtonId(g_variant_get_int64(parameter))};

    global_core_api->get_programmable_buttons(query, [win_weak](orion::GetProgrammableButtonsReply reply){
        VERIFY_LOG(reply.err);
        
        if(reply.items.empty()) return;
        std::string* string = new std::string(rfl::json::write(reply.items.at(0)));

        GtkFileDialog* dialog = gtk_file_dialog_new();
        std::string title = "button"; //TODO: quite likely add this field to it all

        GFile* file = g_file_new_build_filename(g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD), (title+".json").c_str(), NULL);
        gtk_file_dialog_set_initial_file(dialog, file);
        g_object_unref(file);
        auto win_strong = win_weak.lock();
        gtk_file_dialog_save(dialog, GTK_WINDOW(win_strong.get()), NULL, export_button_to_file_dialog_finished, string);
        g_object_unref(dialog);
    });
}

void
customize_menu(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_setting(profile_id, orion::settings::tabs::tab_layout,
    [win_weak](orion::GetSettingReply<orion::settings::TabLayout> reply){
        VERIFY_LOG(reply.err);
        bool is_compact_tab_layout = (reply.value == orion::settings::TabLayout::compact);
        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());
        adw_dialog_present(customize_toolbar_dialog_new(is_compact_tab_layout), GTK_WIDGET(win));
    });
}

void
edit_programmable_button(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_on_edit_programmable_button(win, orion::ProgrammableButtonId(g_variant_get_int64(parameter)));
}

void
summarize_page(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    adw_dialog_present(summary_unavailable_dialog_new(), GTK_WIDGET(win));
}

//-------------------------------------------------------------------------
// Tabs
//-------------------------------------------------------------------------

static void
bookmark_tab_internal(MainWindow* win, TabId tab_id, TabType type)
{
    WeakRefWrapper win_weak(win);

    global_core_api->get_tabs({tab_id}, [win_weak,type](GetTabsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.tabs.empty()) return;
        TabDto tab = reply.tabs.at(0);

        global_core_api->get_descendant_tab_ids(tab.id, [win_weak,tab,type](GetDescendantTabIdsReply reply){
            VERIFY_LOG(reply.err);

            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            if (type == TabType::normal) {
                adw_dialog_present(add_bookmark_dialog_new_init_from_tab_dto(tab), GTK_WIDGET(win));
            } else if (type == TabType::tree) {
                std::vector<TabId> tab_ids_to_bookmark;
                tab_ids_to_bookmark.push_back(tab.id);
                for (TabId descendant_id : reply.descendant_tab_ids) {
                    tab_ids_to_bookmark.push_back(descendant_id);
                }

                std::string folder_name = _("Saved Tabs");

                AddBookmarksForTabsDialog* dialog = ADD_BOOKMARKS_FOR_TABS_DIALOG(add_bookmarks_for_tabs_dialog_new());
                add_bookmarks_for_tabs_dialog_set_source_tab_ids(dialog, tab_ids_to_bookmark);
                add_bookmarks_for_tabs_dialog_set_target_folder_name(dialog, folder_name.c_str());
                adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(win));
            } else {
                log_error("bookmark_tab_internal: unknown TabType %lld", static_cast<int64_t>(type));
            }
        });
    });
}

void
bookmark_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId id{g_variant_get_int64(parameter)};
    bookmark_tab_internal(win, id, TabType::normal);
}

void
bookmark_tab_tree(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId id{g_variant_get_int64(parameter)};
    bookmark_tab_internal(win, id, TabType::tree);
}

void
duplicate_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::TabId id = orion::TabId(g_variant_get_int64(parameter));
    orion::DuplicateTabQuery query{};
    query.id = id;
    global_core_api->duplicate_tab(query, [](orion::DuplicateTabReply reply){VERIFY_LOG(reply.err);});
}

static void
move_tab_to_window_internal(MainWindow* win, TabId tab_id, WindowId target_window_id, TabType type)
{
    global_core_api->get_tabs({tab_id}, [tab_id, target_window_id, type](GetTabsReply reply){
        VERIFY_LOG(reply.err);

        if(!reply.tabs.size())
        {
            return;
        }
        WindowId source_window_id = reply.tabs.at(0).window_id;

        MoveTabQuery query{};
        query.tab_id = tab_id;
        query.parent_tab_id = TabId(0);
        query.after_tab_id = TabId(0);
        query.target_window_id = target_window_id;
        query.is_nested_tabs_operation = type == TabType::tree;
        global_core_api->move_tab(query, [source_window_id](Error err){
            VERIFY_LOG(err);

            global_core_api->get_windows({source_window_id}, [](GetWindowsReply reply){
                VERIFY_LOG(reply.err);
                if (reply.windows.empty()) return;
                const WindowDto& source_window = reply.windows.at(0);
                if (source_window.num_tabs > 0) return;
                if (source_window.is_tab_group) {
                    // the source window is unsaved tab group and this was its last tab, close the tab group
                    std::vector<UiWindowId> ui_window_ids = global_core_api->get_ui_windows_by_window_id(source_window.id);
                    for (UiWindowId ui_window_id : ui_window_ids) {
                        MainWindow* main_win = MAIN_WINDOW(ui_window_id.value);
                        main_window_close(main_win);
                    }
                } else {
                    // the source window is saved window, create Start Page tab in it
                    CreateTabQuery query{
                        .window_id = source_window.id,
                        .uri = "orion://newtab/",
                    };
                    global_core_api->create_tab(query, [](CreateTabReply reply){
                        VERIFY_LOG(reply.err);
                    });
                }
            });
        });
    });
}

void
move_tab_to_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id{};
    WindowId target_window_id{};
    g_variant_get(parameter, "(xx)", &tab_id.value, &target_window_id.value);

    move_tab_to_window_internal(win, tab_id, target_window_id, TabType::normal);
}

void
move_tab_tree_to_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id{};
    WindowId target_window_id{};
    g_variant_get(parameter, "(xx)", &tab_id.value, &target_window_id.value);

    move_tab_to_window_internal(win, tab_id, target_window_id, TabType::tree);
}

static void
move_tab_to_new_window_internal(TabId tab_id, ProfileId profile_id, TabType tab_type)
{
    global_core_api->get_tabs({tab_id}, [tab_id,profile_id,tab_type](GetTabsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.tabs.empty()) {
            log_warning("move_tab_to_new_window_internal: tab %lld not found",
                tab_id.value
            );
            return;
        }
        const orion::TabDto& tab = reply.tabs.at(0);
        if (tab.is_pinned) {
            log_warning("move_tab_to_new_window_internal: tab %lld is pinned, cannot move to new window",
                tab_id.value
            );
            return;
        }

        global_core_api->get_windows({tab.window_id}, [tab,profile_id,tab_id,tab_type](GetWindowsReply reply){
            VERIFY_LOG(reply.err);
            if (reply.windows.empty()) {
                log_warning("move_tab_to_new_window_internal: source window %lld not found",
                    tab.window_id.value
                );
                return;
            }
            const orion::WindowDto& source_window = reply.windows.at(0);

            // The newly created window must be private if the tab is being moved from a private window
            CreateWindowQuery query{
                .profile_id = profile_id,
                .icon_name = "window-flat-symbolic",
                .is_tab_group = true,
                .is_private = source_window.is_private,
            };

            orion::PrivateMode private_mode = source_window.is_private
                ? orion::PrivateMode::yes
                : orion::PrivateMode::no;

            global_core_api->create_window(query, [tab_id,profile_id,tab_type,private_mode](CreateWindowReply reply){

                VERIFY_LOG(reply.err);

                WindowId new_window_id = reply.id;

                MoveTabQuery query{};
                query.tab_id = tab_id;
                query.after_tab_id = TabId(0);
                query.parent_tab_id = TabId(0);
                query.target_window_id = new_window_id;
                query.is_nested_tabs_operation = tab_type == TabType::tree;
                global_core_api->move_tab(query, [new_window_id,profile_id,private_mode](Error err){
                    VERIFY_LOG(err);

                    MainWindow* win = main_window_new(
                        g_application_get_default(), profile_id, new_window_id, private_mode);
                    gtk_window_present(GTK_WINDOW(win));
                });
            });
        });
    });
}

void
move_tab_to_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = static_cast<TabId>(g_variant_get_int64(parameter));
    ProfileId profile_id = main_window_get_current_profile_id(win);

    move_tab_to_new_window_internal(tab_id, profile_id, TabType::normal);
}

void
move_tab_tree_to_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = static_cast<TabId>(g_variant_get_int64(parameter));
    ProfileId profile_id = main_window_get_current_profile_id(win);

    move_tab_to_new_window_internal(tab_id, profile_id, TabType::tree);
}

static void
move_tab_to_new_in_place_window_internal(
    TabId tab_id, ProfileId profile_id, MainWindow* win, TabType tab_type)
{
    WeakRefWrapper win_weak(win);
    
    global_core_api->get_tabs({tab_id}, [tab_id,profile_id,win_weak,tab_type](GetTabsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.tabs.empty()) {
            log_warning("move_tab_to_new_in_place_window_internal: tab %lld not found",
                tab_id.value
            );
            return;
        }
        const orion::TabDto& tab = reply.tabs.at(0);
        if (tab.is_pinned) {
            log_warning("move_tab_to_new_in_place_window_internal: tab %lld is pinned, cannot move to new window", 
                tab_id.value
            );
            return;
        }

        global_core_api->get_windows({tab.window_id}, [tab,profile_id,win_weak,tab_id,tab_type](GetWindowsReply reply){
            VERIFY_LOG(reply.err);
            if (reply.windows.empty()) {
                log_warning("move_tab_to_new_in_place_window_internal: source window %lld not found",
                    tab.window_id.value
                );
                return;
            }
            const orion::WindowDto& source_window = reply.windows.at(0);

            profile::find_unique_window_name(
                profile_id,
                orion::PrivateMode::no,
                [profile_id,tab_id,win_weak,tab_type,source_window](std::string unique_name){

                // The newly created window must be private if the tab is being moved from a private window
                CreateWindowQuery query{
                    .profile_id = profile_id,
                    .name = unique_name,
                    .icon_name = "window-flat-symbolic",
                    .is_tab_group = false, // saved tab group
                    .is_private = source_window.is_private,
                };

                global_core_api->create_window(query, [tab_id,profile_id,win_weak,tab_type](CreateWindowReply reply){
                    VERIFY_LOG(reply.err);

                    WindowId new_window_id = reply.id;

                    MoveTabQuery query{};
                    query.tab_id = tab_id;
                    query.after_tab_id = TabId(0);
                    query.parent_tab_id = TabId(0);
                    query.target_window_id = new_window_id;
                    query.is_nested_tabs_operation = tab_type == TabType::tree;
                    global_core_api->move_tab(query, [new_window_id, win_weak](Error err){
                        VERIFY_LOG(err);

                        auto win_strong = win_weak.lock();
                        if (!win_strong) return;
                        MainWindow* win = MAIN_WINDOW(win_strong.get());
                        
                        // switch to the new tab group in the same UI window
                        main_window_switch_to_window_id_in_place(win, new_window_id);
                    });
                });
            });
        });
    });
}

// Creates a new Saved Tab Group (saved window) and moves the tab to it.
void
move_tab_to_new_in_place_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = static_cast<TabId>(g_variant_get_int64(parameter));
    ProfileId profile_id = main_window_get_current_profile_id(win);

    move_tab_to_new_in_place_window_internal(tab_id, profile_id, win, TabType::normal);
}

void
move_tab_tree_to_new_in_place_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = static_cast<TabId>(g_variant_get_int64(parameter));
    ProfileId profile_id = main_window_get_current_profile_id(win);

    move_tab_to_new_in_place_window_internal(tab_id, profile_id, win, TabType::tree);
}

static void
move_tab_to_window_in_another_profile(
    TabId tab_id, WindowId target_window_id, ProfileId target_profile_id, TabType type)
{
    MoveTabQuery query{};
    query.tab_id = tab_id;
    query.target_window_id = target_window_id;
    query.is_nested_tabs_operation = type == TabType::tree;
    query.after_tab_id = TabId(0);
    query.parent_tab_id = TabId(0);

    global_core_api->move_tab(query, [target_window_id, tab_id](Error err){
        VERIFY_LOG(err);

        // Call to orion_webview_tab_container_set_last_active_time should not be needed here
        UpdateWindowQuery query{
            .id = target_window_id,
            .selected_tab_id = tab_id,
            .update_selected_tab_id = true,
        };

        global_core_api->update_window(query, [target_window_id](Error err){
            VERIFY_LOG(err);
            
            open_window_in_separate_window_or_switch_to_existing_internal(target_window_id);
        });
    });
}

static void
move_tab_to_profile_internal(TabId tab_id, ProfileId target_profile_id, TabType type)
{
    global_core_api->get_profiles({target_profile_id},
        [tab_id,target_profile_id,type](GetProfilesReply reply){
            VERIFY_LOG(reply.err);
            if (reply.profiles.empty()) {
                log_warning("move_tab_to_profile: target profile %lld not found", target_profile_id.value);
                return;
            }
            const orion::ProfileDto& target_profile = reply.profiles.at(0);
            
            global_core_api->get_window_ids({.profile_id=target_profile_id},
                [tab_id,target_profile,type](GetWindowIdsReply reply){
                    VERIFY_LOG(reply.err);

                    // The tab should always be moved to a normal non-private window even if its source window
                    // is private

                    if (reply.ids.empty()) {
                        // No windows in the target profile yet
                        CreateWindowQuery query{
                            .profile_id = target_profile.id,
                            .icon_name = "window-flat-symbolic",
                            .is_tab_group = true,
                        };

                        global_core_api->create_window(query,
                            [tab_id,target_profile,type](CreateWindowReply reply){
                                VERIFY_LOG(reply.err);
                                WindowId target_window_id = reply.id;
                                move_tab_to_window_in_another_profile(tab_id, target_window_id, target_profile.id, type);
                            });
                        return;
                    }

                    WindowId target_window_id = target_profile.selected_window_id;

                    // fall back to the first window if the selected window is not available
                    if (target_window_id.value == 0 || std::find(reply.ids.begin(), reply.ids.end(), target_window_id) == reply.ids.end()) {
                        target_window_id = reply.ids.at(0);
                    }

                    move_tab_to_window_in_another_profile(tab_id, target_window_id, target_profile.id, type);
                });
        });
}

void
move_tab_to_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id{};
    ProfileId target_profile_id{};
    g_variant_get(parameter, "(xx)", &tab_id.value, &target_profile_id.value);

    move_tab_to_profile_internal(tab_id, target_profile_id, TabType::normal);
}

void
move_tab_tree_to_profile(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id{};
    ProfileId target_profile_id{};
    g_variant_get(parameter, "(xx)", &tab_id.value, &target_profile_id.value);

    move_tab_to_profile_internal(tab_id, target_profile_id, TabType::tree);
}

void
pin_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::TabId id = orion::TabId(g_variant_get_int64(parameter));
    orion::WindowId window_id = main_window_get_window_id(win);

    orion::UpdateTabQuery query{};
    query.id = id;
    global_core_api->get_tabs({id}, [window_id](orion::GetTabsReply reply){
        VERIFY_LOG(reply.err);
        if(reply.tabs.size()<1) return;

        orion::TabDto& tab = reply.tabs.at(0);
        orion::UpdateTabQuery query{};
        query.id = tab.id;
        query.window_id = window_id;
        query.is_pinned = !tab.is_pinned;
        query.update_is_pinned = true;
        
        global_core_api->update_tab(query, [](orion::Error err){VERIFY_LOG(err);});
    });
}

void
reload_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::TabId id = orion::TabId(g_variant_get_int64(parameter));
    main_window_reload_tab_by_tab_id(win, id);
}

void
duplicate_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    AdwTabView* view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));
    AdwTabPage* page = adw_tab_view_get_selected_page(view);
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    orion::TabId tab_id = orion_webview_tab_container_get_id(container);

    orion::DuplicateTabQuery query{};
    query.id = tab_id;
    global_core_api->duplicate_tab(query, [](orion::DuplicateTabReply reply){VERIFY_LOG(reply.err);});
}

void
suspend_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = TabId(g_variant_get_int64(parameter));
    suspend_tabs_internal(win, {tab_id});
}

static void
suspend_tab_tree_internal(TabId tab_id, MainWindow* win)
{
    WeakRefWrapper win_weak(win);

    global_core_api->get_descendant_tab_ids(tab_id, [tab_id,win_weak](GetDescendantTabIdsReply reply){
        VERIFY_LOG(reply.err);

        std::vector<TabId> tab_ids_to_suspend;
        tab_ids_to_suspend.push_back(tab_id);
        for (TabId descendant_id : reply.descendant_tab_ids) {
            tab_ids_to_suspend.push_back(descendant_id);
        }

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());
        
        suspend_tabs_internal(win, tab_ids_to_suspend);
    });
}

void
suspend_tab_tree(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = TabId(g_variant_get_int64(parameter));
    suspend_tab_tree_internal(tab_id, win);
}

void
suspend_current_tab_tree(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = main_window_get_current_tab_id(win);
    suspend_tab_tree_internal(tab_id, win);
}

void
suspend_other_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::TabId tab_id = orion::TabId(g_variant_get_int64(parameter));
    orion::GetOtherTabIdsQuery query{
        .exclude_tab_id = tab_id,
    };
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_other_tab_ids(query, [win_weak](orion::GetOtherTabIdsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.other_tab_ids.empty()) return;

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        suspend_tabs_internal(win, reply.other_tab_ids);
    });
}

// In Low Power Mode, tabs that are inactive for this long will automatically be suspended
const int64_t SUSPEND_THRESHOLD_SECONDS = 5 * 60; // 5 minutes

void
suspend_non_active_non_pinned_tabs(MainWindow* win, bool only_inactive_for_some_time)
{
    // main_window_get_current_tab_id() may not return a valid tab id here, call core API
    orion::WindowId win_id = main_window_get_window_id(win);
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_windows({win_id}, [win_id,win_weak,only_inactive_for_some_time](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.windows.empty()) {
            orion::log_warning("suspend_non_active_non_pinned_tabs: window %lld not found", win_id.value);
            return;
        }
        const orion::WindowDto& window = reply.windows.at(0);
        if (window.selected_tab_id.value == 0) {
            orion::log_verbose("suspend_non_active_non_pinned_tabs: window %lld has no selected tab", win_id.value);
            return;
        }

        orion::TabId active_tab_id = window.selected_tab_id;
        orion::GetOtherTabIdsQuery query{
            .exclude_tab_id = active_tab_id,
            .exclude_pinned_tabs = true,
            .exclude_suspended_tabs = true,
        };

        global_core_api->get_other_tab_ids(query, [win_weak,only_inactive_for_some_time](orion::GetOtherTabIdsReply reply){
            VERIFY_LOG(reply.err);
            if (reply.other_tab_ids.empty()) return;

            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            std::vector<orion::TabId> suspend_ids;

            for (const orion::TabId& tab_id : reply.other_tab_ids) {
                if (only_inactive_for_some_time) {
                    OrionWebviewTabContainer* container = main_window_get_orion_webview_tab_container_by_tab_id(win, tab_id);
                    if (!container) continue;
                    orion::UnixTimeUtc64 last_active_time = orion_webview_tab_container_get_last_active_time(container);
                    int64_t age_seconds = orion::tm::duration_seconds(orion::tm::unix_time_utc64_now(), last_active_time);
                    if (age_seconds > SUSPEND_THRESHOLD_SECONDS) {
                        suspend_ids.push_back(tab_id);
                    }
                } else {
                    suspend_ids.push_back(tab_id);
                }
            }

            if (!suspend_ids.empty()) {
                orion::log_debug(
                    "suspend_non_active_non_pinned_tabs: suspending %zu tabs, only_inactive_for_some_time=%d",
                    suspend_ids.size(), only_inactive_for_some_time
                );
                suspend_tabs_internal(win, suspend_ids);
            }
        });
    });
}

void
close_tab_tracking_recent(TabId tab_id, ProfileId profile_id, WindowId window_id, std::function<void(Error)> done_cb)
{
    close_tab_tracking_recent_internal(tab_id, profile_id, window_id, TabType::normal, std::move(done_cb));
}

void
close_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId id{g_variant_get_int64(parameter)};
    // Prefer UI close path if the tab exists in this window's tab view so
    // the same last-tab logic runs (new tab or close window). Otherwise
    // fall back to backend deletion.
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    AdwTabPage* page = orion_webview_tab_view_get_page_with_tab_id(view, id);
    if (page) {
        adw_tab_view_close_page(orion_webview_tab_view_get_actual_view(view), page);
        return;
    }
    ProfileId profile_id = main_window_get_current_profile_id(win);
    WindowId window_id = main_window_get_window_id(win);
    close_tab_tracking_recent_internal(id, profile_id, window_id, TabType::normal);
}

void
close_tab_tree(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId id{g_variant_get_int64(parameter)};
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    AdwTabPage* page = orion_webview_tab_view_get_page_with_tab_id(view, id);
    if (page) {
        adw_tab_view_close_page(orion_webview_tab_view_get_actual_view(view), page);
        return;
    }
    ProfileId profile_id = main_window_get_current_profile_id(win);
    WindowId window_id = main_window_get_window_id(win);
    close_tab_tracking_recent_internal(id, profile_id, window_id, TabType::tree);
}

void
close_tabs_below(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId id = TabId(g_variant_get_int64(parameter));
    GetOtherTabIdsQuery query{
        .exclude_tab_id = id,
    };
    global_core_api->get_other_tab_ids(query, [](GetOtherTabIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->delete_tabs(reply.after_tab_ids, [](Error err){VERIFY_LOG(err);});
    });
}

static void
close_tabs_above_internal(TabId tab_id)
{
    GetOtherTabIdsQuery query{
        .exclude_tab_id = tab_id,
    };
    global_core_api->get_other_tab_ids(query, [](GetOtherTabIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->delete_tabs(reply.before_tab_ids, [](Error err){VERIFY_LOG(err);});
    });
}

void
close_tabs_above(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = TabId(g_variant_get_int64(parameter));
    close_tabs_above_internal(tab_id);
}

void
close_tabs_above_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = main_window_get_current_tab_id(win);
    close_tabs_above_internal(tab_id);
}

static void
mute_tab_internal(MainWindow* win, TabId tab_id)
{
    OrionWebviewTabContainer* container = main_window_get_orion_webview_tab_container_by_tab_id(win, tab_id);
    if(!container) return;
    orion_webview_tab_container_set_is_muted(container, !orion_webview_tab_container_get_is_muted(container));
}

static void
mute_other_tabs_internal(MainWindow* win, TabId tab_id)
{
    // if any other tab is audibly playing and unmuted, mute everything that plays;
    // otherwise (everything that plays is already muted) unmute everything that plays instead
    OtherTabsAudioState audio_state = main_window_get_other_tabs_audio_state(win, tab_id);
    gboolean should_mute = audio_state.any_playing_unmuted || !audio_state.any_playing;

    AdwTabView* tab_view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));
    int n_pages = adw_tab_view_get_n_pages(tab_view);
    for(int i = 0; i < n_pages; i++)
    {
        AdwTabPage* page = adw_tab_view_get_nth_page(tab_view, i);
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
        // only touch tabs that are actually producing audio, so silent tabs never
        // pick up a stray muted state (which would otherwise surface their audio icon)
        if(orion_webview_tab_container_get_id(container).value != tab_id.value
            && orion_webview_tab_container_get_is_playing_audio(container))
        {
            orion_webview_tab_container_set_is_muted(container, should_mute);
        }
    }
}

void
mute_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    mute_tab_internal(win, main_window_get_current_tab_id(win));
}

void
mute_other_tabs_than_current(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    mute_other_tabs_internal(win, main_window_get_current_tab_id(win));
}

void
mute_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    mute_tab_internal(win, TabId(g_variant_get_int64(parameter)));
}

void
mute_other_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    mute_other_tabs_internal(win, TabId(g_variant_get_int64(parameter)));
}

void
close_other_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::TabId id = orion::TabId(g_variant_get_int64(parameter));
    orion::GetOtherTabIdsQuery query{};
    query.exclude_tab_id = id;
    global_core_api->get_other_tab_ids(query, [](orion::GetOtherTabIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->delete_tabs(reply.after_tab_ids, [](orion::Error err){VERIFY_LOG(err);});
    });
}

static void
move_tab(TabDto tab, WindowId source_window_id, WindowId target_window_id)
{
    if (source_window_id == target_window_id) return; // no need to move
    orion::UpdateTabQuery query{
        .id = tab.id,
        .window_id = target_window_id,
        .update_window_id = true,
    };
    global_core_api->update_tab(query, [](orion::Error err){
        VERIFY_LOG(err);
    });
}

void
merge_all_windows(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    WindowId current_window_id = main_window_get_window_id(win);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    
    global_core_api->get_window_ids({.profile_id=profile_id}, [current_window_id](GetWindowIdsReply reply){
        VERIFY_LOG(reply.err);
        
        global_core_api->get_windows(reply.ids, [current_window_id](GetWindowsReply reply){
            VERIFY_LOG(reply.err);

            for (const auto& window : reply.windows) {
                if (window.id == current_window_id) continue; // skip current window
                if (!window.is_tab_group) continue; // skip saved windows

                global_core_api->get_tab_ids({window.id}, [window,current_window_id](GetTabIdsReply reply){
                    VERIFY_LOG(reply.err);
                    
                    global_core_api->get_tabs(reply.ids, [window,current_window_id](GetTabsReply reply){
                        VERIFY_LOG(reply.err);
                        for (const auto& tab : reply.tabs) {
                            if (tab.is_pinned) continue; // skip pinned tabs
                            move_tab(tab, tab.window_id, current_window_id);
                        }
                        close_window_internal(window.id);
                    });
                });
            }
        });
    });
}

void
focus_target_window(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    WindowId current_window_id = main_window_get_window_id(win);
    WindowId window_id = static_cast<WindowId>(g_variant_get_int64(parameter));
    if (current_window_id == window_id) return; // nothing to do
    std::vector<UiWindowId> ui_window_ids = global_core_api->get_ui_windows_by_window_id(window_id);
    if (ui_window_ids.empty()) return; // window is not open in any UI window
    UiWindowId ui_window_id = ui_window_ids.at(0);
    MainWindow* target_win = MAIN_WINDOW(ui_window_id.value);

    main_window_focus(target_win);
}

//-------------------------------------------------------------------------
// Main Menu: DEBUG (only in debug builds)
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Sidebar
//-------------------------------------------------------------------------

void
sidebar_type(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    const char* selected = g_variant_get_string(parameter, NULL);
    g_simple_action_set_state(action, g_variant_new_string(selected));

    if(strcmp(selected, "tabs")==0)
    {
        // When switching to Tabs page in Sidebar, also switch to Vertical Tabs layout automatically
        global_core_api->set_setting(
            profile_id,
            orion::settings::tabs::tab_layout,
            orion::settings::TabLayout::vertical,
            [](orion::SetSettingReply<orion::settings::TabLayout> reply){
                VERIFY_LOG(reply.err);
            }
        );
    }
    //TODO: hook this into persistent settings instead
    main_window_switch_to_sidebar_page(win, selected);
}

static void
on_clear_reading_list_confirmation(AdwAlertDialog* self, gchar* response, gpointer user_data)
{
	orion::log_debug("on_clear_reading_list_confirmation: %s", response);
    if(strcmp(response, "clear") == 0)
    {
        orion::GetReadingListEntryIdsQuery query{};
        query.profile_id = main_window_get_current_profile_id(MAIN_WINDOW(user_data));
        query.added_after = UnixTimeUtc64(std::numeric_limits<int64_t>::min());
        query.added_before = orion::UnixTimeUtc64Max;
        global_core_api->get_reading_list_entry_ids(query, [](orion::GetReadingListEntryIdsReply reply){
            VERIFY_LOG(reply.err);
            global_core_api->delete_reading_list_entries(reply.ids, [](orion::Error err){VERIFY_LOG(err);});
        });
    }
}

void
reading_list_clear_all_items(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    AdwDialog* dialog = adw_alert_dialog_new(NULL, NULL);

	adw_alert_dialog_format_body_markup(
		ADW_ALERT_DIALOG(dialog),
		_("<b>Are you sure you want to clear your Reading List?</b>\n\nAll items in your Reading List will be removed.")
    );

	adw_alert_dialog_add_responses(
		ADW_ALERT_DIALOG(dialog),
		"cancel", _("C_ancel"),
		"clear", _("_Clear"),
		NULL
    );

	adw_alert_dialog_set_response_appearance(
		ADW_ALERT_DIALOG(dialog),
		"clear",
		ADW_RESPONSE_DESTRUCTIVE
    );

	adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "clear");
	adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");

	g_signal_connect(dialog, "response", G_CALLBACK(on_clear_reading_list_confirmation), win);
	adw_dialog_present(dialog, GTK_WIDGET(win));

	// popping down the popover is crucial to be able to receive the "response" signal
    main_window_popdown_all_popovers(win);
}

void
open_reading_list_bookmarks(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    BookmarksDialog* dialog = BOOKMARKS_DIALOG(bookmarks_dialog_new());
    bookmarks_dialog_sidebar_select(dialog, 2);
    adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(win));
}

void
sidebar_rename_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //relies on the exact structure of the widgets inside the columnview
    GtkWidget* box = main_window_get_last_right_clicked_widget(MAIN_WINDOW(win));
    OrionEditableCellEntry* entry = ORION_EDITABLE_CELL_ENTRY(gtk_widget_get_last_child(box));
    orion_editable_cell_entry_set_editable(entry,true);
    gtk_widget_grab_focus(GTK_WIDGET(entry));
}

void
show_tab_preview_on_hover(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GVariant* state = g_action_get_state(G_ACTION(self));
    defer([state]{ g_variant_unref(state); });
    if(!!g_variant_get_boolean(state) == !!g_variant_get_boolean(parameter)) return;

    global_core_api->set_setting(
        main_window_get_current_profile_id(win), orion::settings::tabs::show_tab_preview_on_hover,
        g_variant_get_boolean(parameter), [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
}

void
tab_switch_order(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    size_t len;
    const char* parameter_string = g_variant_get_string(parameter, &len);
    char* end;
    int val = strtol(parameter_string, &end, 10);
    if((size_t)(end-parameter_string)!=len)
    {
        orion::log_debug("programming error: tab_switch_order action got passed a bad string: \"%s\"", parameter_string);
        return;
    }
    orion::settings::TabSwitchOrder order = orion::settings::TabSwitchOrder(val);
    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::tabs::tab_switch_order, 
        order, [](orion::SetSettingReply<orion::settings::TabSwitchOrder> reply){
            VERIFY_LOG(reply.err);
        });
}

void
show_tab_switcher(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::tabs::use_tab_switcher_ui_for_ctrl_tab, 
        g_variant_get_boolean(parameter), [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
}

void
show_nested_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GVariant* state = g_action_get_state(G_ACTION(self));
    defer([state]{ g_variant_unref(state); });

    global_core_api->set_setting(
        main_window_get_current_profile_id(win), orion::settings::tabs::show_nested_vertical_tabs,
        !g_variant_get_boolean(state), [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
}

void
open_new_tab_next_to_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GVariant* state = g_action_get_state(G_ACTION(self));
    defer([state]{ g_variant_unref(state); });

    global_core_api->set_setting(
        main_window_get_current_profile_id(win), orion::settings::tabs::open_new_tab_next_to_current_tab,
        !g_variant_get_boolean(state), [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
}

void
show_favicons_inside_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GVariant* state = g_action_get_state(G_ACTION(self));
    defer([state]{ g_variant_unref(state); });

    global_core_api->set_setting(
        main_window_get_current_profile_id(win), orion::settings::tabs::show_favicons_inside_tabs,
        !g_variant_get_boolean(state), [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
}

void
always_show_website_titles_in_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GVariant* state = g_action_get_state(G_ACTION(self));
    defer([state]{ g_variant_unref(state); });

    global_core_api->set_setting(
        main_window_get_current_profile_id(win), orion::settings::tabs::always_show_website_titles_in_tabs,
        !g_variant_get_boolean(state), [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
}


void
when_opening_link_in_new_tab_make_it_active(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);

    GVariant* state = g_action_get_state(G_ACTION(self));
    defer([state]{ g_variant_unref(state); });

    global_core_api->set_setting(
        profile_id, orion::settings::tabs::when_opening_link_in_new_tab_make_it_active,
        !g_variant_get_boolean(state), [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        }
    );
}


void
open_recently_closed_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    auto tab_id = RecentlyClosedTabId(g_variant_get_int64(parameter));
    open_recently_closed_tab_internal(win, tab_id);
}

void
open_recently_closed_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    auto window_id = RecentlyClosedWindowId(g_variant_get_int64(parameter));
    open_recently_closed_window_internal(win, window_id);
}

void
auto_show_sidebar(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    WeakRefWrapper win_weak(win);
    ProfileId profile_id = main_window_get_current_profile_id(win);

    GVariant* state = g_action_get_state(G_ACTION(self));
    defer([state]{ g_variant_unref(state); });

    global_core_api->set_setting(
        profile_id, orion::settings::appearance::auto_show_sidebar,
        !g_variant_get_boolean(state), [win_weak](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);

            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            main_window_set_show_sidebar_setting(win, !reply.new_value);
        }
    );
}

void
auto_show_sidebar_in_fullscreen(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    WeakRefWrapper win_weak(win);
    ProfileId profile_id = main_window_get_current_profile_id(win);

    GVariant* state = g_action_get_state(G_ACTION(self));
    defer([state]{ g_variant_unref(state); });

    global_core_api->set_setting(
        profile_id, orion::settings::appearance::auto_show_sidebar_in_fullscreen,
        !g_variant_get_boolean(state), [win_weak](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        }
    );
}

//-------------------------------------------------------------------------
// Settings dialog
//-------------------------------------------------------------------------

void
share_crash_reports_setting(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    int32_t val = g_variant_get_int32(parameter);
    auto weak_ref = orion::gtk::WeakRefWrapper(self);
    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::privacy::share_crash_reports, 
        orion::settings::ShareCrashReports(val), [weak_ref](orion::SetSettingReply<orion::settings::ShareCrashReports> reply){
            VERIFY_LOG(reply.err);
            auto ref = weak_ref.lock();
            if (!ref) {
                return;
            }
            GAction* action = G_ACTION(ref.get());
            GVariant* var = g_variant_new_int32((int32_t)reply.new_value);
            g_action_change_state(action, var);
        });
}

void
app_icon_setting(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    std::string val = g_variant_get_string(parameter, NULL);
    auto weak_ref = orion::gtk::WeakRefWrapper(self);
    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::appearance::app_icon, 
        val, [weak_ref](orion::SetSettingReply<std::string> reply){
            VERIFY_LOG(reply.err);
            auto ref = weak_ref.lock();
            if (!ref) {
                return;
            }
            GAction* action = G_ACTION(ref.get());
            GVariant* var = g_variant_new_string(reply.new_value.c_str());
            g_action_change_state(action, var);
        });
}

void
tab_layout_setting(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    const char* string = g_variant_get_string(parameter, NULL);
    int32_t val = 0;
    if(strcmp(string, "standard")==0)
    {
        val = 1;
    }
    else if(strcmp(string, "compact")==0)
    {
        val = 2;
    }
    else if(strcmp(string, "vertical")==0)
    {
        val = 3;
    }

    ProfileId profile_id = main_window_get_current_profile_id(win);
    settings::TabLayout tab_layout = settings::TabLayout(val);
    WeakRefWrapper win_weak(win);

    if(val)
    {
        global_core_api->set_setting(profile_id, settings::tabs::tab_layout, tab_layout,
            [profile_id,win_weak](SetSettingReply<settings::TabLayout> reply){
                VERIFY_LOG(reply.err);

                auto old_layout = reply.old_value;
                auto new_layout = reply.new_value;

                auto win_strong = win_weak.lock();
                if (!win_strong) return;
                MainWindow* win = MAIN_WINDOW(win_strong.get());

                if (old_layout == settings::TabLayout::vertical && new_layout != settings::TabLayout::vertical) {
                    // When switching away from Vertical Tabs layout, also switch to Tab Groups page in Sidebar
                    // automatically (this is Mac behavior)
                    main_window_switch_to_sidebar_page(win, "windows");
                    main_window_set_show_sidebar_setting(win, FALSE);
                }
            }
        );
    }
}

void
file_download_location(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    size_t len;
    const char* parameter_string = g_variant_get_string(parameter, &len);
    char* end;
    int val = strtol(parameter_string, &end, 10);
    if((size_t)(end-parameter_string)!=len)
    {
        orion::log_debug("programming error: file_download_location action got passed a bad string: \"%s\"", parameter_string);
        return;
    }
    orion::settings::FileDownloadLocation value = orion::settings::FileDownloadLocation(val);
    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::general::file_download_location, 
        value, [](orion::SetSettingReply<orion::settings::FileDownloadLocation> reply){
            VERIFY_LOG(reply.err);
        });
}

void
file_download_custom_path(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    size_t len;
    const char* parameter_string = g_variant_get_string(parameter, &len);
    if(len)
    {
        global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::general::file_download_custom_path,
            parameter_string, [](orion::SetSettingReply<std::string> reply){
                VERIFY_LOG(reply.err);
            });
    }
    else
    {
        orion::log_debug("programming error: file_download_custom_path action got passed a bad string: \"%s\"", parameter_string);
    }
}

//-------------------------------------------------------------------------
// Bookmarks dialog
//-------------------------------------------------------------------------

void
bookmarks_open_in_new_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);

    GtkSelectionModel* selection = GTK_SELECTION_MODEL(main_window_get_context_menu_data(win)->user_data);
    GtkBitset* selection_bitset = gtk_selection_model_get_selection(selection);
    uint64_t bitset_size = gtk_bitset_get_size(selection_bitset);
    std::vector<BookmarkFolderId> folder_ids;
    std::vector<BookmarkFolderId> excluded_parents;
    std::vector<BookmarkId> bookmark_ids;
    if(bitset_size > 0)
    {
        for(uint64_t i = 0; i<bitset_size; i++)
        {
            uint64_t item_index = gtk_bitset_get_nth(selection_bitset, i);
            gpointer thing = g_list_model_get_item(G_LIST_MODEL(selection), item_index);
            while((GTK_IS_TREE_LIST_ROW(thing)))
            {
                defer( [thing]{g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer( [thing]{g_object_unref(thing); });
            BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
            if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::bookmark)
            {
                auto parent_folder_id = bookmarks_columnview_item_get_parent_folder_id(item);
                auto search_it = std::find_if(excluded_parents.cbegin(), excluded_parents.cend(),
                    [parent_folder_id](orion::BookmarkFolderId a){return a==parent_folder_id;});
                if(search_it==excluded_parents.cend())
                {
                    bookmark_ids.push_back(bookmarks_columnview_item_get_id(item).bookmark_id);
                }
            }
            else
            {
                //NOTE: requires that the bitset be top to bottom
                auto parent_folder_id = bookmarks_columnview_item_get_parent_folder_id(item);
                auto search_it = std::find_if(excluded_parents.cbegin(), excluded_parents.cend(),
                    [parent_folder_id](orion::BookmarkFolderId a){return a==parent_folder_id;});
                if(search_it==excluded_parents.cend())
                {
                    folder_ids.push_back(bookmarks_columnview_item_get_id(item).folder_id);
                }
                excluded_parents.push_back(bookmarks_columnview_item_get_id(item).folder_id);
            }
        }
    }
    else
    {
        GtkListItem* listitem = GTK_LIST_ITEM(main_window_get_last_right_clicked_widget(win));

        gpointer thing = g_object_ref(gtk_list_item_get_item(listitem));
        while((GTK_IS_TREE_LIST_ROW(thing)))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);

        if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::bookmark)
        {
            bookmark_ids.push_back(bookmarks_columnview_item_get_id(item).bookmark_id);
        }
        else
        {
            folder_ids.push_back(bookmarks_columnview_item_get_id(item).folder_id);
        }
    }
    gtk_bitset_unref(selection_bitset);
    for(auto folder_id : folder_ids)
    {
        orion_webview_tab_view_load_bookmark_folder(view, folder_id);
    }
    for(auto bookmark_id : bookmark_ids)
    {
        orion_webview_tab_view_load_bookmark(view, bookmark_id);
    }
}

void
bookmarks_open_in_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GtkSelectionModel* selection = GTK_SELECTION_MODEL(main_window_get_context_menu_data(win)->user_data);
    GtkBitset* selection_bitset = gtk_selection_model_get_selection(selection);
    uint64_t bitset_size = gtk_bitset_get_size(selection_bitset);
    auto folder_ids = std::make_shared<std::vector<BookmarkFolderId>>();
    std::vector<BookmarkFolderId> excluded_parents;
    auto bookmark_ids = std::make_shared<std::vector<BookmarkId>>();
    if(bitset_size > 0)
    {
        for(uint64_t i = 0; i<bitset_size; i++)
        {
            uint64_t item_index = gtk_bitset_get_nth(selection_bitset, i);
            gpointer thing = g_list_model_get_item(G_LIST_MODEL(selection), item_index);
            while((GTK_IS_TREE_LIST_ROW(thing)))
            {
                defer( [thing]{g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer( [thing]{g_object_unref(thing); });
            BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
            if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::bookmark)
            {
                auto parent_folder_id = bookmarks_columnview_item_get_parent_folder_id(item);
                auto search_it = std::find_if(excluded_parents.cbegin(), excluded_parents.cend(),
                    [parent_folder_id](orion::BookmarkFolderId a){return a==parent_folder_id;});
                if(search_it==excluded_parents.cend())
                {
                    bookmark_ids->push_back(bookmarks_columnview_item_get_id(item).bookmark_id);
                }
            }
            else
            {
                //NOTE: requires that the bitset be top to bottom
                auto parent_folder_id = bookmarks_columnview_item_get_parent_folder_id(item);
                auto search_it = std::find_if(excluded_parents.cbegin(), excluded_parents.cend(),
                    [parent_folder_id](orion::BookmarkFolderId a){return a==parent_folder_id;});
                if(search_it==excluded_parents.cend())
                {
                    folder_ids->push_back(bookmarks_columnview_item_get_id(item).folder_id);
                }
                excluded_parents.push_back(bookmarks_columnview_item_get_id(item).folder_id);
            }
        }
    }
    else
    {
        GtkListItem* listitem = GTK_LIST_ITEM(main_window_get_last_right_clicked_widget(win));

        gpointer thing = g_object_ref(gtk_list_item_get_item(listitem));
        while((GTK_IS_TREE_LIST_ROW(thing)))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);

        if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::bookmark)
        {
            bookmark_ids->push_back(bookmarks_columnview_item_get_id(item).bookmark_id);
        }
        else
        {
            folder_ids->push_back(bookmarks_columnview_item_get_id(item).folder_id);
        }
    }
    gtk_bitset_unref(selection_bitset);

    // The bookmark should always be opened in a new non-private window even if the current window is private
    orion::CreateWindowQuery query{};
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    query.profile_id = profile_id;
    query.icon_name = "window-flat-symbolic";
    query.is_tab_group = true; // this is never a saved window

    global_core_api->create_window(query, [folder_ids, bookmark_ids, profile_id](orion::CreateWindowReply reply){
        VERIFY_LOG(reply.err);
        MainWindow* win = main_window_new_from_backend(
            g_application_get_default(), profile_id, reply.id, orion::PrivateMode::no, // never private
            [folder_ids, bookmark_ids](MainWindow* win){

            OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
            for(auto folder_id : *folder_ids)
            {
                orion_webview_tab_view_load_bookmark_folder(view, folder_id);
            }
            for(auto bookmark_id : *bookmark_ids)
            {
                orion_webview_tab_view_load_bookmark(view, bookmark_id);
            }
        });
        orion::log_debug("new window id: %ld", reply.id.value);
        gtk_window_present(GTK_WINDOW(win));
    });
}

void
bookmarks_rename_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //relies on the exact structure of the widgets inside the columnview, plus things outside the public api

    GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(main_window_get_last_right_clicked_widget(MAIN_WINDOW(win)));
    orion::log_debug(g_type_name(G_OBJECT_TYPE(cell)));
    
    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(gtk_column_view_cell_get_child(cell));
    orion::log_debug(g_type_name(G_OBJECT_TYPE(row_widget)));

    GtkWidget* cell_widget = gtk_widget_get_first_child(row_widget);
    orion::log_debug(g_type_name(G_OBJECT_TYPE(cell_widget)));

    GtkWidget* tree_expander = gtk_widget_get_first_child(cell_widget);
    orion::log_debug(g_type_name(G_OBJECT_TYPE(tree_expander)));

    GtkWidget* box = gtk_widget_get_last_child(tree_expander);
    orion::log_debug(g_type_name(G_OBJECT_TYPE(box)));


    OrionEditableCellEntry* entry = ORION_EDITABLE_CELL_ENTRY(gtk_widget_get_last_child(box));
    orion_editable_cell_entry_set_editable(entry,true);
    gtk_widget_grab_focus(GTK_WIDGET(entry));
}

void
bookmarks_add_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //is reliant on passing through a columnview cell as the last right clicked "widget"
    GtkWidget* widget = main_window_get_last_right_clicked_widget(win);
    if(GTK_IS_LIST_ITEM(widget))
    {
        GtkListItem* listitem = GTK_LIST_ITEM(widget);
        gpointer thing = g_object_ref(gtk_list_item_get_item(listitem));
        while((GTK_IS_TREE_LIST_ROW(thing)))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
        AddBookmarkDialog* dialog = ADD_BOOKMARK_DIALOG(add_bookmark_dialog_new_init_from_current_page(win));

        if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::folder ||
        bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::folder_internal)
        {
            add_bookmark_dialog_set_bookmark_folder_id(dialog, bookmarks_columnview_item_get_id(item).folder_id);
        }

        adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(win));
    }
    else if(GTK_IS_LIST_VIEW(widget))
    {
        orion::BookmarkFolderId* folder_id = static_cast<orion::BookmarkFolderId*>(g_object_get_data(G_OBJECT(widget), "bookmarks-bar-folder-id"));
        if(folder_id)
        {
            AddBookmarkDialog* dialog = ADD_BOOKMARK_DIALOG(add_bookmark_dialog_new_init_from_current_page(win));
            add_bookmark_dialog_set_bookmark_folder_id(dialog, *folder_id);
            adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(win));
        }
    }
}

void
bookmarks_add_folder(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //TODO: translate the name untitled for the added folder

    gpointer data = main_window_get_last_right_clicked_widget(win);



    //is reliant on passing through a columnview cell as the last right clicked "widget"
    //need to check whether I even get this data, if not, blank area was clicked

    //only gets passed if right clicking on an item in the columnview
    if(GTK_IS_LIST_ITEM(data))
    {
        GtkListItem* listitem = GTK_LIST_ITEM(data);
        gpointer thing = g_object_ref(gtk_list_item_get_item(listitem));
        while((GTK_IS_TREE_LIST_ROW(thing)))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);

        if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::folder ||
        bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::folder_internal)
        {
            orion::CreateBookmarkFolderQuery query{};
            query.profile_id = main_window_get_current_profile_id(win);
            query.name = _("Untitled");
            query.parent_folder_id = bookmarks_columnview_item_get_id(item).folder_id;

            global_core_api->create_bookmark_folder(query, [](orion::CreateBookmarkFolderReply reply) {
                VERIFY_LOG(reply.err);
                orion::log_debug("bookmarks_add_folder(): created bookmark folder");
            });
        }
        else
        {
            orion::CreateBookmarkFolderQuery query{};
            query.profile_id = main_window_get_current_profile_id(win);
            query.name = _("Untitled");
            query.parent_folder_id = bookmarks_columnview_item_get_parent_folder_id(item);

            GListModel* list_store = G_LIST_MODEL(bookmarks_columnview_item_get_folder_list_store(item));
            int n_items = g_list_model_get_n_items(list_store);
            double seq_num;
            if(n_items>1)
            {
                int i = 0;
                
                //searches for the index of our item, should always be found since it always originates from the columnview
                for(; i<n_items; i++)
                {
                    gpointer tmp_item = g_list_model_get_item(list_store, i);
                    defer( [tmp_item]{g_object_unref(tmp_item); });
                    if(tmp_item == item)
                    {
                        break;
                    }
                }

                if(i == n_items)
                {
                    //fallback in case item is not found
                    gpointer last_item = g_list_model_get_item(list_store, n_items-1);
                    defer( [last_item]{g_object_unref(last_item);});
                    seq_num = bookmarks_columnview_item_get_sequence_number(last_item)+1;
                }
                else if(i == n_items-1)
                {
                    seq_num = bookmarks_columnview_item_get_sequence_number(item)+1;
                }
                else
                {
                    BookmarksColumnviewItem* item_b = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(list_store, i+1));
                    defer( [item_b]{ g_object_unref(item_b); });
                    double seq_num_a = bookmarks_columnview_item_get_sequence_number(item);
                    double seq_num_b = bookmarks_columnview_item_get_sequence_number(item_b);
                    seq_num = (seq_num_a + seq_num_b)/2.0;
                }
            }
            else
            {
                seq_num = bookmarks_columnview_item_get_sequence_number(item)+1;
            }
            query.seq_num = seq_num; // this should create the folder between Bookmarks Bar and Favorites folders

            global_core_api->create_bookmark_folder(query, [](orion::CreateBookmarkFolderReply reply) {
                VERIFY_LOG(reply.err);
                orion::log_debug("bookmarks_add_folder(): created bookmark folder");
            });
        }  
    }
    
    //will be triggered if clicked in blank area, needs to find the sequence number so should be passed the treelistmodel
    else if(GTK_IS_COLUMN_VIEW(data))
    {
        orion::CreateBookmarkFolderQuery query{};
        query.profile_id = main_window_get_current_profile_id(win);
        query.name = _("Untitled");
        global_core_api->create_bookmark_folder(query, [](orion::CreateBookmarkFolderReply reply) {
            VERIFY_LOG(reply.err);
            orion::log_debug("bookmarks_add_folder(): created bookmark folder");
        });
    }
    else if(GTK_IS_LIST_VIEW(data))
    {
        orion::BookmarkFolderId* folder_id = (orion::BookmarkFolderId*)g_object_get_data(G_OBJECT(data), "bookmarks-bar-folder-id");
        if(folder_id)
        {
            orion::CreateBookmarkFolderQuery query{};
            query.profile_id = main_window_get_current_profile_id(win);
            query.name = _("Untitled");
            query.parent_folder_id = *folder_id;
            global_core_api->create_bookmark_folder(query, [](orion::CreateBookmarkFolderReply reply){VERIFY_LOG(reply.err);});
        }
    }
}

void
bookmarks_delete_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    std::vector<orion::gtk::GObjectPtr>& selected_items = main_window_get_context_menu_data(win)->selected_items;
    std::vector<BookmarkFolderId> folder_ids;
    std::vector<BookmarkId> bookmark_ids;
    if(!selected_items.empty())
    {
        for(orion::gtk::GObjectPtr& item_ptr : selected_items)
        {
            gpointer thing = g_object_ref(item_ptr.get());
            while((GTK_IS_TREE_LIST_ROW(thing)))
            {
                defer([thing]{ g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer([thing]{ g_object_unref(thing); });
            BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
            if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::bookmark)
            {
                bookmark_ids.push_back(bookmarks_columnview_item_get_id(item).bookmark_id);
            }
            else if(!bookmarks_columnview_item_get_is_internal(item))
            {
                folder_ids.push_back(bookmarks_columnview_item_get_id(item).folder_id);
                //TODO: recursively delete all internal items?
            }
        }
    }
    else
    {
        GtkListItem* listitem = GTK_LIST_ITEM(main_window_get_last_right_clicked_widget(win));
        gpointer thing = g_object_ref(gtk_list_item_get_item(listitem));
        while((GTK_IS_TREE_LIST_ROW(thing)))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);

        if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::bookmark)
        {
            bookmark_ids.push_back(bookmarks_columnview_item_get_id(item).bookmark_id);
        }
        else if(!bookmarks_columnview_item_get_is_internal(item))
        {
            folder_ids.push_back(bookmarks_columnview_item_get_id(item).folder_id);
        }
    }
    delete_bookmark_folders(folder_ids);
    delete_bookmarks(bookmark_ids);
}

void
bookmarks_copy_bookmarks(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //NOTE: entirely ignoring selection because mac version does that
    //if you want better, copy code over from bookmarks_open_in_new_window
    orion::CopyBookmarksQuery* query = new orion::CopyBookmarksQuery{};

    GtkListItem* listitem = GTK_LIST_ITEM(main_window_get_last_right_clicked_widget(win));

    gpointer thing = g_object_ref(gtk_list_item_get_item(listitem));
    while((GTK_IS_TREE_LIST_ROW(thing)))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);

    if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::bookmark)
    {
        query->source_bookmark_ids.push_back(bookmarks_columnview_item_get_id(item).bookmark_id);
    }
    else
    {
        query->source_folder_ids.push_back(bookmarks_columnview_item_get_id(item).folder_id);
    }
    OrionApp* app = ORION_APP(g_application_get_default());
    orion_app_set_clipboard_data(app, query, orion::clipboard_data_type::bookmarks_copy_query);
}

void
bookmarks_paste_bookmarks(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //NOTE: entirely ignoring selection because mac version does that
    //if you want better, copy code over from bookmarks_open_in_new_window
    
    OrionApp* app = ORION_APP(g_application_get_default());
    if(orion_app_get_clipboard_data_type(app)!=orion::clipboard_data_type::bookmarks_copy_query) return;

    const orion::CopyBookmarksQuery* clipboard_data = static_cast<const orion::CopyBookmarksQuery*>(orion_app_get_clipboard_data(app));
    orion::CopyBookmarksQuery query(*clipboard_data);

    GtkListItem* listitem = GTK_LIST_ITEM(main_window_get_last_right_clicked_widget(win));

    gpointer thing = g_object_ref(gtk_list_item_get_item(listitem));
    while((GTK_IS_TREE_LIST_ROW(thing)))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);

    if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::bookmark)
    {
        query.target_bookmark_id = bookmarks_columnview_item_get_id(item).bookmark_id;
    }
    else
    {
        query.target_folder_id = bookmarks_columnview_item_get_id(item).folder_id;
    }
    global_core_api->copy_bookmarks(query, [](orion::CopyBookmarksReply reply){VERIFY_LOG(reply.err);});
}

//TODO: rename to open_history_etries_in_new_tab
void
open_history_address_in_new_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GtkSelectionModel* selection = GTK_SELECTION_MODEL(main_window_get_context_menu_data(win)->user_data);
    GtkBitset* selection_bitset = gtk_selection_model_get_selection(selection);
    defer([selection_bitset]{ gtk_bitset_unref(selection_bitset); });
    uint64_t bitset_size = gtk_bitset_get_size(selection_bitset);
    std::vector<std::string> addresses;
    if(bitset_size > 0)
    {
        for(uint64_t i = 0; i<bitset_size; i++)
        {
            uint64_t item_index = gtk_bitset_get_nth(selection_bitset, i);
            gpointer thing = g_list_model_get_item(G_LIST_MODEL(selection), item_index);
            while((GTK_IS_TREE_LIST_ROW(thing)))
            {
                defer( [thing]{g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer( [thing]{g_object_unref(thing); });
            HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(thing);
            if(history_columnview_item_get_item_type(item) == HistoryColumnviewItemType::address)
            {
                addresses.push_back(history_columnview_item_get_address(item)->str);
            }
            else
            {
                //NOTE: inoring as mac version also ignores
            }
        }
    }
    else
    {
        GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(main_window_get_last_right_clicked_widget(win));
        gpointer thing = g_object_ref(gtk_column_view_cell_get_item(cell));
        while((GTK_IS_TREE_LIST_ROW(thing)))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(thing);
        if(history_columnview_item_get_item_type(item) == HistoryColumnviewItemType::address)
        {
            addresses.push_back(history_columnview_item_get_address(item)->str);
        }
        else
        {
            orion::log_error("open_history_address_in_new_tab reached while right clicking a day row");
            return;
        }
    }

    for(std::string address : addresses)
    {
        orion_webview_tab_view_new_tab_with_uri(main_window_get_webview_tab_view(win), address.c_str());
    }
}

void
open_history_address_in_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GtkSelectionModel* selection = GTK_SELECTION_MODEL(main_window_get_context_menu_data(win)->user_data);
    GtkBitset* selection_bitset = gtk_selection_model_get_selection(selection);
    uint64_t bitset_size = gtk_bitset_get_size(selection_bitset);
    std::vector<std::string> addresses;
    if(bitset_size > 0)
    {
        for(uint64_t i = 0; i<bitset_size; i++)
        {
            uint64_t item_index = gtk_bitset_get_nth(selection_bitset, i);
            gpointer thing = g_list_model_get_item(G_LIST_MODEL(selection), item_index);
            while((GTK_IS_TREE_LIST_ROW(thing)))
            {
                defer( [thing]{g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer( [thing]{g_object_unref(thing); });
            HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(thing);
            if(history_columnview_item_get_item_type(item) == HistoryColumnviewItemType::address)
            {
                addresses.push_back(history_columnview_item_get_address(item)->str);
            }
            else
            {
                //NOTE: inoring as mac version also ignores
            }
        }
    }
    else
    {
        GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(main_window_get_last_right_clicked_widget(win));
        gpointer thing = g_object_ref(gtk_column_view_cell_get_item(cell));
        while((GTK_IS_TREE_LIST_ROW(thing)))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(thing);
        if(history_columnview_item_get_item_type(item) == HistoryColumnviewItemType::address)
        {
            addresses.push_back(history_columnview_item_get_address(item)->str);
        }
        else
        {
            orion::log_debug("programming error: open_history_address_in_new_tab reached while right clicking a day row");
            return;
        }
    }
    orion::ProfileId profile_id = main_window_get_current_profile_id(win); 

    // The bookmark should always be opened in a new non-private window even if the current window is private
    orion::CreateWindowQuery query{};
    query.profile_id = profile_id;

    global_core_api->create_window(query, [addresses = std::move(addresses), profile_id](orion::CreateWindowReply reply){
        VERIFY_LOG(reply.err);
        for(std::string address : addresses)
        {
            orion::CreateTabQuery query{};
            query.window_id = reply.id;
            query.uri = address;
            global_core_api->create_tab(query, [](orion::CreateTabReply reply){VERIFY_LOG(reply.err);});
        }
        orion::profile::open_window(profile_id, reply.id, orion::PrivateMode::no); // never private
    });
    gtk_bitset_unref(selection_bitset);
}

void
open_history_address_in_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    const char* address = g_variant_get_string(parameter, NULL);
    main_window_load_bookmark(win, address);
}

// output format example: "2025-09-17 08:00:51"
static std::string
format_history_entry_timestamp(UnixTimeUtc64 timestamp)
{
    GDateTime* date_time = g_date_time_new_from_unix_utc(timestamp.value);
    defer([date_time]{ g_date_time_unref(date_time); });

    GDateTime* local_time = g_date_time_to_local(date_time);
    defer([local_time]{ g_date_time_unref(local_time); });

    gchar* formatted = g_date_time_format(local_time, "%Y-%m-%d %H:%M:%S");
    defer([formatted]{ g_free(formatted); });

    return std::string(formatted);
}

void
copy_history_address(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GtkSelectionModel* selection = GTK_SELECTION_MODEL(main_window_get_context_menu_data(win)->user_data);
    GtkBitset* selection_bitset = gtk_selection_model_get_selection(selection);
    defer([selection_bitset]{ gtk_bitset_unref(selection_bitset); });
    uint64_t bitset_size = gtk_bitset_get_size(selection_bitset);
    std::vector<HistoryColumnviewItem*> items;
    if(bitset_size > 0)
    {
        for(uint64_t i = 0; i<bitset_size; i++)
        {
            uint64_t item_index = gtk_bitset_get_nth(selection_bitset, i);
            gpointer thing = g_list_model_get_item(G_LIST_MODEL(selection), item_index);
            while((GTK_IS_TREE_LIST_ROW(thing)))
            {
                defer( [thing]{g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer( [thing]{g_object_unref(thing); });
            HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(thing);
            if(history_columnview_item_get_item_type(item) == HistoryColumnviewItemType::address)
            {
                items.push_back(item);
            }
            else
            {
                //NOTE: inoring as mac version also ignores
            }
        }
    }
    else
    {
        GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(main_window_get_last_right_clicked_widget(win));
        gpointer thing = g_object_ref(gtk_column_view_cell_get_item(cell));
        while((GTK_IS_TREE_LIST_ROW(thing)))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(thing);
        if(history_columnview_item_get_item_type(item) == HistoryColumnviewItemType::address)
        {
            items.push_back(item);
            //TODO: potentially better manage lifetime
        }
        else
        {
            orion::log_debug("programming error: copy_history_address while right clicking a day row");
            return;
        }
    }

    std::string result;
    for(size_t i = 0; i<items.size(); i++)
    {
        HistoryColumnviewItem* item = items.at(i);
        result += format_history_entry_timestamp(orion::UnixTimeUtc64(g_date_time_to_unix(history_columnview_item_get_date(item))))
        + " " + history_columnview_item_get_website(item)->str + " " + history_columnview_item_get_address(item)->str;
        if(i<(items.size()-1))
        {
            result += "\n";
        }
    }

    GdkDisplay* display = gdk_display_get_default();
    GdkClipboard* clipboard = gdk_display_get_clipboard(display);
    gdk_clipboard_set_text(clipboard, result.c_str());
}

void
history_columnview_delete(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //TODO: redo
    GtkSelectionModel* selection = GTK_SELECTION_MODEL(main_window_get_context_menu_data(win)->user_data);
    GtkBitset* selection_bitset = gtk_selection_model_get_selection(selection);
    uint64_t bitset_size = gtk_bitset_get_size(selection_bitset);
    std::vector<HistoryEntryId> ids;
    if(bitset_size > 0)
    {
        for(uint64_t i = 0; i<bitset_size; i++)
        {
            uint64_t item_index = gtk_bitset_get_nth(selection_bitset, i);
            gpointer thing = g_list_model_get_item(G_LIST_MODEL(selection), item_index);
            while((GTK_IS_TREE_LIST_ROW(thing)))
            {
                defer( [thing]{g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer( [thing]{g_object_unref(thing); });
            HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(thing);
            if(history_columnview_item_get_item_type(item) == HistoryColumnviewItemType::address)
            {
                ids.push_back(history_columnview_item_get_id(item));
            }
            else
            {
                GDateTime* time = history_columnview_item_get_date(item);
                orion::GetHistoryEntryIdsQuery query;
                query.profile_id = main_window_get_current_profile_id(win);
                query.visited_after = static_cast<orion::UnixTimeUtc64>(g_date_time_to_unix(time));
                query.visited_before = static_cast<orion::UnixTimeUtc64>(g_date_time_to_unix(g_date_time_add_days(time, 1)));
                global_core_api->get_history_entry_ids(query, [](orion::GetHistoryEntryIdsReply reply){
                    VERIFY_LOG(reply.err);
                    global_core_api->delete_history_entries(reply.ids, [](orion::Error err){VERIFY_LOG(err);});
                });
            }
        }
    }
    else
    {
        GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(main_window_get_last_right_clicked_widget(win));
        gpointer thing = g_object_ref(gtk_column_view_cell_get_item(cell));
        while((GTK_IS_TREE_LIST_ROW(thing)))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(thing);
        if(history_columnview_item_get_item_type(item) == HistoryColumnviewItemType::address)
        {
            ids.push_back(history_columnview_item_get_id(item));
        }
        else
        {
            GDateTime* time = history_columnview_item_get_date(item);
            orion::GetHistoryEntryIdsQuery query;
            query.profile_id = main_window_get_current_profile_id(win);
            query.visited_after = static_cast<orion::UnixTimeUtc64>(g_date_time_to_unix(time));
            query.visited_before = static_cast<orion::UnixTimeUtc64>(g_date_time_to_unix(g_date_time_add_days(time, 1)));
            global_core_api->get_history_entry_ids(query, [](orion::GetHistoryEntryIdsReply reply){
                VERIFY_LOG(reply.err);
                global_core_api->delete_history_entries(reply.ids, [](orion::Error err){VERIFY_LOG(err);});
            });
        }
    }
    global_core_api->delete_history_entries(ids, [](orion::Error err){VERIFY_LOG(err);});
    gtk_bitset_unref(selection_bitset);
}

void
reading_list_columnview_delete(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GtkSelectionModel* selection = GTK_SELECTION_MODEL(main_window_get_context_menu_data(win)->user_data);
    GtkBitset* selection_bitset = gtk_selection_model_get_selection(selection);
    uint64_t bitset_size = gtk_bitset_get_size(selection_bitset);
    std::vector<ReadingListEntryId> ids;
    if(bitset_size > 0)
    {
        for(uint64_t i = 0; i<bitset_size; i++)
        {
            uint64_t item_index = gtk_bitset_get_nth(selection_bitset, i);
            gpointer thing = g_list_model_get_item(G_LIST_MODEL(selection), item_index);
            while((GTK_IS_TREE_LIST_ROW(thing)))
            {
                defer( [thing]{g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer( [thing]{g_object_unref(thing); });
            ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(thing);
            if(reading_list_columnview_item_get_item_type(item) == ReadingListColumnviewItemType::address)
            {
                ids.push_back(reading_list_columnview_item_get_id(item));
            }
            else
            {
                GDateTime* time = reading_list_columnview_item_get_date(item);
                orion::GetReadingListEntryIdsQuery query;
                query.profile_id = main_window_get_current_profile_id(win);
                query.added_after = static_cast<orion::UnixTimeUtc64>(g_date_time_to_unix(time));
                query.added_before = static_cast<orion::UnixTimeUtc64>(g_date_time_to_unix(g_date_time_add_days(time, 1)));
                global_core_api->get_reading_list_entry_ids(query, [](orion::GetReadingListEntryIdsReply reply){
                    VERIFY_LOG(reply.err);
                    global_core_api->delete_reading_list_entries(reply.ids, [](orion::Error err){VERIFY_LOG(err);});
                });
            }
        }
    }
    else
    {
        GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(main_window_get_last_right_clicked_widget(win));
        gpointer thing = g_object_ref(gtk_column_view_cell_get_item(cell));
        while((GTK_IS_TREE_LIST_ROW(thing)))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(thing);
        if(reading_list_columnview_item_get_item_type(item) == ReadingListColumnviewItemType::address)
        {
            ids.push_back(reading_list_columnview_item_get_id(item));
        }
        else
        {
            GDateTime* time = reading_list_columnview_item_get_date(item);
            orion::GetReadingListEntryIdsQuery query;
            query.profile_id = main_window_get_current_profile_id(win);
            query.added_after = static_cast<orion::UnixTimeUtc64>(g_date_time_to_unix(time));
            query.added_before = static_cast<orion::UnixTimeUtc64>(g_date_time_to_unix(g_date_time_add_days(time, 1)));
            global_core_api->get_reading_list_entry_ids(query, [](orion::GetReadingListEntryIdsReply reply){
                VERIFY_LOG(reply.err);
                global_core_api->delete_reading_list_entries(reply.ids, [](orion::Error err){VERIFY_LOG(err);});
            });
        }
    }
    gtk_bitset_unref(selection_bitset);
    global_core_api->delete_reading_list_entries(ids, [](orion::Error err){VERIFY_LOG(err);});
}

void
reading_list_columnview_open_in_new_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(main_window_get_last_right_clicked_widget(win));
    gpointer thing = g_object_ref(gtk_column_view_cell_get_item(cell));
    while((GTK_IS_TREE_LIST_ROW(thing)))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(thing);
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    orion_webview_tab_view_new_tab_with_uri(view, reading_list_columnview_item_get_address(item)->str);
}

void
reading_list_columnview_open_in_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(main_window_get_last_right_clicked_widget(win));
    gpointer thing = g_object_ref(gtk_column_view_cell_get_item(cell));
    while((GTK_IS_TREE_LIST_ROW(thing)))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(thing);
    std::string address = reading_list_columnview_item_get_address(item)->str;

    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    // The reading list entry should always be opened in a new non-private window even if the current window is private
    orion::CreateWindowQuery query{};
    query.icon_name = "window-flat-symbolic";
    query.profile_id = profile_id;
    query.is_tab_group = true; // this is never a saved window

    global_core_api->create_window(query, [address, profile_id](orion::CreateWindowReply reply){
        VERIFY_LOG(reply.err);
        orion::WindowId window_id = reply.id;
        orion::CreateTabQuery query{};
        query.uri = address;
        query.window_id = window_id;
        global_core_api->create_tab(query, [profile_id, window_id](orion::CreateTabReply reply){
            VERIFY_LOG(reply.err);
            MainWindow* new_win = main_window_new_from_backend(
                g_application_get_default(), profile_id, window_id, orion::PrivateMode::no); // never private
            gtk_window_present(GTK_WINDOW(new_win));
        });
    });
}

//-------------------------------------------------------------------------
// Bookmarks bar
//-------------------------------------------------------------------------

void
bookmarks_bar_rename_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //relies on the exact structure of the widgets inside the columnview, plus things outside the public api
    GtkListItem* listitem = GTK_LIST_ITEM(main_window_get_last_right_clicked_widget(MAIN_WINDOW(win)));
    GtkWidget* button = gtk_list_item_get_child(listitem);

    //see bookmarks_bar_listview.cpp factory_bind for the structure of widgets
    GtkBox* box;
    if(GTK_IS_BUTTON(button)) box = GTK_BOX(gtk_button_get_child(GTK_BUTTON(button)));
    else box = GTK_BOX(gtk_menu_button_get_child(GTK_MENU_BUTTON(button)));

    //first child is the icon, second (and also the last) is the entry
    OrionEditableCellEntry* entry = ORION_EDITABLE_CELL_ENTRY(gtk_widget_get_last_child(GTK_WIDGET(box)));

    //we need to make the entry (temporarily) visible even if icon_only bookmarks bar style is active,
    //in which case the entry should be hidden automatically because of bookmarks change handler
    gtk_widget_set_visible(GTK_WIDGET(entry), true);
    
    orion_editable_cell_entry_set_editable(entry,true);
    gtk_widget_grab_focus(GTK_WIDGET(entry));
}

void
bookmarks_bar_open_in_new_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::BookmarkFolderId folder_id = orion::BookmarkFolderId(g_variant_get_int64(parameter));
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    orion_webview_tab_view_load_bookmark_folder(view, folder_id);
}

void
hide_bookmarks_bar(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::appearance::show_bookmarks_bar, false,
        [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
}

void
bookmarks_bar_style(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    const char* parameter_string = g_variant_get_string(parameter, NULL);
    char* end;
    int val = strtol(parameter_string, &end, 10);
    if((size_t)(end-parameter_string)!=strlen(parameter_string))
    {
        orion::log_debug("programming error: bookmarks_bar_style action got passed a bad string: \"%s\"", parameter_string);
        return;
    }
    orion::settings::BookmarksBarStyle style = orion::settings::BookmarksBarStyle(val);
    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::appearance::bookmarks_bar_style, style,
        [](orion::SetSettingReply<orion::settings::BookmarksBarStyle> reply){
            VERIFY_LOG(reply.err);
        });
}

//-------------------------------------------------------------------------
// Window List Button
//-------------------------------------------------------------------------

void
window_list_button_style(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    const char* parameter_string = g_variant_get_string(parameter, NULL);
    char* end;
    int val = strtol(parameter_string, &end, 10);
    if((size_t)(end-parameter_string)!=strlen(parameter_string))
    {
        orion::log_debug("programming error: window_list_button_style action got passed a bad string: \"%s\"", parameter_string);
        return;
    }
    orion::settings::WindowListButtonStyle style = orion::settings::WindowListButtonStyle(val);
    global_core_api->set_setting(main_window_get_current_profile_id(win), orion::settings::appearance::window_list_button_style, style,
        [](orion::SetSettingReply<orion::settings::WindowListButtonStyle> reply){
            VERIFY_LOG(reply.err);
        });
}

//-------------------------------------------------------------------------
// Window List Box
//-------------------------------------------------------------------------

void
take_workspace_snapshot(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    global_core_api->create_snapshot(profile_id, [](orion::CreateSnapshotReply reply){
        VERIFY_LOG(reply.err);
        orion::log_debug("take_workspace_snapshot: created snapshot with timestamp %lld", reply.created_at.value);
    });
}

struct RestoreSnapshotCallbackData
{
    MainWindow* win;
    ProfileId profile_id;
    orion::UnixTimeUtc64 created_at;
};

static void
on_restore_snapshot_confirmation(AdwAlertDialog* self, gchar* response, gpointer user_data)
{
	orion::log_debug("on_restore_snapshot_confirmation: %s", response);
    if (g_strcmp0(response, "yes") != 0) {
        return;
    }

    RestoreSnapshotCallbackData* data = static_cast<RestoreSnapshotCallbackData*>(user_data);

    orion::RestoreSnapshotQuery query{
        .profile_id = data->profile_id,
        .created_at = data->created_at,
    };

    // we need to close all existing UI main windows first - this process has to run asynchronously
    global_core_api->async([query]{    
        // first, close all existing UI windows

        // create a temporary dummy UI window reference to prevent the app from quitting
        // after we close all existing UI windows
        global_core_api->ref_ui_window();
        defer([]{ global_core_api->unref_ui_window(); });

        std::vector<UiWindowId> ui_ids = global_core_api->get_registered_ui_window_ids();
        for (UiWindowId ui_id : ui_ids) {
            orion::log_debug("restore_snapshot: closing window with ui_id %p", ui_id.value);
            MainWindow* main_window = MAIN_WINDOW(ui_id.value);
            main_window_close(main_window);
        }

        // second, open UI windows from the saved snapshot
        global_core_api->restore_snapshot(query, [profile_id = query.profile_id](orion::RestoreSnapshotReply reply){
            VERIFY_LOG(reply.err);
            orion::log_debug("restore_snapshot: restored snapshot with timestamp %lld", reply.created_at.value);
            
            orion::profile::open_from_snapshot(profile_id);
        });
    });
}

// for example "Today, 3:45 PM", or "5. 9. 2025, 3:56 PM"
std::string
format_timestamp(orion::UnixTimeUtc64 timestamp)
{
    GDateTime* date_time = g_date_time_new_from_unix_utc(timestamp.value);
    defer([date_time]{ g_date_time_unref(date_time); });

    GDateTime* local_time = g_date_time_to_local(date_time);
    defer([local_time]{ g_date_time_unref(local_time); });
    
    {
        // handle today
        GDateTime* now = g_date_time_new_now_utc();
        defer([now]{ g_date_time_unref(now); });
        if (g_date_time_get_year(now) == g_date_time_get_year(date_time) &&
            g_date_time_get_month(now) == g_date_time_get_month(date_time) &&
            g_date_time_get_day_of_month(now) == g_date_time_get_day_of_month(date_time)
        ) {
            char* formatted = g_date_time_format(local_time, "%-I:%M %p");
            defer([formatted]{ g_free(formatted); });
            return std::string(_("Today")) + ", " + formatted;
        }
    }

    char* formatted = g_date_time_format(local_time, "%-e. %-m. %Y, %-I:%M %p");
    defer([formatted]{ g_free(formatted); });
    std::string result = formatted;
    return result;
}

static void
no_snapshot_to_restore_dialog(MainWindow* win)
{
    AdwDialog* dialog = adw_alert_dialog_new(_("No Snapshot"), _("No workspace snapshot created yet."));

    adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "ok", "_OK");
    adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "ok", ADW_RESPONSE_SUGGESTED);
    adw_dialog_present(dialog, GTK_WIDGET(win));
}

void
restore_snapshot_menu(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_last_snapshot(profile_id, [win_weak](orion::GetLastSnapshotReply reply){
        VERIFY_LOG(reply.err);
        orion::log_debug("get_last_snapshot: got snapshot with timestamp %lld", reply.snapshot.created_at.value);

        auto win_ptr = win_weak.lock();
        if (!win_ptr) return;
        MainWindow* win = MAIN_WINDOW(win_ptr.get());

        if (reply.snapshot.created_at == orion::UnixTimeUtc64(0)) {
            no_snapshot_to_restore_dialog(win);
            orion::log_warning("restore_snapshot_menu: no snapshot to restore");
            return;
        }

        std::string timestamp_desc = format_timestamp(reply.snapshot.created_at);

        AdwDialog* dialog = adw_alert_dialog_new(NULL, NULL);

        adw_alert_dialog_format_body_markup(
            ADW_ALERT_DIALOG(dialog),
            _("<b>Are you sure you want to restore snapshot taken on “%s”?</b>"),
            timestamp_desc.c_str()
        );

        adw_alert_dialog_add_responses(
            ADW_ALERT_DIALOG(dialog),
            "no", _("_No"),
            "yes", _("_Yes"),
            NULL
        );

        adw_alert_dialog_set_response_appearance(
            ADW_ALERT_DIALOG(dialog),
            "yes",
            ADW_RESPONSE_SUGGESTED
        );

        adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "yes");
        adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "no");

        RestoreSnapshotCallbackData* callback_data = new RestoreSnapshotCallbackData{
            .win = win,
            .profile_id = main_window_get_current_profile_id(win),
            .created_at = reply.snapshot.created_at,
        };

        g_signal_connect_data(dialog, "response", G_CALLBACK(on_restore_snapshot_confirmation), callback_data,
            [](gpointer data, GClosure* closure){
                delete static_cast<RestoreSnapshotCallbackData*>(data);
            }, G_CONNECT_AFTER);

        adw_dialog_present(dialog, GTK_WIDGET(win));

        // popping down the popover is crucial to be able to receive the "response" signal
        main_window_popdown_all_popovers(win);
    });
}

void
show_recently_opened_on_top(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GVariant* state = g_action_get_state(G_ACTION(self));
    defer([state]{ g_variant_unref(state); });

    global_core_api->set_setting(
        main_window_get_current_profile_id(win),
        orion::settings::windows::show_recently_opened_on_top,
        !g_variant_get_boolean(state),
        [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
}

void
switch_saved_windows_in_place(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GVariant* state = g_action_get_state(G_ACTION(self));
    defer([state]{ g_variant_unref(state); });

    global_core_api->set_setting(
        main_window_get_current_profile_id(win),
        orion::settings::windows::switch_saved_windows_in_place,
        !g_variant_get_boolean(state),
        [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
}

void
duplicate_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::WindowId window_id = static_cast<orion::WindowId>(g_variant_get_int64(parameter));
    orion::gtk::WeakRefWrapper win_weak(win);
    
    global_core_api->get_windows({window_id}, [window_id,win_weak](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);

        if (reply.windows.empty()) {
            orion::log_warning("duplicate_window: no window found with id %lu", window_id);
            return;
        }

        const auto& window = reply.windows.front();

        // By default, the duplicated window name should be empty (it is an Unsaved Tab Group)
        std::string name = "";

        if (window.is_private) {
            // The duplicated private window name should be "Copy of X" where X is the original window name
            name = orion::text::translatef("Copy of %1$s", window.name.c_str());
        }

        orion::DuplicateWindowQuery query{
            .window_id = window_id,
            .name = name,
        };

        global_core_api->duplicate_window(query, [win_weak,window](orion::DuplicateWindowReply reply){
            VERIFY_LOG(reply.err);

            auto win_ptr = win_weak.lock();
            if(!win_ptr) return;
            MainWindow* win = MAIN_WINDOW(win_ptr.get());
            ProfileId profile_id = main_window_get_current_profile_id(win);
            PrivateMode private_mode = window.is_private ? PrivateMode::yes : PrivateMode::no;

            orion::profile::open_window(profile_id, reply.new_window_id, private_mode);
        });
    });
}

void
open_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::WindowId window_id = static_cast<orion::WindowId>(g_variant_get_int64(parameter));
    main_window_switch_to_window_id_in_place(win, window_id);
}

// Closes all UI windows that are owned by the given window_id.
void
close_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::WindowId window_id = static_cast<orion::WindowId>(g_variant_get_int64(parameter));
    close_window_internal(window_id);
}

static void
switch_main_windows_to_different_window_id(ProfileId profile_id, WindowId window_id)
{
    // in each main window, switch to the first existing window that is not suspended (if possible)
    global_core_api->get_window_ids({.profile_id=profile_id}, [profile_id,window_id](orion::GetWindowIdsReply reply){
        VERIFY_LOG(reply.err);

        global_core_api->get_windows(reply.ids, [profile_id,window_id](orion::GetWindowsReply reply){
            VERIFY_LOG(reply.err);

            // cannot use std::map because we need to preserve insertion order
            std::vector<orion::WindowDto> window_dtos = reply.windows;

            std::vector<UiWindowId> main_windows = global_core_api->get_ui_windows_by_window_id(window_id);
            for (UiWindowId ui_id : main_windows) {
                MainWindow* main_window = MAIN_WINDOW(ui_id.value);

                // try to switch to the first non-suspended window that is not the current one
                for (const auto& window : window_dtos) {
                    if (window.id != window_id && !window.is_suspended) {
                        main_window_switch_to_window_id_in_place(main_window, window.id);
                        return;
                    }
                }

                // try to switch to the first window that is not the current one
                for (const auto& window : window_dtos) {
                    if (window.id != window_id) {
                        main_window_switch_to_window_id_in_place(main_window, window.id);
                        return;
                    }
                }

                // no windows left to switch to - close the main window
                orion::log_debug("switch_main_windows_to_different_window_id: no other windows in profile %lu, closing main window %p",
                    main_window_get_current_profile_id(main_window),
                    main_window
                );
                main_window_close(main_window);
            }
        });
    });
}

void
suspend_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    // TODO: suspend the window in webkit
    orion::WindowId window_id = static_cast<orion::WindowId>(g_variant_get_int64(parameter));
    ProfileId profile_id = main_window_get_current_profile_id(win);

    orion::UpdateWindowQuery query{};
    query.id = window_id;
    query.is_suspended = true;
    query.update_is_suspended = true;

    global_core_api->update_window(query, [window_id,profile_id](orion::Error err){
        VERIFY_LOG(err);
        // in each main window, switch to the first existing window if the current one is suspended        
        switch_main_windows_to_different_window_id(profile_id, window_id);
    });
}

struct DeleteStoredWindowCallbackData
{
    MainWindow* main_window;
    orion::WindowId window_id;
};

static void
on_delete_stored_window_confirmation(AdwAlertDialog* self, gchar* response, gpointer user_data)
{
    // alert dialog is always closed automatically after any response
	if (g_strcmp0(response, "cancel") == 0) {
        return;
    }

    DeleteStoredWindowCallbackData* callback_data = static_cast<DeleteStoredWindowCallbackData*>(user_data);
    MainWindow* main_window = callback_data->main_window;
    orion::WindowId window_id = callback_data->window_id;

    ProfileId profile_id = main_window_get_current_profile_id(main_window);

    global_core_api->delete_windows({window_id}, [window_id,profile_id](orion::Error err){
        VERIFY_LOG(err);

        // in each main window, switch to the first existing window if the current one is deleted        
        switch_main_windows_to_different_window_id(profile_id, window_id);
    });
}

void
delete_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::WindowId window_id = static_cast<orion::WindowId>(g_variant_get_int64(parameter));
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_windows({window_id}, [window_id,win_weak](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.windows.empty()) {
            orion::log_warning("delete_window: no such window with id %lu", window_id);
            return;
        }
        orion::WindowDto window = reply.windows.at(0);

        orion::GetTabIdsQuery query{};
        query.window_id = window_id;
        global_core_api->get_tab_ids(query, [window,win_weak](orion::GetTabIdsReply reply){
            VERIFY_LOG(reply.err);

            auto win_ptr = win_weak.lock();
            if (!win_ptr) return;
            MainWindow* win = MAIN_WINDOW(win_ptr.get());

            const char* window_group_name = window.name.c_str();
            int tabs_count = reply.ids.size();

            AdwDialog* dialog = adw_alert_dialog_new(NULL, NULL);

            adw_alert_dialog_format_heading(ADW_ALERT_DIALOG(dialog), _("Delete “%s”?"), window_group_name);

            adw_alert_dialog_format_body_markup(
                ADW_ALERT_DIALOG(dialog),
                _("Are you sure you want to delete “%s”?\n\nThis would delete a group with %d tabs."),
                window_group_name,
                tabs_count);

            adw_alert_dialog_add_responses(
                ADW_ALERT_DIALOG(dialog),
                "cancel", _("_Cancel"),
                "delete", _("_Delete"),
                NULL);

            adw_alert_dialog_set_response_appearance(
                ADW_ALERT_DIALOG(dialog),
                "delete",
                ADW_RESPONSE_DESTRUCTIVE);

            adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "delete");
            adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");

            // pass both window.id and main_window pointer to the callback
            DeleteStoredWindowCallbackData* callback_data = new DeleteStoredWindowCallbackData{win, window.id};

            g_signal_connect_data(dialog, "response", G_CALLBACK(on_delete_stored_window_confirmation), callback_data,
                [](gpointer data, GClosure* closure){
                    delete static_cast<DeleteStoredWindowCallbackData*>(data);
                },
                G_CONNECT_DEFAULT);

            adw_dialog_present(dialog, GTK_WIDGET(win));

            // popping down the popovers is crucial to be able to receive the "response" signal
            main_window_popdown_all_popovers(win);
        });
    });
}

void
copy_tab_links_of_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::WindowId window_id = static_cast<orion::WindowId>(g_variant_get_int64(parameter));

    global_core_api->get_windows({window_id}, [window_id](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.windows.empty()) {
            orion::log_warning("copy_tab_links_of_window: no such window with id %lu", window_id);
            return;
        }
        orion::WindowDto window = reply.windows.at(0);

        orion::GetTabIdsQuery query{};
        query.window_id = window_id;
        global_core_api->get_tab_ids(query, [window](orion::GetTabIdsReply reply){
            VERIFY_LOG(reply.err);
            
            global_core_api->get_tabs(reply.ids, [window](orion::GetTabsReply reply){
                VERIFY_LOG(reply.err);
                std::string result;
                // include Window title and web page title, not just URIs
                if (window.is_tab_group) {
                    result += _("Tab Group");
                } else {
                    result += window.name;
                }

                for (const auto& tab : reply.tabs)
                {
                    result += "\n\n";
                    bool is_start_page = tab.uri.empty() || tab.uri == "orion://newtab/";
                    if (is_start_page) {
                        result += _("New Tab");
                    } else {
                        result += tab.name;
                    }
                    result += "\n";

                    if (is_start_page) {
                        result += "orion://newtab/";
                    } else {
                        result += tab.uri;
                    }
                }
                GdkDisplay* display = gdk_display_get_default();
                GdkClipboard* clipboard = gdk_display_get_clipboard(display);
                gdk_clipboard_set_text(clipboard, result.c_str());
            });
        });
    });
}

void
bookmark_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    WindowId window_id = static_cast<WindowId>(g_variant_get_int64(parameter));
    
    MainWindow* win = MAIN_WINDOW(user_data);
    WeakRefWrapper win_weak(win);

    global_core_api->get_windows({window_id}, [window_id,win_weak](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.windows.empty()) {
            log_warning("bookmark_tabs: no such window with id %lu", window_id);
            return;
        }
        WindowDto window = reply.windows.at(0);

        GetTabIdsQuery query{};
        query.window_id = window_id;

        global_core_api->get_tab_ids(query, [win_weak,window](orion::GetTabIdsReply reply){
            VERIFY_LOG(reply.err);
            if (reply.ids.empty()) {
                log_warning("bookmark_tabs: window %lu has no tabs", window.id.value);
                return;
            }

            auto win_ptr = win_weak.lock();
            if (!win_ptr) return;
            MainWindow* win = MAIN_WINDOW(win_ptr.get());
            
            std::string folder_name;
            if (window.is_tab_group) {
                folder_name = _("Tab Group");
            } else {
                folder_name = window.name;
            }

            AddBookmarksForTabsDialog* dialog = ADD_BOOKMARKS_FOR_TABS_DIALOG(add_bookmarks_for_tabs_dialog_new());
            add_bookmarks_for_tabs_dialog_set_source_tab_ids(dialog, reply.ids);
            add_bookmarks_for_tabs_dialog_set_target_folder_name(dialog, folder_name.c_str());
            adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(win));
        });
    });
}

void
suspend_tabs(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    std::vector<int64_t> ids = orion::gtk::get_int64_vector_from_fixed_array_variant(parameter);
    std::vector<orion::TabId> tab_ids;
    tab_ids.reserve(ids.size());
    std::transform(ids.begin(), ids.end(), std::back_inserter(tab_ids),
        [](int64_t id){ return orion::TabId(id); }
    );

    suspend_tabs_internal(win, tab_ids);
}

void
suspend_tabs_in_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    WindowId window_id = static_cast<orion::WindowId>(g_variant_get_int64(parameter));
    WeakRefWrapper win_weak(win);

    GetTabIdsQuery query{
        .window_id = window_id,
    };

    global_core_api->get_tab_ids(query, [win_weak](GetTabIdsReply reply){
        VERIFY_LOG(reply.err);

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        suspend_tabs_internal(win, reply.ids);
    });
}

void
open_window_in_separate_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::WindowId window_id = static_cast<orion::WindowId>(g_variant_get_int64(parameter));
    open_window_in_separate_window_internal(window_id);
}

[[maybe_unused]] static void
popdown_all_popovers(GtkWidget *widget)
{
    if (GTK_IS_POPOVER(widget)) {
        if (gtk_widget_get_visible(widget))
            gtk_popover_popdown(GTK_POPOVER(widget));
    }

    if (GTK_IS_WIDGET(widget)) {
        for (GtkWidget *child = gtk_widget_get_first_child(widget);
             child != NULL;
             child = gtk_widget_get_next_sibling(child)) {
            popdown_all_popovers(child);
        }
    }
}

void
open_window_in_separate_window_or_switch_to_existing(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    WindowId window_id = static_cast<WindowId>(g_variant_get_int64(parameter));
    open_window_in_separate_window_or_switch_to_existing_internal(window_id);
}

void
open_window_in_separate_window_or_switch_to_existing_and_edit(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    log_verbose("open_window_in_separate_window_or_switch_to_existing_and_edit");
    WindowId window_id = static_cast<WindowId>(g_variant_get_int64(parameter));
    open_window_in_separate_window_or_switch_to_existing_internal(window_id, [window_id](MainWindow* new_win){
        log_verbose("win callback: window_id=%lld new_win=%p", window_id.value, new_win);
        edit_window_group_internal(window_id, new_win, false);
    });
}

void
switch_to_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    TabId tab_id = static_cast<TabId>(g_variant_get_int64(parameter));
    switch_to_tab_internal(win, tab_id);
}

// This action is special because it expects the tab ID to be passed via user_data
// instead of the parameter. This is needed to work around limitations of stateful
// checked menu item actions in GTK that have to also be parameterized by a variant
// (for example int64) value.
void
switch_to_tab_in_any_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    TabId tab_id = static_cast<TabId>(void_ptr_to_int64(user_data));
    global_core_api->get_tabs({tab_id}, [tab_id](GetTabsReply reply){
        VERIFY_LOG(reply.err);

        if (reply.tabs.empty()) {
            log_warning("switch_to_tab_in_any_window: no such tab with id %lld", tab_id.value);
            return;
        }

        TabDto tab = reply.tabs.at(0);
        WindowId window_id = tab.window_id;

        open_window_in_separate_window_or_switch_to_existing_internal(window_id, [tab_id](MainWindow* new_win){
            switch_to_tab_internal(new_win, tab_id);
        });
    });
}

//-------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------

void
reading_list_entry_delete(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    global_core_api->delete_reading_list_entries({orion::ReadingListEntryId(g_variant_get_int64(parameter))},
        [](orion::Error err){VERIFY_LOG(err);});
}

void
reading_list_entry_mark_as_read(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    orion::UpdateReadingListEntryQuery query{};
    query.id = orion::ReadingListEntryId(g_variant_get_int64(parameter));
    query.is_unread = false;
    query.update_is_unread = true;
    global_core_api->update_reading_list_entry(query, [](orion::Error err){VERIFY_LOG(err);});
}

void
reading_list_entry_mark_as_unread(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    orion::UpdateReadingListEntryQuery query{};
    query.id = orion::ReadingListEntryId(g_variant_get_int64(parameter));
    query.is_unread = true;
    query.update_is_unread = true;
    global_core_api->update_reading_list_entry(query, [](orion::Error err){VERIFY_LOG(err);});
}

//-------------------------------------------------------------------------
// Other
//-------------------------------------------------------------------------

void
profiles_button_style(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    ProfileId profile_id = main_window_get_current_profile_id(win);

    std::string str = g_variant_get_string(parameter, NULL);
    int64_t value{};
    VERIFY_LOG(orion::str::try_parse_int64(str, &value));

    orion::settings::ProfilesButtonStyle style = static_cast<orion::settings::ProfilesButtonStyle>(value);
    
    global_core_api->set_setting(
        profile_id,
        orion::settings::appearance::profiles_button_style,
        style,
        [](orion::SetSettingReply<orion::settings::ProfilesButtonStyle> reply){
            VERIFY_LOG(reply.err);
        });
}

static bool
try_close_bookmarks_dialog(MainWindow* win)
{   
    std::vector<GtkWidget*> dialogs = orion::gtk::get_descendants_by_name(
        GTK_WIDGET(win), "bookmarks-dialog"
    );

    bool found = false;

    for (GtkWidget* dialog : dialogs) {
        if (!BOOKMARKS_IS_DIALOG(dialog)) {
            continue;
        }
        adw_dialog_close(ADW_DIALOG(dialog));
        found = true;
    }
    return found;
}

void
install_from_firefox_store(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    orion_webview_tab_view_new_tab_with_uri(view, "https://addons.mozilla.org/en-US/firefox/");
    try_close_bookmarks_dialog(win);
}

void
install_from_chrome_store(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    orion_webview_tab_view_new_tab_with_uri(view, "https://chromewebstore.google.com/");
    try_close_bookmarks_dialog(win);
}

void
edit_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //TODO: figure out whether there are different needs for this action elsewhere
    GtkListItem* listitem = GTK_LIST_ITEM(main_window_get_last_right_clicked_widget(win));
    gpointer thing = g_object_ref(gtk_list_item_get_item(listitem));
    while((GTK_IS_TREE_LIST_ROW(thing)))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
    adw_dialog_present(edit_bookmark_dialog_new_for_id(bookmarks_columnview_item_get_id(item).bookmark_id), GTK_WIDGET(win));
}

void
edit_bookmark_folder(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //TODO: figure out whether there are different needs for this action elsewhere
    GtkListItem* listitem = GTK_LIST_ITEM(main_window_get_last_right_clicked_widget(win));
    gpointer thing = g_object_ref(gtk_list_item_get_item(listitem));
    while((GTK_IS_TREE_LIST_ROW(thing)))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
    AdwDialog* dialog = bookmarks_dialog_new();
    bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(dialog), 1);
    bookmarks_dialog_set_selected_bookmark_folder(BOOKMARKS_DIALOG(dialog), bookmarks_columnview_item_get_id(item).folder_id);
    adw_dialog_present(dialog, GTK_WIDGET(win));
}

void
open_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    const char* address = g_variant_get_string(parameter, NULL);
    main_window_load_bookmark(win, address);
}

void
add_bookmark(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    adw_dialog_present(add_bookmark_dialog_new_init_from_current_page(win), GTK_WIDGET(win));
}

void
add_bookmark_folder(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::CreateBookmarkFolderQuery query{};
    query.profile_id = main_window_get_current_profile_id(win);
    query.name = _("Untitled");
    global_core_api->create_bookmark_folder(query, [](orion::CreateBookmarkFolderReply reply){VERIFY_LOG(reply.err);});
    gtk_widget_activate_action(GTK_WIDGET(win), "sidebar_type", "s", "bookmarks");
    main_window_set_show_sidebar_setting(win, FALSE);
}

static void
import_passwords_from_browser_on_response(AdwAlertDialog* self, gchar* response, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    import_passwords_csv_file_internal(win, false /* show_passwords_button */);
}

void
import_passwords_from_browser(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    //adw_dialog_present(import_from_browser_dialog_new(), GTK_WIDGET(win));

    AdwDialog* dialog = adw_alert_dialog_new(
        _("Use Import From CSV file"),
        _("Export your passwords from the other browser as a CSV file"
            " and import them into Orion using Import From CSV file action.")
    );

    adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "ok", _("OK"));
    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "ok");
    adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "ok", ADW_RESPONSE_SUGGESTED);
    g_signal_connect(dialog, "response", G_CALLBACK(import_passwords_from_browser_on_response), win);
    adw_dialog_present(dialog, GTK_WIDGET(win));
}

void
import_passwords_from_extension(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    adw_dialog_present(import_from_extension_dialog_new(), GTK_WIDGET(win));
}

void
extension_permissions(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    if (!MAIN_IS_WINDOW(win)) {
        log_warning("extension_permissions: not main window");
    }

    settings_dialog_present(win, 8);
}

void
do_not_update_extension(GSimpleAction* action, GVariant* parameter, gpointer user_data)
{
}

void
copy_website(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    clipboard_copy(self, parameter, win);
}

void
copy_username(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    clipboard_copy(self, parameter, win);
}

void
copy_password(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    clipboard_copy(self, parameter, win);
}

void
edit_password(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    PasswordItemId id = PasswordItemId(g_variant_get_int64(parameter));
    WeakRefWrapper win_weak(win);

    global_core_api->get_password_items({id}, [id,win_weak](GetPasswordItemsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.items.empty()) {
            log_warning("edit_password: no such password item with id %lld", id.value);
            return;
        }
        PasswordItemDto item = reply.items.at(0);

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        EditPasswordDialog* dialog = APP_EDIT_PASSWORD_DIALOG(edit_password_dialog_new());

        edit_password_dialog_set_id(dialog, id);
        edit_password_dialog_set_url(dialog, item.url.c_str());
        edit_password_dialog_set_username(dialog, item.username.c_str());
        edit_password_dialog_set_password(dialog, item.password.c_str());
        edit_password_dialog_set_verification_key(dialog, item.verification_key.c_str());
        edit_password_dialog_set_verification_code(dialog, item.verification_code.c_str());

        adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(win));
    });
}

void
edit_search_engine(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    SearchEngineId id = SearchEngineId(g_variant_get_int64(parameter));
    WeakRefWrapper win_weak(MAIN_WINDOW(user_data));

    global_core_api->get_search_engines({id}, [id,win_weak](GetSearchEnginesReply reply){
        VERIFY_LOG(reply.err);
        if (reply.search_engines.empty()) {
            log_warning("edit_search_engine: no such search engine with id %lld", id.value);
            return;
        }
        const SearchEngineDto& engine = reply.search_engines.at(0);

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        AddSearchEngineDialog* dialog = ADD_SEARCH_ENGINE_DIALOG(add_search_engine_dialog_new(engine));
        adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(win));
    });
}

void
open_uri_in_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    gsize len;
    const gchar* uri_str = g_variant_get_string(parameter, &len);
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    auto uri = Uri::parse_strict(uri_str);
    orion_webview_tab_view_load_uri(view, uri);
}

void
open_uri_in_new_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    gsize len;
    const gchar* uri = g_variant_get_string(parameter, &len);
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    orion_webview_tab_view_new_tab_with_uri(view, uri);
}

void
open_uri_in_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    std::string uri = g_variant_get_string(parameter, NULL);
    open_uri_in_new_window_internal(uri, win);
}

void
open_uri_in_new_private_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    std::string uri = g_variant_get_string(parameter, NULL);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::profile::open_new_window_with_uri(profile_id, profile::FocusMode::no, orion::PrivateMode::yes, uri);
}

void
open_uri_in_preview_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    const char* uri_c_str;
    gdouble x, y;
    g_variant_get(parameter, "(&sdd)", &uri_c_str, &x, &y);

    if(!uri_c_str)
    {
        orion::log_error("got null string for opening in preview window");
        return;
    }

    std::string uri = uri_c_str;
    open_uri_in_preview_window_internal(uri, x, y, win);
}

std::string
insert_title_or_uri_at_placeholders(const gchar* original_string, const std::string& uri, const std::string& title)
{
    std::stringstream output_stream;
    for(const gchar* p = original_string; *p != '\0'; p++)
    {
        if(*p == '%')
        {
            switch(*(p+1))
            {
                case 'u':
                    output_stream << uri;
                    p++;
                    break;
                case 't':
                    output_stream << title;
                    p++;
                    break;
                default:
                    output_stream << "%";
                    break;
            }
        }
        else
        {
            output_stream << *p;
        }
    }
    return output_stream.str();
}

void
open_uri_with_placeholders_in_current_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    gsize len;
    const gchar* uri_str = g_variant_get_string(parameter, &len);

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    orion::Uri current_uri = orion_webview_tab_view_get_uri(view);
    std::string title = orion_webview_tab_view_get_title(view);

    auto uri = Uri::parse_strict(insert_title_or_uri_at_placeholders(uri_str, current_uri.uri(), title).c_str());
    orion_webview_tab_view_load_uri(view, uri);
}

void
open_uri_with_placeholders_in_new_tab(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    gsize len;
    const gchar* uri_str = g_variant_get_string(parameter, &len);

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    orion::Uri current_uri = orion_webview_tab_view_get_uri(view);
    std::string title = orion_webview_tab_view_get_title(view);
    std::string uri = insert_title_or_uri_at_placeholders(uri_str, current_uri.uri(), title);

    orion_webview_tab_view_new_tab_with_uri(view, uri.c_str(), {
        .focus_address_entry = false,
        .autoselect = true,
        .ignore_when_opening_setting = false,
    });
}

void
open_uri_with_placeholders_in_new_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    const gchar* uri_str = g_variant_get_string(parameter, NULL);

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    orion::Uri current_uri = orion_webview_tab_view_get_uri(view);
    std::string title = orion_webview_tab_view_get_title(view);

    open_uri_in_new_window_internal(insert_title_or_uri_at_placeholders(uri_str, current_uri.uri(), title), win);
}

void
open_uri_with_placeholders_in_background(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    const gchar* uri_str = g_variant_get_string(parameter, NULL);

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    orion::Uri current_uri = orion_webview_tab_view_get_uri(view);
    std::string title = orion_webview_tab_view_get_title(view);
    std::string uri = insert_title_or_uri_at_placeholders(uri_str, current_uri.uri(), title);

    orion_webview_tab_view_new_tab_with_uri(view, uri.c_str(), {
        .focus_address_entry = false,
        .autoselect = false,
        .ignore_when_opening_setting = true,
    });
}

void
open_uri_with_placeholders_in_preview_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    const char* uri_c_str;
    gdouble x, y;
    g_variant_get(parameter, "(&sdd)", &uri_c_str, &x, &y);
    if(!uri_c_str)
    {
        orion::log_error("got null string for opening in preview window");
        return;
    }

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    orion::Uri current_uri = orion_webview_tab_view_get_uri(view);
    std::string title = orion_webview_tab_view_get_title(view);
    std::string uri = insert_title_or_uri_at_placeholders(uri_c_str, current_uri.uri(), title);

    open_uri_in_preview_window_internal(uri, x, y, win);
}


void
clipboard_copy(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    GdkDisplay* display = gdk_display_get_default();
    GdkClipboard* clipboard = gdk_display_get_clipboard(display);
    gdk_clipboard_set_text(clipboard, g_variant_get_string(parameter, NULL));
}

void
reading_list_open_entry_new_tab_mark_read(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::UpdateReadingListEntryQuery query{};
    query.id = orion::ReadingListEntryId(g_variant_get_int64(parameter));
    query.is_unread = false;
    query.update_is_unread = true;
    global_core_api->update_reading_list_entry(query, [](orion::Error err){VERIFY_LOG(err);});

    std::vector<orion::ReadingListEntryId> vec{
    orion::ReadingListEntryId(g_variant_get_int64(parameter))
    };

    orion::gtk::WeakRefWrapper weak_win(win);
    global_core_api->get_reading_list_entries(vec, [weak_win](orion::GetReadingListEntriesReply reply){
        VERIFY_LOG(reply.err);
        if(reply.entries.size() != 1) return;

        auto ptr = weak_win.lock();
        if(!ptr) return;
        MainWindow* win = MAIN_WINDOW(ptr.get()); 

        orion::ReadingListEntryDto item = reply.entries.at(0);
        OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
        orion_webview_tab_view_new_tab_with_uri(view, item.address.c_str());

    });
}

void
reading_list_open_entry_new_window_mark_read(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::UpdateReadingListEntryQuery query{};
    query.id = orion::ReadingListEntryId(g_variant_get_int64(parameter));
    query.is_unread = false;
    query.update_is_unread = true;
    global_core_api->update_reading_list_entry(query, [](orion::Error err){VERIFY_LOG(err);});

    std::vector<orion::ReadingListEntryId> vec{
    orion::ReadingListEntryId(g_variant_get_int64(parameter))
    };

    orion::gtk::WeakRefWrapper weak_win(win);
    global_core_api->get_reading_list_entries(vec, [weak_win](orion::GetReadingListEntriesReply reply){
        VERIFY_LOG(reply.err);
        if(reply.entries.size() != 1) return;

        auto ptr = weak_win.lock();
        if(!ptr) return;
        MainWindow* win = MAIN_WINDOW(ptr.get()); 

        orion::ReadingListEntryDto item = reply.entries.at(0);

        orion::ProfileId profile_id = main_window_get_current_profile_id(win);

        // The reading list entry should always be opened in a new non-private window even
        // if the current window is private
        orion::CreateWindowQuery query{};
        query.icon_name = "window-flat-symbolic";
        query.profile_id = profile_id;
        query.is_tab_group = true; // this is never a saved window

        global_core_api->create_window(query, [address = std::move(item.address), profile_id](orion::CreateWindowReply reply){
            VERIFY_LOG(reply.err);
            orion::WindowId window_id = reply.id;
            orion::CreateTabQuery query{};
            query.uri = address;
            query.window_id = window_id;
            global_core_api->create_tab(query, [profile_id, window_id](orion::CreateTabReply reply){
                VERIFY_LOG(reply.err);
                MainWindow* new_win = main_window_new_from_backend(
                    g_application_get_default(), profile_id, window_id, orion::PrivateMode::no); // never private
                gtk_window_present(GTK_WINDOW(new_win));
            });
        });

    });
}

void
new_saved_window(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    // Saved Tab Group is never private
    orion::CreateWindowQuery query{
        .profile_id = main_window_get_current_profile_id(win),
        .template_name = _("Tab Group"),
        .icon_name = "window-flat-symbolic",
    };

    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->create_window(query, [win_weak](orion::CreateWindowReply reply){
        VERIFY_LOG(reply.err);

        orion::CreateTabQuery query{
            .window_id = reply.id,
            .name = _("Start Page"),
            .uri = "orion://newtab/",
        };

        global_core_api->create_tab(query, [window_id=reply.id,win_weak](orion::CreateTabReply reply){
            VERIFY_LOG(reply.err);

            // Call to orion_webview_tab_container_set_last_active_time should not be needed here
            orion::UpdateWindowQuery query{
                .id = window_id,
                .selected_tab_id = reply.id,
                .update_selected_tab_id = true,
            };

            global_core_api->update_window(query, [win_weak,window_id](orion::Error err){
                VERIFY_LOG(err);

                // no need to unsuspend the tab here because new tabs are never suspended

                auto win_ptr = win_weak.lock();
                if (!win_ptr) return;
                MainWindow* main_window = MAIN_WINDOW(win_ptr.get());

                main_window_switch_to_window_id_in_place(main_window, window_id);
                edit_window_group_internal(window_id, main_window, true);
            });
        });
    });
}

static void
switch_to_bookmark_by_ctrl_num(MainWindow* win, int num)
{
    ProfileId profile_id = main_window_get_current_profile_id(win);
    WeakRefWrapper win_weak(win);

    global_core_api->get_bookmark_folder_id_by_type(profile_id, BookmarkFolderType::bookmarks_bar, 
        [win_weak,num](orion::GetBookmarkFolderIdByTypeReply reply){
            VERIFY_LOG(reply.err);

            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            GetBookmarkIdsQuery query{
                .profile_id = main_window_get_current_profile_id(win),
                .parent_folder_id = reply.folder_id,
            };
            global_core_api->get_bookmark_ids(query, [num,win_weak](GetBookmarkIdsReply reply){
                VERIFY_LOG(reply.err);

                const std::vector<BookmarkId>& bookmark_ids = reply.ids;

                if (bookmark_ids.empty()) {
                    return;
                }

                size_t bookmark_index = static_cast<int64_t>(num) - 1;
                if (bookmark_index >= bookmark_ids.size()) {
                    bookmark_index = bookmark_ids.size() - 1;
                }

                BookmarkId bookmark_id = reply.ids.at(bookmark_index);

                GetBookmarksQuery query{
                    .ids = {bookmark_id},
                };

                global_core_api->get_bookmarks(query, [win_weak,bookmark_id](GetBookmarksReply reply){
                    VERIFY_LOG(reply.err);
                    if (reply.bookmarks.empty()) {
                        log_warning("switch_to_bookmark_by_ctrl_num: no such bookmark with id %lld", bookmark_id.value);
                        return;
                    }
                    auto win_strong = win_weak.lock();
                    if (!win_strong) return;
                    MainWindow* win = MAIN_WINDOW(win_strong.get());

                    BookmarkDto bookmark = reply.bookmarks.at(0);
                    main_window_load_bookmark(win, bookmark.address.c_str());
                });
            });
        });
}

static void
switch_to_tab_by_ctrl_num(MainWindow* win, int num)
{
    WindowId window_id = main_window_get_window_id(win);
    GetTabIdsQuery query{
        .window_id = window_id,
    };
    WeakRefWrapper win_weak(win);

    global_core_api->get_tab_ids(query, [num,win_weak](GetTabIdsReply reply){
        VERIFY_LOG(reply.err);

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        const std::vector<TabId>& tab_ids = reply.ids;

        if (tab_ids.empty()) {
            return;
        }

        size_t tab_index = static_cast<int64_t>(num) - 1;
        if (tab_index >= tab_ids.size()) {
            tab_index = tab_ids.size() - 1;
        }

        TabId tab_id = reply.ids.at(tab_index);
        switch_to_tab_internal(win, tab_id);
    });
}

static void
handle_ctrl_num_internal(MainWindow* win, int num)
{
    ProfileId profile_id = main_window_get_current_profile_id(win);
    WeakRefWrapper win_weak(win);

    // check use_ctrl_num_to_switch_tabs setting and act accordingly
    global_core_api->get_setting(
        profile_id,
        orion::settings::tabs::use_ctrl_1_to_9_to_switch_tabs,
        [win_weak,num](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);

            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            if (reply.value) {
                switch_to_tab_by_ctrl_num(win, num);
            } else {
                switch_to_bookmark_by_ctrl_num(win, num);
            }
        });
}

void
handle_ctrl_num_1(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    handle_ctrl_num_internal(win, 1);
}

void
handle_ctrl_num_2(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    handle_ctrl_num_internal(win, 2);
}

void
handle_ctrl_num_3(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    handle_ctrl_num_internal(win, 3);
}

void
handle_ctrl_num_4(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    handle_ctrl_num_internal(win, 4);
}

void
handle_ctrl_num_5(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    handle_ctrl_num_internal(win, 5);
}

void
handle_ctrl_num_6(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    handle_ctrl_num_internal(win, 6);
}

void
handle_ctrl_num_7(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    handle_ctrl_num_internal(win, 7);
}

void
handle_ctrl_num_8(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    handle_ctrl_num_internal(win, 8);
}

void
handle_ctrl_num_9(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    // this should always switch to the last item (tab or bookmark) - use 0
    // which will underflow to SIZE_MAX in a controlled manner
    handle_ctrl_num_internal(win, 0);
}

void
select_address_entry(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    HeaderBar* header_bar = main_window_get_header_bar(win);
    GtkEntry* address_entry = header_bar_get_address_entry(header_bar);
    if(address_entry)
    {
        gtk_widget_grab_focus(GTK_WIDGET(address_entry));
    }
}

void
open_start_page(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    open_uri_in_current_tab(self, g_variant_new_string("orion://newtab/"), user_data);
}

void
show_page_tweaks(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    adw_dialog_present(page_tweaks_popover_new(), GTK_WIDGET(win));
}

void
show_privacy(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_show_privacy_popover(win);
}

void
show_kagi_privacy_pass(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    // TODO:
}

void
programmable_button_action(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);

    int64_t id_val = g_variant_get_int64(parameter);

    main_window_activate_programmable_button_action(win, orion::ProgrammableButtonId(id_val));
}

void
run_javascript_code(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    AdwTabPage* page = orion_webview_tab_view_get_selected_page(view);
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
    if(!content) return;

    GVariant* title_val = g_variant_get_child_value(parameter, 0);
    GVariant* script_val = g_variant_get_child_value(parameter, 1);
    GVariant* auto_open_sidebar_val = g_variant_get_child_value(parameter, 2);

    defer([title_val]{ g_variant_unref(title_val); });
    defer([script_val]{ g_variant_unref(script_val); });
    defer([auto_open_sidebar_val]{ g_variant_unref(auto_open_sidebar_val); });

    gsize script_len = 0;
    const gchar* script_c_str = g_variant_get_string(script_val, &script_len);

    gsize title_len = 0;
    const gchar* title_c_str = g_variant_get_string(title_val, &title_len);


    if(script_len)
    {
        std::string_view script_string(script_c_str, script_len);

        AdwOverlaySplitView* split_view = main_window_get_js_sidebar_split_view(MAIN_WINDOW(win));
        JsSidebar* js_sidebar = APP_JS_SIDEBAR(adw_overlay_split_view_get_sidebar(split_view));

        if(g_variant_get_boolean(auto_open_sidebar_val))
            adw_overlay_split_view_set_show_sidebar(split_view, TRUE);

        //TODO: title
        std::string script_with_epoch = orion::gtk::get_orion_internals_script(
            script_string,
            js_sidebar_start_operation(js_sidebar, title_c_str)
        );

        //we need to wrap this with a uuid setter
        orion_webview_tab_content_evaluate_javascript(
            content,
            script_with_epoch.c_str(),
            script_with_epoch.length(),
            PROGRAMMABLE_BUTTON_SCRIPT_WORLD,
            NULL,
            NULL,
            NULL,
            NULL);
    }
}

#ifndef NDEBUG
void
test_action(GSimpleAction* self, GVariant* parameter, gpointer user_data)
{
}
#endif


} // namespace orion::gtk::main_window
