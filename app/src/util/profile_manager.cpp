#define ENABLE_VERBOSE_LOGS 0

#include "profile_manager.h"
#include "app.h"
#include "window/main_window/main_window.h"
#include <core/settings.h>
#include <common/src/string_util.h>
#include <common/src/log.h>
#include <glib/gi18n.h>
#include <format>
#include "global_core_api.h"
#include "util/private_windows.h"

// Use fully qualified names to avoid conflicts with orion::profile namespace
using ::orion::settings::AppOpensWith;
using ::orion::GetSettingReply;

namespace orion {
namespace profile {

namespace {
    // Internal state for profile management
    struct ProfileState {
        bool is_quitting = false;
        bool preserve_windows_on_quit = false;
        orion::settings::AppOpensWith app_opens_with = orion::settings::AppOpensWith::not_set;
        orion::ObserverId windows_change_observer_id{};
    };
    
    std::map<orion::ProfileId, ProfileState> profile_states;

    // Clean up backend state when not restoring session:
    // - Delete unsaved tab groups (both private and non-private)
    // - Reset has_ui_window flag for saved windows
    static void
    cleanup_backend_windows(orion::ProfileId profile_id, AppOpensWith app_opens_with, std::function<void()> done_cb)
    {
        global_core_api->get_window_ids({.profile_id=profile_id},
        [profile_id,app_opens_with,done_cb](orion::GetWindowIdsReply reply){
            VERIFY_LOG(reply.err);
            
            if (reply.ids.empty()) {
                done_cb();
                return;
            }
            
            global_core_api->get_windows(reply.ids, [profile_id,app_opens_with,done_cb](orion::GetWindowsReply reply){
                VERIFY_LOG(reply.err);
                
                std::vector<orion::WindowId> ids_to_delete;
                std::vector<orion::WindowId> ids_to_update;
                
                for (const orion::WindowDto& window : reply.windows) {
                    if (app_opens_with == AppOpensWith::new_window
                     || app_opens_with == AppOpensWith::new_private_window
                     || app_opens_with == AppOpensWith::not_set) {

                        if (window.is_tab_group) {
                            // Delete unsaved tab groups
                            ids_to_delete.push_back(window.id);
                        } else if (window.has_ui_window) {
                            // Reset has_ui_window flag for saved windows
                            ids_to_update.push_back(window.id);
                        }
                    } else if (app_opens_with == AppOpensWith::all_non_private_windows_from_last_session) {
                        if (window.is_private) {
                            // Delete private windows that must not be restored
                            ids_to_delete.push_back(window.id);
                        } else if (!window.is_tab_group && window.has_ui_window) {
                            // Reset has_ui_window flag for saved non-private windows
                            ids_to_update.push_back(window.id);
                        }
                    } else {
                        // No cleanup needed for other app_opens_with values
                    }
                }
                
                std::shared_ptr<size_t> pending_ops = std::make_shared<size_t>(0);
                if (!ids_to_delete.empty()) (*pending_ops)++;
                if (!ids_to_update.empty()) (*pending_ops)++;
                
                if (*pending_ops == 0) {
                    done_cb();
                    return;
                }
                
                auto check_done = [pending_ops, done_cb](){
                    if ((--(*pending_ops)) == 0) {
                        done_cb();
                    }
                };
                
                if (!ids_to_delete.empty()) {
                    orion::log_debug(
                        "cleanup_backend_windows: deleting %zu unsaved tab groups for profile_id=%lld",
                        ids_to_delete.size(), profile_id.value
                    );
                    global_core_api->delete_windows(ids_to_delete, [check_done](orion::Error err){
                        VERIFY_LOG(err);
                        check_done();
                    });
                }
                
                if (!ids_to_update.empty()) {
                    orion::log_debug(
                        "cleanup_backend_windows: resetting has_ui_window for %zu saved windows for profile_id=%lld",
                        ids_to_update.size(), profile_id.value
                    );
                    
                    std::shared_ptr<size_t> pending_updates = std::make_shared<size_t>(ids_to_update.size());
                    for (orion::WindowId window_id : ids_to_update) {
                        orion::UpdateWindowQuery query{
                            .id = window_id,
                            .has_ui_window = false,
                            .update_has_ui_window = true,
                        };
                        global_core_api->update_window(query, [pending_updates, check_done](orion::Error err){
                            VERIFY_LOG(err);
                            if ((--(*pending_updates)) == 0) {
                                check_done();
                            }
                        });
                    }
                }
            });
        });
    }
}

static bool
app_opens_with_restored_session(AppOpensWith app_opens_with)
{
    switch (app_opens_with) {
        case AppOpensWith::all_windows_from_last_session:
        case AppOpensWith::all_non_private_windows_from_last_session:
            return true;
        case AppOpensWith::new_window:
        case AppOpensWith::new_private_window:
        case AppOpensWith::not_set:
        default:
            return false;
    }
}

void
open_window(::orion::ProfileId profile_id, ::orion::WindowId window_id, PrivateMode private_mode)
{
    GApplication* app = g_application_get_default();
    if (!app) return;

    MainWindow* win = main_window_new(app, profile_id, window_id, private_mode);
    gtk_window_present(GTK_WINDOW(win));
}

void
find_unique_window_name(ProfileId profile_id, PrivateMode private_mode, std::function<void(std::string)> cb)
{
    global_core_api->get_window_ids({.profile_id=profile_id},
    [profile_id,cb=std::move(cb),private_mode](GetWindowIdsReply reply){
        VERIFY_LOG(reply.err);

        global_core_api->get_windows(reply.ids,
            [profile_id,cb=std::move(cb),private_mode](GetWindowsReply reply){

            VERIFY_LOG(reply.err);

            std::string prefix = (private_mode == PrivateMode::yes) ? _("Private Window") : _("Window");
            
            std::regex pattern(std::format(R"(^{} (\d+)$)", orion::str::escape_regex(prefix)));
            int64_t max_number = 0;

            for (const auto& window : reply.windows) {
                std::smatch match;
                if (std::regex_match(window.name, match, pattern)) {
                    int64_t number{};
                    VERIFY_LOG(orion::str::try_parse_int64(match[1].str(), &number));
                    max_number = std::max(max_number, number);
                }
            }

            std::string unique_name = prefix + " " + std::to_string(max_number + 1);
            cb(unique_name);
        });
    });
}

void
open_new_window_with_uri(
    orion::ProfileId profile_id, FocusMode focus_mode, PrivateMode private_mode, const std::string& uri)
{
    // Find a unique window name - relevant only for private windows
    find_unique_window_name(profile_id, PrivateMode::yes,
    [profile_id,focus_mode,private_mode,uri](std::string unique_private_name){

        orion::CreateWindowQuery query{
            .profile_id = profile_id,
            .is_tab_group = true,
            .is_focus_mode = (focus_mode == FocusMode::yes),
        };

        if (private_mode == PrivateMode::no) {
            query.is_private = false;
            query.icon_name = "window-flat-symbolic";
            query.name = ""; // non-private (regular) tab group has always an empty name
        } else {
            query.is_private = true;
            query.icon_name = "hand-openyay-symbolic";
            query.name = unique_private_name;
        }

        global_core_api->create_window(query, [profile_id,private_mode,uri](orion::CreateWindowReply reply){
            VERIFY_LOG(reply.err);
            orion::WindowId window_id = reply.id;

            orion::CreateTabQuery query{
                .window_id = reply.id,
                .uri = uri,
            };

            if (uri == "" || uri == "orion://newtab/") {
                query.name = _("Start Page");
            }

            global_core_api->create_tab(query, [profile_id,window_id,private_mode](orion::CreateTabReply reply){
                VERIFY_LOG(reply.err);
                
                open_window(profile_id, window_id, private_mode);
            });
        });
    });
}

void
open_new_window(orion::ProfileId profile_id, FocusMode focus_mode, PrivateMode private_mode)
{
    open_new_window_with_uri(profile_id, focus_mode, private_mode, "orion://newtab/");
}

static void
unregister_window_change_observer(orion::ProfileId profile_id)
{
    if (!profile_states[profile_id].windows_change_observer_id.value) {
        // Already unregistered or never registered, nothing to do
        return;
    }

    orion::log_verbose(
        "unregister_window_change_observer: profile_id=%lld observer_id=%lld",
        profile_id.value, profile_states[profile_id].windows_change_observer_id.value
    );

    global_core_api->windows_change().clear_observer(&profile_states[profile_id].windows_change_observer_id);
}

static void
register_window_change_observer(orion::ProfileId profile_id)
{
    if (profile_states[profile_id].windows_change_observer_id.value) {
        // Already registered, nothing to do
        return;
    }

    profile_states[profile_id].windows_change_observer_id = global_core_api->windows_change().add_observer(
    [profile_id](const orion::WindowsChange& change){
        // Try to find at least one deleted window for this profile
        bool is_private_deleted = false;
        bool is_regular_deleted = false;

        for (const auto& deleted : change.deleted) {
            if (deleted.profile_id == profile_id) {
                if (deleted.is_private) {
                    is_private_deleted = true;
                } else {    
                    is_regular_deleted = true;
                }
            }
        }

        if (!is_private_deleted && !is_regular_deleted) return;

        global_core_api->get_windows_by_query(profile_id,
        [profile_id,is_private_deleted,is_regular_deleted](orion::GetWindowsReply reply){
            VERIFY_LOG(reply.err);

            if (is_private_deleted) {
                bool has_private_window = false;

                for (const auto& window : reply.windows) {
                    if (window.is_private) {
                        has_private_window = true;
                        break;
                    }
                }

                if (!has_private_window) {
                    OrionApp* app = ORION_APP(g_application_get_default());
                    orion::gtk::WeakRefWrapper app_weak(app);
                    
                    orion_app_wait_for_profile_async_ops(app, profile_id, {orion::PrivateMode::yes},
                    [profile_id,app_weak]{
                        auto app_strong = app_weak.lock();
                        if (!app_strong) return;
                        OrionApp* app = ORION_APP(app_strong.get());

                        orion_app_cleanup_profile_resources(
                            app, profile_id, {orion::PrivateMode::yes}
                        );

                        // Clean up any remaining private data for this profile since there are no more private windows
                        orion::log_debug(
                            "profile::window_change: no more private windows for profile_id=%lld, cleaning up private data",
                            profile_id.value
                        );
                        
                        cleanup_private_data(profile_id, [](orion::Error err){
                            VERIFY_LOG(err);
                        });
                    });
                }
            }

            if (is_regular_deleted) {
                bool has_regular_window = false;

                for (const auto& window : reply.windows) {
                    if (!window.is_private) {
                        has_regular_window = true;
                        break;
                    }
                }

                if (!has_regular_window) {
                    OrionApp* app = ORION_APP(g_application_get_default());
                    orion::gtk::WeakRefWrapper app_weak(app);
                    
                    orion_app_wait_for_profile_async_ops(app, profile_id, {orion::PrivateMode::no},
                    [profile_id,app_weak]{
                        auto app_strong = app_weak.lock();
                        if (!app_strong) return;
                        OrionApp* app = ORION_APP(app_strong.get());

                        orion_app_cleanup_profile_resources(
                            app, profile_id, {orion::PrivateMode::no}
                        );
                    });
                }
            }
        });
    });
    
    orion::log_verbose(
        "register_window_change_observer: profile_id=%lld observer_id=%lld",
        profile_id.value, profile_states[profile_id].windows_change_observer_id.value
    );
}

// Cleanup all private windows on startup and quit if app_opens_with is one of:
// - all_non_private_windows_from_last_session
// - new_window
// - new_private_window.
static void
open_impl(orion::ProfileId profile_id, bool from_snapshot)
{
    register_window_change_observer(profile_id);

    orion::gtk::WeakRefWrapper app_weak(g_application_get_default());

    global_core_api->get_setting(profile_id, ::orion::settings::general::app_opens_with,
    [app_weak, profile_id, from_snapshot](GetSettingReply<AppOpensWith> setting_reply){
        VERIFY_LOG(setting_reply.err);

        AppOpensWith opens_with = setting_reply.value;

        if (!app_opens_with_restored_session(opens_with) && !from_snapshot) {
            // Clean up backend: delete unsaved tab groups (both private and non-private)
            // and reset has_ui_window flags
            cleanup_backend_windows(profile_id, opens_with, [profile_id,opens_with]{
                PrivateMode private_mode = PrivateMode::no;
                if (opens_with == AppOpensWith::new_private_window) {
                    private_mode = PrivateMode::yes;
                }
                open_new_window(profile_id, FocusMode::no, private_mode);
            });
            return;
        }

        global_core_api->get_windows_by_query(profile_id,
        [app_weak,profile_id,opens_with](orion::GetWindowsReply reply){
            VERIFY_LOG(reply.err);

            const auto& windows = reply.windows;

            std::vector<orion::WindowId> delete_window_ids{};

            if (opens_with == AppOpensWith::all_non_private_windows_from_last_session) {
                // Cleanup remaining private windows that must not be restored
                for (const auto& window : windows) {
                    if (window.is_private) {
                        delete_window_ids.push_back(window.id);
                    }
                }
            }

            // Do not capture a (stale) copy of windows vector here, perform another get query instead
            // to get a fresh list of windows (some of them might have been deleted). The same is true
            // for selected window.
            auto perform = [app_weak,profile_id,opens_with]{
                global_core_api->get_windows_by_query(profile_id,
                [app_weak,profile_id,opens_with](orion::GetWindowsReply reply){
                    VERIFY_LOG(reply.err);
                    const auto& windows = reply.windows;
                    
                    global_core_api->get_selected_window_id(profile_id,
                    [app_weak,profile_id,opens_with,windows](orion::GetSelectedWindowIdReply reply){
                        VERIFY_LOG(reply.err);

                        orion::GetSelectedWindowIdReply selected_window = reply;

                        auto app_strong = app_weak.lock();
                        if (!app_strong) return;
                        GApplication* app = G_APPLICATION(app_strong.get());
                        
                        // Find and show all windows that have a UI window
                        std::vector<orion::WindowDto> owning_windows;
                        std::copy_if(
                            windows.begin(), windows.end(),
                            std::back_inserter(owning_windows),
                            [](const orion::WindowDto& window){ return window.has_ui_window; }
                        );

                        if (selected_window.window_id.value) {
                            // Reorder owning windows so that the selected window is the first one
                            std::stable_partition(
                                owning_windows.begin(), owning_windows.end(),
                                [selected_window](const orion::WindowDto& window){
                                    return window.id == selected_window.window_id;
                                }
                            );
                        }

                        if (!owning_windows.empty()) {
                            // Show all windows that should have a UI window
                            for (const orion::WindowDto& window : owning_windows) {
                                orion::PrivateMode private_mode = window.is_private
                                    ? orion::PrivateMode::yes
                                    : orion::PrivateMode::no;
                                MainWindow* win = main_window_new_from_backend(app, profile_id, window.id, private_mode);
                                gtk_window_present(GTK_WINDOW(win));
                            }
                        } else {
                            // No windows with a UI window
                            if (selected_window.window_id.value && selected_window.window_exists) {
                                orion::WindowId window_id = selected_window.window_id;
                                orion::PrivateMode private_mode = selected_window.is_private
                                    ? orion::PrivateMode::yes
                                    : orion::PrivateMode::no;
                                MainWindow* win = main_window_new_from_backend(app, profile_id, window_id, private_mode);
                                gtk_window_present(GTK_WINDOW(win));
                            } else {
                                // No selected window or the selected window does not exist
                                PrivateMode private_mode = PrivateMode::no;
                                if (opens_with == AppOpensWith::new_private_window) {
                                    private_mode = PrivateMode::yes;
                                }
                                open_new_window(profile_id, FocusMode::no, private_mode);
                            }
                        }
                    });
                });
            };

            if (delete_window_ids.empty()) {
                perform();
            } else {
                global_core_api->delete_windows(delete_window_ids, [perform](orion::Error err){
                    VERIFY_LOG(err);
                    perform();
                });
            }
        });
    });
}

void
open(orion::ProfileId profile_id)
{
    orion::log_verbose("profile::open: profile_id=%lld", profile_id.value);
    open_impl(profile_id, false);
}

void
open_from_snapshot(orion::ProfileId profile_id)
{
    orion::log_verbose("profile::open_from_snapshot: profile_id=%lld", profile_id.value);
    open_impl(profile_id, true);
}

void
close(orion::ProfileId profile_id)
{
    unregister_window_change_observer(profile_id);
}

void
close_all()
{
    for (const auto& [profile_id, state] : profile_states) {
        close(profile_id);
    }
}

static void
remove_profile_state(orion::ProfileId profile_id)
{
    close(profile_id);
    profile_states.erase(profile_id);
}

void
quit(orion::ProfileId profile_id)
{
    OrionApp* app = ORION_APP(g_application_get_default());
    
    if (app && orion_app_is_quitting(app)) {
        orion::log_info("profile::quit: app is already quitting, ignoring profile quit request for profile_id=%lld", profile_id.value);
        return;
    }

    profile_states[profile_id].is_quitting = true;

    orion::gtk::WeakRefWrapper app_weak(app);

    global_core_api->get_setting(profile_id, ::orion::settings::general::app_opens_with, 
    [profile_id,app_weak](GetSettingReply<AppOpensWith> reply){
        VERIFY_LOG(reply.err);
        bool preserve_windows = app_opens_with_restored_session(reply.value);
        AppOpensWith app_opens_with = reply.value;
        
        auto app_strong = app_weak.lock();
        if (!app_strong) return;
        
        profile_states[profile_id].preserve_windows_on_quit = preserve_windows;
        profile_states[profile_id].app_opens_with = app_opens_with;
        
        orion::log_verbose(
            "profile::quit: profile_id=%lld preserve_windows=%d app_opens_with=%d",
            profile_id.value, preserve_windows, static_cast<int>(app_opens_with)
        );
        
        // Get all windows for this profile and close their UI windows
        global_core_api->get_window_ids({.profile_id=profile_id},
        [profile_id,app_weak,app_opens_with](orion::GetWindowIdsReply reply){
            VERIFY_LOG(reply.err);
            
            if (reply.ids.empty()) {
                orion::log_debug("profile::quit: no windows for profile_id=%lld", profile_id.value);
                // Still need to clean up resources even if there are no windows
                auto cleanup_cb = [profile_id,app_weak]{
                    // Always cleanup private data on app quit
                    orion::profile::cleanup_private_data(profile_id, [profile_id,app_weak](orion::Error err){
                        VERIFY_LOG(err);

                        auto app_strong = app_weak.lock();
                        if (!app_strong) return;
                        OrionApp* app = ORION_APP(app_strong.get());

                        // Clean up profile-specific resources
                        orion_app_cleanup_profile_resources(
                            app, profile_id, {orion::PrivateMode::yes, orion::PrivateMode::no}
                        );

                        // Remove profile state - quitting is complete
                        remove_profile_state(profile_id);
                    });
                };

                auto app_strong = app_weak.lock();
                if (!app_strong) return;
                OrionApp* app = ORION_APP(app_strong.get());
                
                // Wait for async operations to complete
                orion_app_wait_for_profile_async_ops(
                    app, profile_id, {orion::PrivateMode::yes, orion::PrivateMode::no}, cleanup_cb
                );
                return;
            }
            
            global_core_api->get_windows(reply.ids,
            [profile_id,app_weak,app_opens_with](orion::GetWindowsReply reply){
                VERIFY_LOG(reply.err);
                
                auto app_strong = app_weak.lock();
                if (!app_strong) return;
                OrionApp* app = ORION_APP(app_strong.get());
                
                // Close UI windows for this profile
                for (const orion::WindowDto& window : reply.windows) {
                    // Get UI windows associated with this backend window
                    std::vector<orion::UiWindowId> ui_ids = 
                        global_core_api->get_ui_windows_by_window_id(window.id);
                    
                    for (orion::UiWindowId ui_id : ui_ids) {
                        MainWindow* win = MAIN_WINDOW(ui_id.value);
                        if (win && main_window_get_current_profile_id(win) == profile_id) {
                            orion::log_verbose(
                                "profile::quit: closing UI window for profile_id=%lld window_id=%lld private=%d",
                                profile_id.value, window.id.value, main_window_get_private_mode(win)
                            );
                            gtk_window_close(GTK_WINDOW(win));
                        }
                    }
                }
                
                auto quit_cb = [profile_id,app_weak,app_opens_with](orion::ErrorFn done_cb){
                    // Define cleanup callback
                    auto cleanup_cb = [profile_id,app_weak,done_cb]{
                        auto app_strong = app_weak.lock();
                        if (!app_strong) return;
                        OrionApp* app = ORION_APP(app_strong.get());
                        
                        // Wait for async operations and clean up resources
                        orion_app_wait_for_profile_async_ops(
                        app, profile_id, {orion::PrivateMode::yes, orion::PrivateMode::no},
                        [profile_id,app_weak,done_cb=std::move(done_cb)]{
                            // Always cleanup private data on app quit
                            orion::profile::cleanup_private_data(profile_id,
                            [profile_id,app_weak,done_cb=std::move(done_cb)](orion::Error err){
                                VERIFY_LOG(err);

                                auto app_strong = app_weak.lock();
                                if (!app_strong) return;
                                OrionApp* app = ORION_APP(app_strong.get());
                                
                                orion_app_cleanup_profile_resources(
                                    app, profile_id, {orion::PrivateMode::yes, orion::PrivateMode::no}
                                );
                                
                                // Remove profile state - quitting is complete
                                remove_profile_state(profile_id);
                                
                                done_cb(orion::Error::ok());
                            });
                        });
                    };

                    // Clean up backend windows if not preserving
                    cleanup_backend_windows(profile_id, app_opens_with, cleanup_cb);
                };

                // We must register this quit callback in app as soon as possible, otherwise a reply callback
                // of some pending async operation might never by called because orion_app_quit_internal was
                // already called and the run loop was requested to stop.
                orion_app_register_quit_callback(app, {"profile::quit", quit_cb});
            });
        });
    });
}

bool
has_windows_for_profile(orion::ProfileId profile_id)
{
    // Check if all windows for this profile have been finalized
    std::vector<orion::UiWindowId> all_ui_ids = global_core_api->get_registered_ui_window_ids();
    bool has_windows = false;
    
    for (orion::UiWindowId ui_id : all_ui_ids) {
        MainWindow* win = MAIN_WINDOW(ui_id.value);
        if (!win) continue;

        orion::ProfileId win_profile_id = main_window_get_current_profile_id(win);

        if (win_profile_id == profile_id) {
            has_windows = true;
            break;
        }
    }
    return has_windows;
}

bool
has_other_private_windows_for_profile(orion::ProfileId profile_id, orion::WindowId exclude_window_id)
{
    std::vector<orion::UiWindowId> all_ui_ids = global_core_api->get_registered_ui_window_ids();
    bool has_other_private_windows = false;
    
    for (orion::UiWindowId ui_id : all_ui_ids) {
        MainWindow* win = MAIN_WINDOW(ui_id.value);
        if (!win) continue;

        orion::ProfileId win_profile_id = main_window_get_current_profile_id(win);
        orion::WindowId win_window_id = main_window_get_window_id(win);
        bool is_private = main_window_get_private_mode(win) == PrivateMode::yes;

        if (win_profile_id == profile_id && win_window_id != exclude_window_id && is_private) {
            has_other_private_windows = true;
            break;
        }
    }
    return has_other_private_windows;
}

// Should be called before CoreApi::unregister_ui_window is called. This is called from main_window_finalize just
// before the main window is destroyed. This means that we cannot (asynchronously) call certain functions that
// require the main window to still exist.
void
on_window_finalized(orion::ProfileId profile_id, orion::WindowId window_id, bool has_windows_for_profile)
{
    OrionApp* app = ORION_APP(g_application_get_default());
    if (!app) return;
    
    orion::log_verbose(
        "profile::on_window_finalized: profile_id=%lld window_id=%lld has_windows_for_profile=%d",
        profile_id.value, window_id.value, has_windows_for_profile
    );
    
    if (!has_windows_for_profile) {
        orion::log_verbose(
            "profile::on_window_finalized: all windows for profile_id=%lld finalized",
            profile_id.value
        );
        
        // If the profile is not already in the process of quitting, auto-quit it
        if (!profile_states[profile_id].is_quitting) {
            orion::log_info(
                "profile::on_window_finalized: auto-quitting profile_id=%lld (last window closed)",
                profile_id.value
            );
            quit(profile_id);
        }
        // If already quitting, do nothing - quit() will clean up when done
    }
}

bool
should_preserve_windows_on_quit(orion::ProfileId profile_id)
{
    auto it = profile_states.find(profile_id);
    if (it != profile_states.end()) {
        return it->second.preserve_windows_on_quit;
    }
    return false;
}

orion::settings::AppOpensWith
get_app_opens_with(orion::ProfileId profile_id)
{
    auto it = profile_states.find(profile_id);
    if (it != profile_states.end()) {
        return it->second.app_opens_with;
    }
    return orion::settings::AppOpensWith::not_set;
}

} // namespace profile
} // namespace orion
