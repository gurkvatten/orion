#define ENABLE_VERBOSE_LOGS 0

#include "profile_windows_submenu.h"
#include "app.h"
#include "util/menu_util.h"
#include "util/text_util.h"
#include "util/custom_uri_util.h"
#include "window/main_window/main_window.h"
#include "common/src/ptr_util.h"
#include "action/main_window/action.h"
#include "window/main_menu/main_menu.h"

static void
append_tab_items(GMenu* menu, orion::WindowDto window, const std::vector<orion::TabDto>& tabs, MainWindow* win)
{
    for (const auto& tab : tabs) {
        orion::Uri uri = orion::Uri::parse_strict(tab.uri.c_str());
        std::string label;
        if (uri.known_scheme() == orion::Uri::Scheme::orion) {
            label = orion::custom_uri_get_corresponding_title(tab.uri);
        } else {
            label = tab.name.empty() ? _("<no-title>") : tab.name;
        }
        label = orion::text::ellipsize_string(label, MAX_CONTEXT_MENU_USER_STRING_LENGTH);


        // This has to be a checked (not radio) menu item because multiple
        // radio items would not work here with multiple windows and their tabs.
        // Note also that this must be a simple item without a target, otherwise
        // it would not work correctly with stateful action that should be displayed
        // with a checkmark in the menu. So we have to track the tab ID using user_data,
        // not variant parameter.
        std::string action_name = "switch-to-tab-in-any-window-tab-id-" + std::to_string(tab.id.value);

        GVariant* state = g_variant_new_boolean(window.selected_tab_id == tab.id);

        GSimpleAction* action = g_simple_action_new_stateful(
            action_name.c_str(),
            NULL,
            state
        );
        defer([action]{ g_object_unref(action); });

        g_signal_connect(
            action, "activate", G_CALLBACK(orion::gtk::main_window::switch_to_tab_in_any_window),
            orion::int64_to_void_ptr(tab.id.value)
        );

        // the possibly existing action is dropped automatically if it exists with this
        // action_name already
        // TODO: possibly force drop these actions as these might balloon out of control
        g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(action));

        orion::gtk::menu_append_item(
            menu,
            label.c_str(),
            ("win." + action_name).c_str()
        );
    }
}

static void
append_window_details(
    GMenu* menu,
    const orion::WindowDto& window,
    const std::vector<orion::TabDto>& tabs,
    MainWindow* win,
    int window_counter
)
{
    std::vector<orion::UiWindowId> ui_window_ids = global_core_api->get_registered_ui_window_ids();

    bool has_main_window = false;
    bool is_current_main_window = false;

    for (const auto& ui_window_id : ui_window_ids) {
        MainWindow* main_window = MAIN_WINDOW(ui_window_id.value);
        orion::WindowId window_id = main_window_get_window_id(main_window);
        if (window.id == window_id) {
            has_main_window = true;
            if (main_window == win) {
                is_current_main_window = true;
            }
            break;
        }
    }

    if (has_main_window) {
        if (!is_current_main_window) {
            orion::gtk::menu_append_action_with_int64_target(
                menu,
                _("Switch to Window"),
                "win.open-window-in-separate-window-or-switch-to-existing",
                window.id.value
            );
        }
    } else {
        orion::gtk::menu_append_action_with_int64_target(
            menu,
            _("Restore"),
            "win.open-window-in-separate-window",
            window.id.value
        );
    }

    orion::gtk::menu_append_action_with_int64_target(
        menu,
        _("Edit"),
        "win.open-window-in-separate-window-or-switch-to-existing-and-edit",
        window.id.value
    );

    if (has_main_window) {
        orion::gtk::menu_append_action_with_int64_target(
            menu,
            _("Close"),
            "win.close-window",
            window.id.value
        );
    }

    orion::gtk::menu_append_action_with_int64_target(
        menu,
        _("Duplicate"),
        "win.duplicate-window",
        window.id.value
    );

    orion::gtk::menu_append_action_with_int64_target(
        menu,
        _("Copy Tab Links"),
        "win.copy-tab-links-of-window",
        window.id.value
    );

    std::string bookmark_label = orion::text::ngettext_string(
        "Bookmark %" PRId64 " Tab", "Bookmark %" PRId64 " Tabs", window.num_tabs
    );

    orion::gtk::menu_append_action_with_int64_target(
        menu,
        bookmark_label.c_str(),
        "win.bookmark-tabs",
        window.id.value
    );

    // "Suspend N Tabs" should only be visible if the window is not suspended
    if (!window.is_suspended) {
        std::string suspend_label = orion::text::ngettext_string(
            "Suspend %" PRId64 " Tab", "Suspend %" PRId64 " Tabs", window.num_tabs
        );

        orion::gtk::menu_append_action_with_int64_target(
            menu,
            suspend_label.c_str(),
            "win.suspend-tabs-in-window",
            window.id.value
        );
    }

    std::vector<orion::TabDto> pinned_tabs;
    std::vector<orion::TabDto> unpinned_tabs;

    for (const auto& tab : tabs) {
        if (tab.is_pinned) {
            pinned_tabs.push_back(tab);
        } else {
            unpinned_tabs.push_back(tab);
        }
    }

    if (!window.is_tab_group) {
        // Delete / Pinned Tabs section
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });

        if (pinned_tabs.empty()) {
            orion::gtk::menu_append_action_with_int64_target(
                section,
                _("Delete"),
                "win.delete-window",
                window.id.value
            );
        } else {
            GMenu* pinned_submenu = g_menu_new();
            defer([pinned_submenu]{ g_object_unref(pinned_submenu); });

            append_tab_items(pinned_submenu, window, pinned_tabs, win);

            g_menu_append_submenu(
                section,
                orion::gtk::append_invisible_unicode_suffix(_("Pinned Tabs"), window_counter).c_str(),
                G_MENU_MODEL(pinned_submenu)
            );
        }

        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    }

    {
        // Unpinned Tabs section
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });

        append_tab_items(section, window, unpinned_tabs, win);

        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    }
}

