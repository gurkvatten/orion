#define ENABLE_VERBOSE_LOGS 0

#include "util/tab_context_menu.h"
#include "util/ref_util.h"
#include "util/menu_util.h"
#include "util/text_util.h"

#include <core/core_api.h>
#include <core/settings.h>

#include "window/orion_webview_tabview_widgets/orion_webview_tab_container.h"

namespace orion::gtk {

// returns true if error happened and was handled
static bool
handle_error(Error err, Cb<Error> cb)
{
    if (err) {
        if (cb) {
            cb(err);
        }
        return true;
    }
    return false;
}

void
append_move_tab_submenu(MainWindow* win, TabId tab_id, GMenu* menu, TabType tab_type)
{
    ProfileId profile_id = main_window_get_current_profile_id(win);

    GMenu* move_tab_submenu = g_menu_new();
    defer([move_tab_submenu]{ g_object_unref(move_tab_submenu); });

    WindowId current_window_id = main_window_get_window_id(win);

    // Hack to avoid submenu name collisions in GMenu - this is needed specifically
    // because of AdwTabView menu model handling. This has to be a static variable
    // to avoid collisions.
    static int submenu_counter = 0;
    submenu_counter++;

    WeakRefWrapper move_tab_submenu_weak(move_tab_submenu);

    // we do not need to grab strong pointer to move_tab_submenu here because
    // it is reffed by g_menu_append_section calls below
    global_core_api->get_window_ids({.profile_id=profile_id},
        [move_tab_submenu_weak,current_window_id,tab_id,profile_id,tab_type]
        (GetWindowIdsReply reply){
            VERIFY_LOG(reply.err);
            global_core_api->get_windows(reply.ids,
                [move_tab_submenu_weak,current_window_id,tab_id,profile_id,tab_type]
                (GetWindowsReply reply){
                    VERIFY_LOG(reply.err);

                    auto move_tab_submenu_strong = move_tab_submenu_weak.lock();
                    if (!move_tab_submenu_strong) return;
                    GMenu* move_tab_submenu = G_MENU(move_tab_submenu_strong.get());

                    GMenu* tab_group_section = g_menu_new();
                    defer([tab_group_section]{ g_object_unref(tab_group_section); });

                    GMenu* saved_window_section = g_menu_new();
                    defer([saved_window_section]{ g_object_unref(saved_window_section); });

                    for(auto window : reply.windows)
                    {
                        if(window.id == current_window_id || window.profile_id != profile_id) continue;

                        std::string label;
                        if (window.is_tab_group && !window.is_private) {
                            label = orion::text::ngettext_string("%" PRId64 " Tab", "%" PRId64 " Tabs", window.num_tabs);
                        } else {
                            // Saved Tab Group or Private Tab Group
                            label = orion::text::ellipsize_string(window.name, MAX_CONTEXT_MENU_USER_STRING_LENGTH);
                        }

                        GMenuItem* item = g_menu_item_new(label.c_str(), NULL);
                        defer([item]{ g_object_unref(item); });

                        GVariant* var = g_variant_new("(xx)", tab_id.value, window.id.value);
                        if (tab_type == TabType::tree) {
                            g_menu_item_set_action_and_target_value(item, "win.move-tab-tree-to-window", var);
                        } else {
                            g_menu_item_set_action_and_target_value(item, "win.move-tab-to-window", var);
                        }
                        g_menu_append_item(window.is_tab_group?tab_group_section:saved_window_section, item);
                    }
                    g_menu_prepend_section(move_tab_submenu, NULL, G_MENU_MODEL(saved_window_section));
                    g_menu_prepend_section(move_tab_submenu, NULL, G_MENU_MODEL(tab_group_section));
                });
        });

    GMenu* profile_submenu = g_menu_new();
    defer([profile_submenu]{ g_object_unref(profile_submenu); });

    WeakRefWrapper profile_submenu_weak(profile_submenu);

    // we do not need to grab strong pointer to profile_submenu_weak here because
    // it is reffed by g_menu_append_submenu call below
    global_core_api->get_profile_ids(
        [profile_submenu_weak,profile_id,tab_id,tab_type](GetProfileIdsReply reply){
            VERIFY_LOG(reply.err);
            global_core_api->get_profiles(reply.ids,
                [profile_submenu_weak,profile_id,tab_id,tab_type](GetProfilesReply reply){
                    VERIFY_LOG(reply.err);

                    auto strong_ref = profile_submenu_weak.lock();
                    if (!strong_ref) return;
                    GMenu* profile_submenu = G_MENU(strong_ref.get());

                    for(auto profile : reply.profiles)
                    {
                        if (profile.id == profile_id) continue; // skip current profile in the list

                        std::string ellipsized_profile_name = orion::text::ellipsize_string(profile.name, MAX_CONTEXT_MENU_USER_STRING_LENGTH);
                        GMenuItem* item = g_menu_item_new(ellipsized_profile_name.c_str(), NULL);
                        defer([item]{ g_object_unref(item); });

                        GVariant* tuple = g_variant_new("(xx)", tab_id.value, profile.id.value);
                        if (tab_type == TabType::tree) {
                            g_menu_item_set_action_and_target_value(item, "win.move-tab-tree-to-profile", tuple);
                        } else {
                            g_menu_item_set_action_and_target_value(item, "win.move-tab-to-profile", tuple);
                        }
                        g_menu_append_item(profile_submenu, item);
                    }
                });
        });

    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_submenu(section, /*orion::gtk::append_invisible_unicode_suffix(*/_("Profile")/*,
            submenu_counter).c_str()*/, G_MENU_MODEL(profile_submenu)
        );
        g_menu_append_section(move_tab_submenu, NULL, G_MENU_MODEL(section));
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });

        if (tab_type == TabType::tree) {
            menu_append_action_with_int64_target(section, _("New Window"),
                "win.move-tab-tree-to-new-window", tab_id.value
            );
        } else {
            menu_append_action_with_int64_target(section, _("New Window"),
                "win.move-tab-to-new-window", tab_id.value
            );
        }

        if (tab_type == TabType::tree) {
            menu_append_action_with_int64_target(section, _("Move Tree to New In-Place Window"),
                "win.move-tab-tree-to-new-in-place-window", tab_id.value
            );
        } else {
            menu_append_action_with_int64_target(section, _("Move Tab to New In-Place Window"),
                "win.move-tab-to-new-in-place-window", tab_id.value
            );
        }
        g_menu_append_section(move_tab_submenu, NULL, G_MENU_MODEL(section));
    }

    {
        const char* title = (tab_type == TabType::tree ? _("Move Tree") : _("Move Tab"));
        g_menu_append_submenu(menu, orion::gtk::append_invisible_unicode_suffix(title, submenu_counter).c_str(),
            G_MENU_MODEL(move_tab_submenu)
        );
    }
}