static void
append_windows_section(GMenu* parent_menu, const std::vector<orion::WindowDto>& windows, MainWindow* win)
{
    GMenu* section = g_menu_new();
    defer([section]{ g_object_unref(section); });
    
    orion::gtk::WeakRefWrapper win_weak(win);

    for (const auto& window : windows) {
        std::string label;
        if (window.is_tab_group && !window.is_private) {
            label = orion::text::ngettext_string("%" PRId64 " Tab", "%" PRId64 " Tabs", window.num_tabs);
        } else {
            // Saved Tab Group or Private Tab Group
            label = orion::text::ellipsize_string(window.name, MAX_CONTEXT_MENU_USER_STRING_LENGTH);
        }
        label = orion::gtk::append_invisible_unicode_suffix(label, global_window_counter);
        global_window_counter++;

        GMenu* win_submenu = g_menu_new();
        defer([win_submenu]{ g_object_unref(win_submenu); });

        g_menu_append_submenu(section, label.c_str(), G_MENU_MODEL(win_submenu));

        orion::gtk::WeakRefWrapper win_submenu_weak(win_submenu);
        int window_counter = global_window_counter; // capture current counter value

        orion::GetTabIdsQuery query{
            .window_id = window.id,
        };
        global_core_api->get_tab_ids(query, [win_submenu_weak,window,win_weak,window_counter](orion::GetTabIdsReply reply){
            VERIFY_LOG(reply.err);

            global_core_api->get_tabs(reply.ids, [win_submenu_weak,window,win_weak,window_counter](orion::GetTabsReply reply){
                VERIFY_LOG(reply.err);

                auto win_submenu_strong = win_submenu_weak.lock();
                if (!win_submenu_strong) return;
                GMenu* win_submenu = G_MENU(win_submenu_strong.get());

                auto win_strong = win_weak.lock();
                if (!win_strong) return;
                MainWindow* win = MAIN_WINDOW(win_strong.get());

                append_window_details(win_submenu, window, reply.tabs, win, window_counter);
            });
        });
    }
    
    g_menu_append_section(parent_menu, NULL, G_MENU_MODEL(section));
}

void
update_profile_windows_submenu(
    GMenu* profile_windows_submenu, orion::ProfileId profile_id, MainWindow* win,
    std::function<void()> done_cb
)
{
    orion::log_verbose("update_profile_windows_submenu: profile_id=%" PRId64 " menu_ptr=%p",
        profile_id.value, profile_windows_submenu
    );

    orion::gtk::WeakRefWrapper profile_windows_submenu_weak(profile_windows_submenu);
    orion::gtk::WeakRefWrapper win_weak(win);

    global_core_api->get_window_ids({.profile_id=profile_id},
        [profile_windows_submenu_weak,win_weak,done_cb=std::move(done_cb)](orion::GetWindowIdsReply reply){
            VERIFY_LOG(reply.err);

            global_core_api->get_windows(reply.ids,
                [profile_windows_submenu_weak,win_weak,done_cb=std::move(done_cb)](orion::GetWindowsReply reply){
                    VERIFY_LOG(reply.err);

                    std::vector<orion::WindowDto> saved_windows;
                    std::vector<orion::WindowDto> unsaved_windows;

                    for (orion::WindowDto window : reply.windows) {
                        if (window.is_tab_group) {
                            unsaved_windows.push_back(window);
                        } else {
                            saved_windows.push_back(window);
                        }
                    }

                    auto profile_windows_submenu_strong = profile_windows_submenu_weak.lock();
                    if (!profile_windows_submenu_strong) return;
                    GMenu* profile_windows_submenu = G_MENU(profile_windows_submenu_strong.get());

                    auto win_strong = win_weak.lock();
                    if (!win_strong) return;
                    MainWindow* win = MAIN_WINDOW(win_strong.get());

                    g_menu_remove_all(profile_windows_submenu);
                    append_windows_section(profile_windows_submenu, unsaved_windows, win);
                    append_windows_section(profile_windows_submenu, saved_windows, win);

                    if (done_cb) {
                        done_cb();
                    }
                });
        });
}