void
fill_tab_context_menu(GMenu* menu, MainWindow* win, AdwTabView* tab_view, AdwTabPage* page,
    GetOtherTabIdsReply other_tab_ids_reply, CreateTabContextMenuType type, Cb<Error> done_cb)
{
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    TabId tab_id = orion_webview_tab_container_get_id(container);
    std::string tab_uri = orion_webview_tab_container_get_uri(container).uri();
    gboolean is_pinned = adw_tab_page_get_pinned(page); // gets pinned state from UI (not from backend) for simplicity
    int position = adw_tab_view_get_page_position(tab_view, page);
    ProfileId profile_id = main_window_get_current_profile_id(win);
    WindowId window_id = main_window_get_window_id(win);

    log_verbose("fill_tab_context_menu: tab_id=%lld is_pinned=%d position=%d",
        tab_id.value, is_pinned, position
    );

    WeakRefWrapper win_weak(win);
    WeakRefWrapper container_weak(container);
    WeakRefWrapper menu_weak(menu);

    global_core_api->get_setting(profile_id, orion::settings::tabs::show_nested_vertical_tabs,
        [tab_id,done_cb,win_weak,container_weak,is_pinned,menu_weak,tab_uri,other_tab_ids_reply,type,window_id]
        (GetSettingReply<bool> reply){

        if (handle_error(WRAP_ERROR(reply.err), done_cb)) return;
        bool show_nested_vertical_tabs = reply.value;

        global_core_api->get_descendant_tab_ids(tab_id,
            [tab_id,done_cb,win_weak,container_weak,is_pinned,menu_weak,tab_uri,other_tab_ids_reply,type,
             show_nested_vertical_tabs,window_id]
            (GetDescendantTabIdsReply reply){

            if (handle_error(WRAP_ERROR(reply.err), done_cb)) return;

            auto descendant_tab_ids = reply.descendant_tab_ids;

            global_core_api->get_windows({window_id},
                [tab_id,done_cb,win_weak,container_weak,is_pinned,menu_weak,tab_uri,other_tab_ids_reply,type,
                 show_nested_vertical_tabs,window_id,descendant_tab_ids]
                (GetWindowsReply reply){

                if (handle_error(WRAP_ERROR(reply.err), done_cb)) return;

                if (reply.windows.empty()) {
                    handle_error(
                        MAKE_ERROR(ErrorCode::not_found, "window with id %lld not found", window_id.value),
                        done_cb);
                    return;
                }

                const auto& window = reply.windows[0];

                auto win_strong = win_weak.lock();
                if (!win_strong) {
                    if (done_cb) done_cb(Error::ok());
                    return;
                }
                MainWindow* win = MAIN_WINDOW(win_strong.get());

                auto content_strong = container_weak.lock();
                if (!content_strong) {
                    if (done_cb) done_cb(Error::ok());
                    return;
                }
                OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(content_strong.get());

                auto menu_strong = menu_weak.lock();
                if (!menu_strong) {
                    if (done_cb) done_cb(Error::ok());
                    return;
                }
                GMenu* menu = G_MENU(menu_strong.get());

                bool is_tree_mode = (
                        type == CreateTabContextMenuType::sidebar
                    && show_nested_vertical_tabs
                    && !descendant_tab_ids.empty()
                );
                
                GMenu* move_tab_submenu = g_menu_new();
                defer([move_tab_submenu]{ g_object_unref(move_tab_submenu); });

                WeakRefWrapper move_tab_submenu_weak(move_tab_submenu);

                WindowId current_window_id = main_window_get_window_id(win);
                ProfileId profile_id = main_window_get_current_profile_id(win);
                bool is_start_page = tab_uri.empty() || tab_uri == "orion://newtab/";

                {
                    // normal tab section
                    GMenu* section = g_menu_new();
                    defer([section]{ g_object_unref(section); });

                    menu_append_action_with_int64_target(section, _("Bookmark Tab"), "win.bookmark-tab", tab_id.value);
                    menu_append_action_with_int64_target(section, _("Duplicate Tab"), "win.duplicate-tab", tab_id.value);
                    std::string uri = orion_webview_tab_container_get_uri(container).uri();
                    menu_append_action_with_string_target(section, _("Copy Link"), "win.clipboard-copy", uri.c_str());

                    if (!is_pinned) {
                        // Move Tab submenu should not be available for pinned tabs
                        append_move_tab_submenu(win, tab_id, section, TabType::normal);
                    }

                    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
                }
                if (is_tree_mode) {
                    // tree tab section
                    GMenu* section = g_menu_new();
                    defer([section]{ g_object_unref(section); });

                    menu_append_action_with_int64_target(section, _("Bookmark Tree"), "win.bookmark-tab-tree", tab_id.value);
                    
                    if (orion_webview_tab_container_get_is_suspended(container) || is_start_page) {
                        g_menu_append(section, _("Suspend Tree"), "disabled"); // disabled (but visible) item
                    } else {
                        menu_append_action_with_int64_target(section, _("Suspend Tree"), "win.suspend-tab-tree", tab_id.value);
                    }

                    if (!is_pinned) {
                        // Move Tree submenu should not be available for pinned tabs
                        append_move_tab_submenu(win, tab_id, section, TabType::tree);
                    }
                    
                    menu_append_action_with_int64_target(section, _("Close Tree"), "win.close-tab-tree", tab_id.value);

                    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
                }
                {
                    GMenu* section = g_menu_new();
                    defer([section]{ g_object_unref(section); });

                    if (!window.is_private) {
                        const char* pin_label = is_pinned ? _("Unpin Tab") : _("Pin Tab");
                        menu_append_action_with_int64_target(section, pin_label, "win.pin-tab", tab_id.value);
                    }

                    menu_append_action_with_int64_target(section, _("Reload Tab"), "win.reload-tab", tab_id.value);

                    if (orion_webview_tab_container_get_is_suspended(container) || is_start_page) {
                        g_menu_append(section, _("Suspend Tab"), "disabled"); // disabled (but visible) item
                    } else {
                        menu_append_action_with_int64_target(section, _("Suspend Tab"), "win.suspend-tab", tab_id.value);
                    }

                    menu_append_action_with_int64_target(section, _("Suspend Other Tabs"), "win.suspend-other-tabs", tab_id.value);

                    if (orion_webview_tab_container_get_is_playing_audio(container)) {
                        bool is_muted = orion_webview_tab_container_get_is_muted(container);
                        menu_append_action_with_int64_target(section, is_muted ? _("Unmute Tab") : _("Mute Tab"),
                            "win.mute-tab", tab_id.value);
                    }

                    OtherTabsAudioState other_tabs_audio_state = main_window_get_other_tabs_audio_state(win, tab_id);
                    if (other_tabs_audio_state.any_playing) {
                        const char* label = other_tabs_audio_state.any_playing_unmuted
                            ? _("Mute Other Tabs") : _("Unmute Other Tabs");
                        menu_append_action_with_int64_target(section, label, "win.mute-other-tabs", tab_id.value);
                    }

                    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
                }
                {
                    GMenu* section = g_menu_new();
                    defer([section]{ g_object_unref(section); });

                    menu_append_action_with_int64_target(section, _("Close Tab"), "win.close-tab", tab_id.value);

                    if (!is_pinned) {
                        // Close multiple tabs actions should not be available for pinned tabs

                        if (!other_tab_ids_reply.after_tab_ids.empty()) {
                            const char* label = type == CreateTabContextMenuType::sidebar
                                ? _("Close Tabs Below") : _("Close Tabs to the Right");
                            menu_append_action_with_int64_target(section, label, "win.close-tabs-below", tab_id.value);
                        }

                        if (!other_tab_ids_reply.before_tab_ids.empty()) {
                            const char* label = type == CreateTabContextMenuType::sidebar
                                ? _("Close Tabs Above") : _("Close Tabs to the Left");
                            menu_append_action_with_int64_target(section, label, "win.close-tabs-above", tab_id.value);
                        }

                        if (!other_tab_ids_reply.other_tab_ids.empty()) {
                            menu_append_action_with_int64_target(section, _("Close Other Tabs"), "win.close-other-tabs", tab_id.value);
                        }
                    }

                    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
                }

            #ifndef NDEBUG
                g_menu_append_item(menu, g_menu_item_new(std::format("DEBUG: TabId: {}", tab_id.value).c_str(), NULL));
            #endif

                if (done_cb) {
                    done_cb(Error::ok());
                }
            });
        });
    });
}

} // namespace orion::gtk