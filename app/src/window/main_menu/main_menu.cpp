#define ENABLE_VERBOSE_LOGS 0

#include "main_menu.h"
#include "app.h"
#include "util/menu_util.h"
#include "util/bookmarks_util.h"
#include "window/profiles_icon_selection_box.h"
#include "window/main_window/header/header_bar.h"
#include "util/tab_context_menu.h"
#include "util/text_util.h"
#include "util/custom_uri_util.h"
#include "common/src/time_util.h"
#include "window/main_window/menu/profiles_button_menu.h"
#include "window/main_window/main_window.h"
#include <common/src/log.h>

#include <functional>
#include <gtk/gtk.h>

#include <common/src/defer.h>
#include <core/core_api.h>
#include <common/src/coroutine.h>
#include <core/settings.h>
#include <common/src/string_util.h>

#include "data_item/menu_item.h"

const gint submenu_index_orion     = 0; // "Orion" submenu
const gint submenu_index_file      = 1; // "File" submenu
const gint submenu_index_edit      = 2; // "Edit" submenu
const gint submenu_index_view      = 3; // "View" submenu
const gint submenu_index_history   = 4; // "History" submenu
const gint submenu_index_bookmarks = 5; // "Bookmarks" submenu
const gint submenu_index_tools     = 6; // "Tools" submenu
const gint submenu_index_develop   = 7; // "Develop" submenu
const gint submenu_index_window    = 8; // "Window" submenu
const gint submenu_index_help      = 9; // "Help" submenu

// Workaround to avoid submenu name collisions in sliding GMenu
// TODO: check how exactly this is used, because we sometimes make a copy instead of passing by reference, so the count for something like an async callback will be messed up
// also, this probably should not be in the global space, with enough main menu opens, this will balloon out of control, should've just been passed as an argument
// also, orion::gtk::transform_to_unique_menu_name should've likely been the thing to use
// also check other global/static counters
int global_window_counter = 0;

static void
setup_recently_bookmarked_submenu(MainWindow* win, GMenu* bookmarks_submenu)
{
    GMenu* recently_bookmarked_menu = g_menu_new();
    defer([menu=recently_bookmarked_menu]{ g_object_unref(menu); });

    main_window_set_recently_bookmarked_menu(win, recently_bookmarked_menu);

    GMenu* section = g_menu_new();
    defer([section]{ g_object_unref(section); });

    g_menu_append_submenu(section, _("Recently Bookmarked"), G_MENU_MODEL(recently_bookmarked_menu));
    g_menu_append_section(bookmarks_submenu, NULL, G_MENU_MODEL(section));

    orion::gtk::WeakRefWrapper menu_weak(recently_bookmarked_menu);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::GetRecentBookmarkIdsQuery query{};
    query.profile_id = profile_id;

    global_core_api->get_recent_bookmark_ids(query, [menu_weak](orion::GetRecentBookmarkIdsReply reply){
        VERIFY_LOG(reply.err);
        orion::GetBookmarksQuery query{
            .ids = reply.ids,
            .order_by = orion::BookmarkOrderByMode::created_at_desc,
        };
        global_core_api->get_bookmarks(query, [menu_weak](orion::GetBookmarksReply reply){
            VERIFY_LOG(reply.err);

            auto menu_strong = menu_weak.lock();
            if (!menu_strong) return;
            GMenu* menu = G_MENU(menu_strong.get());

            g_menu_remove_all(menu);
            for(auto bookmark : reply.bookmarks)
            {
                //TODO: correct action
                std::string item_title = orion::text::ellipsize_string(bookmark.name, MAX_CONTEXT_MENU_USER_STRING_LENGTH);
                orion::gtk::menu_append_action_with_string_target(menu, item_title.c_str(), "win.open-bookmark", bookmark.address.c_str());
            }
        });
    });

    if (main_window_get_recently_bookmarked_change_observer(win).value == 0) {
        main_window_set_recently_bookmarked_change_observer(win, global_core_api->bookmarks_change().add_observer(
            [menu_weak,profile_id](const orion::BookmarksChange& change){
                orion::GetRecentBookmarkIdsQuery query{};
                query.profile_id = profile_id;
                global_core_api->get_recent_bookmark_ids(query, [menu_weak](orion::GetRecentBookmarkIdsReply reply){
                    VERIFY_LOG(reply.err);
                    orion::GetBookmarksQuery query{
                        .ids = reply.ids,
                        .order_by = orion::BookmarkOrderByMode::created_at_desc,
                    };
                    global_core_api->get_bookmarks(query, [menu_weak](orion::GetBookmarksReply reply){
                        VERIFY_LOG(reply.err);

                        auto menu_strong = menu_weak.lock();
                        if (!menu_strong) return;
                        GMenu* menu = G_MENU(menu_strong.get());

                        g_menu_remove_all(menu);
                        for(auto bookmark : reply.bookmarks)
                        {
                            //TODO: correct action
                            std::string item_title = orion::text::ellipsize_string(bookmark.name, MAX_CONTEXT_MENU_USER_STRING_LENGTH);
                            orion::gtk::menu_append_action_with_string_target(menu, item_title.c_str(), "win.open-bookmark", bookmark.address.c_str());
                        }
                    });
                });
        }));
    }
}

//The ultimate children seek & destroy fun
static void
destroy_all_children_recursive(GMenu* menu)
{
    int n_items = g_menu_model_get_n_items(G_MENU_MODEL(menu));

    for (int i = n_items - 1; i >= 0; i--) {
        GMenuModel* child_submenu = g_menu_model_get_item_link(G_MENU_MODEL(menu), i, "submenu");
        if (child_submenu) {
            if (G_IS_MENU(child_submenu)) {
                destroy_all_children_recursive(G_MENU(child_submenu));
            }
            g_object_unref(child_submenu);
        }

        g_menu_remove(menu, i);
    }
}

static void
fill_folder_menu(
    GMenu* root_menu,
    orion::BookmarkFolderId root_folder_id,
    std::vector<orion::BookmarkFolderDto>* folders,
    std::vector<orion::BookmarkDto>* bookmarks,
    std::multiset<std::string>* menu_names)
{
    std::vector<std::pair<double, GMenuItem*>> entries;
    defer([&entries]{
        for (auto& entry : entries) {
            g_object_unref(entry.second);
        }
    });

    for (int bm_index = bookmarks->size() - 1; bm_index >= 0; bm_index--) 
    {
        const orion::BookmarkDto& book_mark = bookmarks->at(bm_index);
        if (book_mark.parent_folder_id == root_folder_id) {
            std::string ellipsized = orion::text::ellipsize_string(book_mark.name, MAX_CONTEXT_MENU_USER_STRING_LENGTH);
            //this is unnecessary, only matters for submenu items, will reduce overhead
            //std::string name = orion::gtk::transform_to_unique_menu_name("bookmark", ellipsized, menu_names);
            GMenuItem* menuitem = g_menu_item_new(ellipsized.c_str(), NULL);
            g_menu_item_set_action_and_target(menuitem, "win.open-bookmark", "s",  book_mark.address.c_str());
            entries.emplace_back(book_mark.seq_num, menuitem);
            bookmarks->erase(bookmarks->begin() + bm_index);
        }
    }
    for (int folder_index = folders->size() - 1; folder_index >= 0; folder_index--)
    {
        const orion::BookmarkFolderDto& folder = folders->at(folder_index);
        if (folder.parent_folder_id == root_folder_id) {
            GMenu* new_menu = g_menu_new();
            defer([new_menu]{
                g_object_unref(new_menu);
            });
            
            fill_folder_menu(new_menu, folder.id, folders, bookmarks, menu_names);

            std::string ellipsized = orion::text::ellipsize_string(folder.name, MAX_CONTEXT_MENU_USER_STRING_LENGTH);
            std::string name = orion::gtk::transform_to_unique_menu_name("folder", ellipsized, menu_names);

            GMenuItem* folder_item = g_menu_item_new_submenu(name.c_str(), G_MENU_MODEL(new_menu));
            entries.emplace_back(folder.seq_num, folder_item);

            folders->erase(folders->begin() + folder_index); //Could proly erase before the recursive call, but just to be on a safe side
        }
    }

    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) {
                  return a.first < b.first;
              });

    for (auto& entry : entries) {
        auto value = g_menu_item_get_attribute_value(entry.second, "label", G_VARIANT_TYPE_STRING);
        if (value) {
            defer([value]{g_variant_unref(value);});
            const gchar* label = g_variant_get_string(value, NULL);
            orion::log_nop("fill_folder_menu: adding bookmark entry: label=%s menu_ptr=%p seq_num=%f", label, root_menu, entry.first);
        }

        g_menu_append_item(root_menu, entry.second);
    }    
}

static void
setup_bookmarks_menu(MainWindow* win)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    GMenu* all_bookmarks_section = main_window_get_all_bookmarks_section(win);

    orion::gtk::WeakRefWrapper wrapped_ref(all_bookmarks_section);

    fetch_all_bookmarks_data(profile_id,
        [wrapped_ref]
        (BookmarksAllReply reply){
            VERIFY_LOG(reply.error);
            auto ptr = wrapped_ref.lock();
            if (!ptr) return;

            GMenu* all_bookmarks_section = G_MENU(ptr.get());

            destroy_all_children_recursive(all_bookmarks_section);

            std::multiset<std::string> menu_names; // to avoid duplicate names in the menu
            fill_folder_menu(all_bookmarks_section, orion::BookmarkFolderId(0), &reply.folders, &reply.bookmarks, &menu_names);
    });
}

static void
setup_all_bookmarks_submenu(MainWindow* win, GMenu* bookmarks_submenu)
{
    GMenu* all_bookmarks_section = g_menu_new();
    main_window_set_all_bookmarks_section(win, all_bookmarks_section);

    g_menu_append_section(bookmarks_submenu, NULL, G_MENU_MODEL(all_bookmarks_section));

    setup_bookmarks_menu(win);
    orion::gtk::WeakRefWrapper win_weak(win);

    if (main_window_get_all_folders_change_observer(win).value == 0) {
        main_window_set_all_folders_change_observer(win, global_core_api->bookmark_folders_change().add_observer(
            [win_weak](const orion::BookmarkFoldersChange& change){
                auto win_strong = win_weak.lock();
                if (!win_strong) return;
                MainWindow* win = MAIN_WINDOW(win_strong.get());

                setup_bookmarks_menu(win);
            }));
    }

    if (main_window_get_all_bookmarked_change_observer(win).value == 0) {
        main_window_set_all_bookmarked_change_observer(win, global_core_api->bookmarks_change().add_observer(
            [win_weak](const orion::BookmarksChange& change){
                auto win_strong = win_weak.lock();
                if (!win_strong) return;
                MainWindow* win = MAIN_WINDOW(win_strong.get());

                setup_bookmarks_menu(win);
            }));
    }
}

using RecentlyClosedWindowTabsMap = std::map<orion::RecentlyClosedWindowId, std::vector<orion::RecentlyClosedTabDto>>;

static void
fetch_recently_closed_window_tabs_map(
    orion::ProfileId profile_id, const std::vector<orion::RecentlyClosedWindowId>& window_ids,
    Cb<RecentlyClosedWindowTabsMap> cb)
{
    if (window_ids.empty()) {
        cb(RecentlyClosedWindowTabsMap{});
        return;
    }

    auto done_windows = std::make_shared<size_t>(0);
    auto window_tabs_map = std::make_shared<RecentlyClosedWindowTabsMap>();

    for (auto window_id : window_ids) {
        orion::GetRecentlyClosedTabIdsQuery query{
            .profile_id = profile_id,
            .window_id = window_id,
        };
        
        // note: do not try to std::move cb here, we need it potentially for each window_id
        global_core_api->get_recently_closed_tab_ids(query, [=](orion::GetRecentlyClosedTabIdsReply reply){
            VERIFY_LOG(reply.err);

            global_core_api->get_recently_closed_tabs(reply.ids, [=](orion::GetRecentlyClosedTabsReply reply){
                VERIFY_LOG(reply.err);
                window_tabs_map->emplace(window_id, reply.tabs);
                (*done_windows)++;
                if (*done_windows == window_ids.size()) {
                    cb(*window_tabs_map);
                }
            });
        });
    }
}

enum class RecentlyClosedObjectType {
    not_set,
    tab,
    window,
};

// run synchronously, no need to capture weak pointers
static auto
handle_single_tab = [](GMenu* menu, orion::RecentlyClosedTabDto tab){
    std::string title = tab.name;
    if (orion::Uri::parse_strict(tab.uri.c_str()).known_scheme() == orion::Uri::Scheme::orion) {
        title = orion::custom_uri_get_corresponding_title(tab.uri);
    } else if (tab.name.empty()) {
        title = _("<no-title>");
    }
    std::string ellipsized = orion::text::ellipsize_string(title, MAX_CONTEXT_MENU_USER_STRING_LENGTH);
    orion::gtk::menu_append_action_with_int64_target(
        menu, ellipsized.c_str(), "win.open-recently-closed-tab", tab.id.value
    );
};

static void
create_recently_closed_tabs_menu(
    orion::ProfileId profile_id, std::function<void(GMenu* section, std::vector<orion::RecentlyClosedTabDto> tabs, RecentlyClosedObjectType type)> cb)
{
    orion::GetRecentlyClosedTabIdsQuery query{
        .profile_id = profile_id,
        .window_id = static_cast<orion::RecentlyClosedWindowId>(0), // include only tabs without a window
    };
    global_core_api->get_recently_closed_tab_ids(query, [=](orion::GetRecentlyClosedTabIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->get_recently_closed_tabs(reply.ids, [=](orion::GetRecentlyClosedTabsReply reply){
            VERIFY_LOG(reply.err);
            global_core_api->get_recently_closed_window_ids(profile_id, [=,tabs=reply.tabs](orion::GetRecentlyClosedWindowIdsReply reply){
                VERIFY_LOG(reply.err);

                fetch_recently_closed_window_tabs_map(profile_id, reply.ids, [=](RecentlyClosedWindowTabsMap window_tabs_map){
                    global_core_api->get_recently_closed_windows(reply.ids, [=](orion::GetRecentlyClosedWindowsReply reply){
                        VERIFY_LOG(reply.err);

                        const auto& windows = reply.windows;

                        GMenu* section = g_menu_new();
                        defer([section]{ g_object_unref(section); });

                        // for sorting
                        struct Item {
                            orion::UnixTimeUtc64 closed_at{};
                            RecentlyClosedObjectType type{};
                            size_t index{};
                        };

                        // make this configurable in the future if needed
                        const size_t max_items = 20;

                        std::vector<Item> items;
                        for (size_t i = 0; i < tabs.size(); i++) {
                            if (items.size() >= max_items) break;
                            items.push_back(Item{tabs[i].closed_at, RecentlyClosedObjectType::tab, i});
                        }
                        for (size_t i = 0; i < windows.size(); i++) {
                            if (items.size() >= max_items) break;
                            items.push_back(Item{windows[i].closed_at, RecentlyClosedObjectType::window, i});
                        }

                        // sort by closed_at descending
                        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b){
                            return a.closed_at.value > b.closed_at.value;
                        });

                        // needed to avoid submenu name collisions in sliding GMenu
                        int submenu_counter = 0;

                        for (const auto& item : items) {
                            if (item.type == RecentlyClosedObjectType::tab) {
                                const auto& tab = tabs[item.index];
                                handle_single_tab(section, tab);
                            } else if (item.type == RecentlyClosedObjectType::window) {
                                const auto& window = windows[item.index];
                                auto it = window_tabs_map.find(window.id);
                                if (it == window_tabs_map.end()) {
                                    orion::log_warning("create_recently_closed_tabs_menu: no tabs found for recently closed window id=%lld",
                                        window.id.value
                                    );
                                    continue;
                                }
                                if (it->second.empty()) {
                                    orion::log_verbose("create_recently_closed_tabs_menu: empty tabs list for recently closed window id=%lld",
                                        window.id.value
                                    );
                                    continue;
                                }

                                auto first_tab = it->second[0];
                                if (it->second.size() == 1) {
                                    // show single tab directly
                                    handle_single_tab(section, first_tab);
                                    continue;
                                }

                                std::string submenu_title = first_tab.name + " " + orion::text::ngettext_string(
                                    "(and %" PRId64 " other tab)",
                                    "(and %" PRId64 " other tabs)",
                                    it->second.size() - 1 // exclude the first tab which is shown in submenu title
                                );
                                std::string ellipsized_title = orion::text::ellipsize_string(submenu_title, MAX_CONTEXT_MENU_USER_STRING_LENGTH);

                                GMenu* window_submenu = g_menu_new();
                                defer([window_submenu]{ g_object_unref(window_submenu); });

                                {
                                    GMenu* subsection = g_menu_new();
                                    defer([subsection]{ g_object_unref(subsection); });

                                    orion::gtk::menu_append_action_with_int64_target(
                                        subsection, _("Reopen Window"), "win.open-recently-closed-window", window.id.value
                                    );

                                    g_menu_append_section(window_submenu, NULL, G_MENU_MODEL(subsection));
                                }
                                {
                                    GMenu* submenu_section = g_menu_new();
                                    defer([submenu_section]{ g_object_unref(submenu_section); });

                                    for (const auto& tab : it->second) {
                                        handle_single_tab(submenu_section, tab);
                                    }

                                    g_menu_append_section(window_submenu, NULL, G_MENU_MODEL(submenu_section));
                                }
                                ellipsized_title = orion::gtk::append_invisible_unicode_suffix(ellipsized_title, submenu_counter);
                                submenu_counter++;
                                g_menu_append_submenu(section, ellipsized_title.c_str(), G_MENU_MODEL(window_submenu));
                            }
                        }

                        RecentlyClosedObjectType type = RecentlyClosedObjectType::not_set;
                        if (!items.empty()) {
                            type = items[0].type;
                        }

                        cb(section, tabs, type);
                    });
                });
            });
        });
    });
}

#ifndef NDEBUG
static void
append_debug_submenu(GMenu* main_menu, MainWindow* win)
{
    GMenu* submenu = g_menu_new();
    defer([submenu]{ g_object_unref(submenu); });

    // Do not localize these:
    orion::gtk::menu_append_item(submenu, "MainWindow pointer: " + orion::str::ptr_to_hex(win), "win.disabled");

    GMenu* section = g_menu_new();
    defer([section]{ g_object_unref(section); });

    g_menu_append_section(main_menu, NULL, G_MENU_MODEL(section));
    g_menu_append_submenu(section, _("DEBUG"), G_MENU_MODEL(submenu));
}
#endif // !NDEBUG


// output format example: "Thursday 18 September 2025"
static std::string
_format_history_entry_timestamp(int64_t timestamp)
{
    GDateTime* date_time = g_date_time_new_from_unix_utc(timestamp);
    defer([date_time]{ g_date_time_unref(date_time); });

    GDateTime* local_time = g_date_time_to_local(date_time);
    defer([local_time]{ g_date_time_unref(local_time); });

    gchar* formatted = g_date_time_format(local_time, "%A %d %B %Y");
    defer([formatted]{ g_free(formatted); });

    return std::string(formatted);
}

static std::string make_title(const orion::HistoryEntryDto& page){
    //TODO maybe some global url utils?
    std::string title = page.website;
    if (title.empty()) {
        size_t pos = page.address.find("://");
        if (pos != std::string::npos) {
            title = page.address.substr(pos + 3);
        }
    }
    return orion::text::ellipsize_string(title, MAX_CONTEXT_MENU_USER_STRING_LENGTH);
}

static auto
create_day_history_manu_group = [](int64_t collecting, size_t idx, std::vector<orion::HistoryEntryDto>& entries,  GMenu* history_menu) -> size_t {
    //create day group section and append just created day history sites submenu to it
    GMenu* day_history_menu =  g_menu_new();
    defer([day_history_menu]{ g_object_unref(day_history_menu); });

    for (;idx < entries.size(); idx++) {
        const auto& page = entries.at(idx);
        if (page.visited_at.value <= 0) {
            continue;
        }
        if (page.visited_at.value != collecting) {
            break;
        }
        //TODO maybe some global url utils?
        std::string title = make_title(page);
        orion::gtk::menu_append_action_with_string_target(
            day_history_menu
                        , title.c_str()
                        , "win.open-history-address-in-current-tab"
                        , page.address.c_str());
    }
    if (g_menu_model_get_n_items(G_MENU_MODEL(day_history_menu)) > 0) {
        //attach new setion only if we've got items, which we really ougth have
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_submenu(section
            , _format_history_entry_timestamp(collecting).c_str()
            , G_MENU_MODEL(day_history_menu));
        //addd the day section to the history menu
        g_menu_append_section(history_menu, NULL, G_MENU_MODEL(section));
    }
    return idx;
};

static void
create_history_submenu(MainWindow* win, GMenu* history_menu)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper history_menu_weak(history_menu);
    orion::gtk::WeakRefWrapper win_weak(win);

    create_recently_closed_tabs_menu(profile_id,
        [history_menu_weak,win_weak, profile_id](GMenu* recently_closed_tabs_section, std::vector<orion::RecentlyClosedTabDto> tabs
            , RecentlyClosedObjectType type){
            auto history_menu_strong = history_menu_weak.lock();
            if (!history_menu_strong) return;
            GMenu* history_menu = G_MENU(history_menu_strong.get());

            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            OrionWebviewTabView* tab_view = main_window_get_webview_tab_view(win);

            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                g_menu_append_section(history_menu, NULL, G_MENU_MODEL(section));

                orion::gtk::menu_append_item(section, _("Show History"), "win.show-history");
                orion::gtk::menu_append_item(section, _("Clear History…"), "win.clear-history");
            }
            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                g_menu_append_section(history_menu, NULL, G_MENU_MODEL(section));

                if (orion_webview_tab_view_can_go_back(tab_view)) {
                    orion::gtk::menu_append_item(section, _("Back"), "win.back");
                } else {
                    orion::gtk::menu_append_item(section, _("Back"), "win.disabled");
                }

                if (orion_webview_tab_view_can_go_forward(tab_view)) {
                    orion::gtk::menu_append_item(section, _("Forward"), "win.forward");
                } else {
                    orion::gtk::menu_append_item(section, _("Forward"), "win.disabled");
                }
                
                orion::gtk::menu_append_item(section, _("Home"), "win.home");
            }
            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                g_menu_append_section(history_menu, NULL, G_MENU_MODEL(section));

                g_menu_append_submenu(section, _("Recently Closed"), G_MENU_MODEL(recently_closed_tabs_section));

                switch (type) {
                case RecentlyClosedObjectType::tab:
                    orion::gtk::menu_append_item(section, _("Reopen Last Closed Tab"), "win.reopen-last-closed-tab");
                    break;
                case RecentlyClosedObjectType::window:
                    orion::gtk::menu_append_item(section, _("Reopen Last Closed Window"), "win.reopen-last-closed-window");
                    break;
                default:
                    orion::gtk::menu_append_item(section, _("Reopen Last Closed Tab / Window"), "win.disabled");
                    break;
                }

                // probably not needed ever because windows from the last session are always
                // reopened on startup automatically
                orion::gtk::menu_append_item(section, _("Reopen All Windows from Last Session"),
                    "win.reopen-all-windows-from-last-session"
                );
            }
            {
                orion::ProfileHistoryQuery query{
                    .profile_id = profile_id,
                    .search_filter = "length(address) > 0",
                    .offset = 0,
                    .limit = 4 //last non empty 4 days, including today. If today isn't present then there'll be 4 days
                    //in the dayly grouped menu
                };

                //lets pass weak menu ptr rather than to move the unique ptr, that way we avoid dealing with menu that might be destroyed already
                global_core_api->get_history_entries(query,
                    [history_menu_weak](orion::GetHistoryEntriesReply reply) {
                        VERIFY_LOG(reply.err);
                        auto history_menu_strong = history_menu_weak.lock();
                        if (!history_menu_strong) return;
                        GMenu* history_menu = G_MENU(history_menu_strong.get());
                        int64_t today = orion::tm::today_local();
                        gint n_items = 0;
                        {
                            //last N pages (6?) from history from today
                            // TODO: missing icon?
                            GMenu* section = g_menu_new();
                            defer([section]{ g_object_unref(section); });
                            for (const auto& page : reply.entries) {
                                if (today == orion::tm::to_day(page.visited_at)) {
                                    orion::gtk::menu_append_action_with_string_target(section, make_title(page).c_str()
                                        , "win.open-history-address-in-current-tab"
                                        , page.address.c_str());
                                    if (++n_items >= 6) break;
                                }
                            }
                            if (n_items > 0) {
                                g_menu_append_section(history_menu, NULL, G_MENU_MODEL(section));
                            }
                        }                        
                        //items are sorted timewise descendent hnce we can start from where we left off above
                        for (size_t i = n_items; i < reply.entries.size(); i++) {
                            const auto& page = reply.entries.at(i);
                            //above we bail out after 6 items => need to check here as well
                            if (page.visited_at.value <= 0 || today == orion::tm::to_day(page.visited_at)) {
                                continue;
                            }
                            i = create_day_history_manu_group(page.visited_at.value, i, reply.entries
                                    , history_menu) - 1; //need to come back to where the 
                                    //create_day_history_manu_group bailed out + account for the loop increase
                        };
                    });
            }
        });
}

struct CreateFileSubmenuImplQuery {
    MainWindow* win{};
    GMenu* file_menu{};
    orion::settings::TabLayout tab_layout{};
    orion::TabDto tab{};
    orion::gtk::WeakRefWrapper popover_menu_weak;
    const std::vector<orion::TabId>& descendant_tab_ids{};
    orion::WindowDto window{};
};

static void
create_file_submenu_impl(CreateFileSubmenuImplQuery query)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(query.win);
    orion::gtk::WeakRefWrapper win_weak(query.win);

    {
        GMenu* profile_section = g_menu_new();
        defer([profile_section]{ g_object_unref(profile_section); });
        g_menu_append_submenu(query.file_menu, _("Profiles"), G_MENU_MODEL(profile_section));

        recreate_profiles_menu(profile_section, profile_id, query.win,
            [popover_menu_weak=query.popover_menu_weak,win_weak]{
            
            auto popover_menu_strong = popover_menu_weak.lock();
            if (!popover_menu_strong) return;
            GtkPopoverMenu* popover_menu = GTK_POPOVER_MENU(popover_menu_strong.get());

            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            ProfilesIconSelectionBox* icon_selection = profiles_icon_selection_box_new();
            g_signal_connect(icon_selection, "map", G_CALLBACK(on_profiles_button_icon_selection_box_map), win);

            // this has to be called after the menu item that is using it is already present in the menu
            // otherwise the custom widget would not be visible
            gtk_popover_menu_add_child(GTK_POPOVER_MENU(popover_menu), GTK_WIDGET(icon_selection), "profiles_icon_selection");
        });
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(query.file_menu, NULL, G_MENU_MODEL(section));
        g_menu_append(section, _("New Tab"), "win.new-tab");
        g_menu_append(section, _("New Window"), "win.new-window");
        g_menu_append(section, _("New Focus Window"), "win.new-focus-window");
        g_menu_append(section, _("New Private Window"), "win.new-private-window");
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(query.file_menu, NULL, G_MENU_MODEL(section));
        g_menu_append(section, _("Open File…"), "win.open-file");
        g_menu_append(section, _("Open Location…"), "win.select-address-entry");
        if (query.window.is_private) {
            g_menu_append(section, _("Save Window…"), "win.disabled");
        } else {
            g_menu_append(section, _("Save Window…"), "win.edit-current-window-group");
        }
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(query.file_menu, NULL, G_MENU_MODEL(section));
        g_menu_append(section, _("Close Window"), "win.close-current-window");
        g_menu_append(section, _("Close All Windows"), "win.close-all-windows");
        g_menu_append(section, _("Close Tab"), "win.close-current-tab");

        {
            const char* action = query.tab.is_pinned ? "win.disabled" : "win.close-tabs-above-current-tab";
            if (query.tab_layout == orion::settings::TabLayout::vertical) {
                g_menu_append(section, _("Close Tabs Above"), action);
            } else {
                g_menu_append(section, _("Close Tabs to the Left"), action);
            }
        }

        if (query.tab_layout == orion::settings::TabLayout::vertical && !query.descendant_tab_ids.empty()) {
            g_menu_append(section, _("Close Tree"), "win.close-current-tab-tree");
        } else {
            g_menu_append(section, _("Close Tree"), "win.disabled");
        }
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        //TODO:
        g_menu_append_section(query.file_menu, NULL, G_MENU_MODEL(section));
        GMenu* share_submenu = g_menu_new();
        defer([share_submenu]{ g_object_unref(share_submenu); });
        g_menu_append_submenu(section, _("Share"), G_MENU_MODEL(share_submenu));
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(query.file_menu, NULL, G_MENU_MODEL(section));
        g_menu_append(section, _("Save Page…"), "win.save-page");
        g_menu_append(section, _("Export as PDF…"), "win.export-as-pdf");
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(query.file_menu, NULL, G_MENU_MODEL(section));
        {
            GMenu* submenu = g_menu_new();
            defer([submenu]{ g_object_unref(submenu); });
            g_menu_append_submenu(section, _("Import From"), G_MENU_MODEL(submenu));
            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                g_menu_append(section, _("Profile data…"), "win.import-profile-data");
                g_menu_append(section, _("Bookmarks HTML File…"), "win.import-bookmarks-html-file");
                g_menu_append(section, _("Passwords CSV File…"), "win.import-passwords-csv-file-main-menu");
                g_menu_append_section(submenu, NULL, G_MENU_MODEL(section));
            }
        }
        {
            GMenu* submenu = g_menu_new();
            defer([submenu]{ g_object_unref(submenu); });
            g_menu_append_submenu(section, _("Export"), G_MENU_MODEL(submenu));
            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                g_menu_append(section, _("Profile data…"), "win.export-profile-data");
                g_menu_append(section, _("Bookmarks…"), "win.export-bookmarks");
                g_menu_append(section, _("Passwords…"), "win.export-passwords");
                g_menu_append_section(submenu, NULL, G_MENU_MODEL(section));
            }
        }
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(query.file_menu, NULL, G_MENU_MODEL(section));
        g_menu_append(section, _("Page Setup…"), "win.page-setup");
        g_menu_append(section, _("Print…"), "win.print");
    }
}

static void
create_file_submenu(MainWindow* win, GMenu* file_menu, GtkPopoverMenu* popover_menu)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    orion::gtk::WeakRefWrapper win_weak(win);
    orion::gtk::WeakRefWrapper file_menu_weak(file_menu);
    orion::gtk::WeakRefWrapper popover_menu_weak(popover_menu);

    // How to make generic settings work with AWAIT()?
    //orion::GetSettingReply<orion::settings::TabLayout> tab_layout = AWAIT(get_setting, profile_id, orion::settings::tabs::tab_layout);

    orion::TabId tab_id = main_window_get_current_tab_id(win);
    orion::WindowId window_id = main_window_get_window_id(win);

    global_core_api->get_windows({window_id},
        [profile_id,win_weak,file_menu_weak,popover_menu_weak,tab_id](orion::GetWindowsReply reply){

        VERIFY_LOG(reply.err);

        if (reply.windows.empty()) {
            orion::log_error("create_file_submenu: current window not found, cannot create File submenu");
            return;
        }
        const orion::WindowDto& window = reply.windows.at(0);

        global_core_api->get_tabs({tab_id},
            [profile_id,win_weak,file_menu_weak,popover_menu_weak,window](orion::GetTabsReply reply){

            VERIFY_LOG(reply.err);

            if (reply.tabs.empty()) {
                orion::log_error("create_file_submenu: current tab not found, cannot create File submenu");
                return;
            }
            const orion::TabDto& tab = reply.tabs.at(0);

            global_core_api->get_descendant_tab_ids(tab.id,
                [win_weak,file_menu_weak,tab,popover_menu_weak,profile_id,window]
                (orion::GetDescendantTabIdsReply reply){

                VERIFY_LOG(reply.err);

                std::vector<orion::TabId> descendant_tab_ids = reply.descendant_tab_ids;
                
                global_core_api->get_setting(profile_id, orion::settings::tabs::tab_layout,
                    [win_weak,file_menu_weak,tab,popover_menu_weak,descendant_tab_ids,window]
                    (orion::GetSettingReply<orion::settings::TabLayout> reply){

                    VERIFY_LOG(reply.err);

                    orion::settings::TabLayout tab_layout = reply.value;

                    auto win_strong = win_weak.lock();
                    if (!win_strong) return;
                    MainWindow* win = MAIN_WINDOW(win_strong.get());

                    auto file_menu_strong = file_menu_weak.lock();
                    if (!file_menu_strong) return;
                    GMenu* file_menu = G_MENU(file_menu_strong.get());

                    create_file_submenu_impl(CreateFileSubmenuImplQuery{
                        .win = win,
                        .file_menu = file_menu,
                        .tab_layout = tab_layout,
                        .tab = tab,
                        .popover_menu_weak = popover_menu_weak,
                        .descendant_tab_ids = descendant_tab_ids,
                        .window = window,
                    });
                });
            });
        });
    });
}

// callback is called with true if "Merge All Windows" item should be enabled, false otherwise
static void
is_merge_all_windows_item_enabled(MainWindow* win, std::function<void(bool)> cb)
{
    orion::WindowId window_id = main_window_get_window_id(win);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    global_core_api->get_window_ids({.profile_id=profile_id}, [window_id,cb=std::move(cb)](orion::GetWindowIdsReply reply){
        VERIFY_LOG(reply.err);

        global_core_api->get_windows(reply.ids, [window_id,cb=std::move(cb)](orion::GetWindowsReply reply){
            VERIFY_LOG(reply.err);

            int n_tab_groups = 0;
            for (const auto& window : reply.windows) {
                if (window.id == window_id && !window.is_tab_group) {
                    // current window is Saved Window (Saved Tab Group)
                    cb(false);
                    return;
                }

                if (window.is_tab_group) {
                    n_tab_groups++;
                }
            }
            if (n_tab_groups < 2) {
                // not enough tab groups to merge
                cb(false);
                return;
            }
            cb(true);
        });
    });
}

static void
append_open_windows_section(MainWindow* current_win, GMenu* window_submenu)
{
    std::vector<orion::UiWindowId> ui_window_ids = global_core_api->get_registered_ui_window_ids();
    std::vector<orion::WindowId> window_ids;

    for (const auto& ui_window_id : ui_window_ids) {
        MainWindow* main_window = MAIN_WINDOW(ui_window_id.value);
        orion::WindowId window_id = main_window_get_window_id(main_window);
        window_ids.push_back(window_id);
    }

    GMenu* section = g_menu_new();
    defer([section]{ g_object_unref(section); });

    g_menu_append_section(window_submenu, NULL, G_MENU_MODEL(section));

    orion::gtk::WeakRefWrapper section_weak(section);
    orion::gtk::WeakRefWrapper current_win_weak(current_win);

    orion::WindowId current_win_id = main_window_get_window_id(current_win);

    global_core_api->get_windows(window_ids, [current_win_id,section_weak,current_win_weak](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);

        std::vector<orion::WindowDto> windows = reply.windows;
        std::vector<orion::TabId> tab_ids;
        
        for (const auto& window : windows) {
            tab_ids.push_back(window.selected_tab_id);
        }

        // display current window first
        std::stable_partition(windows.begin(), windows.end(),
            [current_win_id](const orion::WindowDto& window){
                return window.id == current_win_id;
            }
        );

        global_core_api->get_tabs(tab_ids, [section_weak,current_win_weak,windows,current_win_id](orion::GetTabsReply reply){
            VERIFY_LOG(reply.err);

            auto section_strong = section_weak.lock();
            if (!section_strong) return;
            GMenu* section = G_MENU(section_strong.get());

            auto current_win_strong = current_win_weak.lock();
            if (!current_win_strong) return;
            MainWindow* current_win = MAIN_WINDOW(current_win_strong.get());

            std::vector<orion::TabDto> tabs = reply.tabs;

            for (const auto& window : windows) {
                std::string name;
                if (window.is_tab_group && !window.is_private) {
                    name = orion::text::ngettext_string("%" PRId64 " Tab", "%" PRId64 " Tabs", window.num_tabs);
                } else {
                    // Saved Tab Group or Private Tab Group
                    name = window.name;
                }

                // append selected tab title to each window item
                auto it = std::find_if(tabs.begin(), tabs.end(),
                    [window](const orion::TabDto& tab){
                        return tab.id == window.selected_tab_id;
                    }
                );
                if (it != tabs.end()) {
                    std::string tab_title = it->name;
                    if (!tab_title.empty()) {
                        name += " - " + tab_title;
                    }
                }

                std::string ellipsized = orion::text::ellipsize_string(name, MAX_CONTEXT_MENU_USER_STRING_LENGTH);
                ellipsized = orion::gtk::append_invisible_unicode_suffix(ellipsized, global_window_counter);
                global_window_counter++;

                orion::gtk::menu_append_item_full_int64_target(section, ellipsized, "win.focus-target-window", window.id.value, "radio");
                
                if (window.id == current_win_id) {
                    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(current_win), "focus-target-window");
                    g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_int64(window.id.value));
                }
            }
        });
    });
}

static void
update_window_submenu(MainWindow* win, GMenu* window_menu)
{
    orion::TabId current_tab_id = main_window_get_current_tab_id(win);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper window_menu_weak(window_menu);
    orion::gtk::WeakRefWrapper win_weak(win);
    orion::WindowId window_id = main_window_get_window_id(win);

    global_core_api->get_tabs({current_tab_id},
        [window_menu_weak,win_weak,profile_id,window_id]
        (orion::GetTabsReply reply){

        VERIFY_LOG(reply.err);

        if (reply.tabs.empty()) return;
        const orion::TabDto& current_tab = reply.tabs.at(0);

        global_core_api->get_setting(profile_id, orion::settings::tabs::tab_layout,
            [window_menu_weak,win_weak,current_tab,window_id]
            (orion::GetSettingReply<orion::settings::TabLayout> reply){

            VERIFY_LOG(reply.err);

            orion::settings::TabLayout tab_layout = reply.value;

            global_core_api->get_descendant_tab_ids(current_tab.id, 
                [window_menu_weak,win_weak,current_tab,tab_layout,window_id]
                (orion::GetDescendantTabIdsReply reply){

                VERIFY_LOG(reply.err);

                auto descendant_tab_ids = reply.descendant_tab_ids;

                global_core_api->get_windows({window_id},
                    [window_menu_weak,win_weak,current_tab,tab_layout,descendant_tab_ids]
                    (orion::GetWindowsReply reply){

                    VERIFY_LOG(reply.err);

                    if (reply.windows.empty()) {
                        orion::log_error("update_window_submenu: current window not found, cannot update Window submenu");
                        return;
                    }
                    const auto& window = reply.windows.at(0);

                    auto win_strong = win_weak.lock();
                    if (!win_strong) return;
                    MainWindow* win = MAIN_WINDOW(win_strong.get());

                    auto window_menu_strong = window_menu_weak.lock();
                    if (!window_menu_strong) return;
                    GMenu* window_menu = G_MENU(window_menu_strong.get());

                    // section that starts with "Show Previous Tab" item
                    g_menu_remove(window_menu, 2);
                    
                    // recreate the section dynamically
                    GMenu* section = g_menu_new();
                    defer([section]{ g_object_unref(section); });

                    orion::gtk::menu_append_item(section, _("Show Previous Tab"), "win.show-previous-tab");
                    orion::gtk::menu_append_item(section, _("Show Next Tab"), "win.show-next-tab");
                    orion::gtk::menu_append_item(section, _("Go to Previous Tab Group"), "win.go-to-previous-tab-group");
                    orion::gtk::menu_append_item(section, _("Go to Next Tab Group"), "win.go-to-next-tab-group");

                    if (!window.is_private) {
                        if (current_tab.is_pinned) {
                            orion::gtk::menu_append_item(section, _("Unpin Tab"), "win.pin-current-tab");
                        } else {
                            orion::gtk::menu_append_item(section, _("Pin Tab"), "win.pin-current-tab");
                        }
                    }

                    orion::gtk::menu_append_item(section, _("Duplicate Tab"), "win.duplicate-current-tab");
                    orion::gtk::menu_append_item(section, _("Suspend Tab"), "win.suspend-current-tab");

                    // Move Tab submenu (disabled if current tab is pinned)
                    if (current_tab.is_pinned) {
                        orion::gtk::menu_append_item(section, _("Move Tab"), "win.disabled");
                    } else {
                        orion::gtk::append_move_tab_submenu(win, current_tab.id, section, orion::gtk::TabType::normal);
                    }

                    OrionWebviewTabContainer* current_tab_container =
                        main_window_get_orion_webview_tab_container_by_tab_id(win, current_tab.id);
                    if (current_tab_container && orion_webview_tab_container_get_is_playing_audio(current_tab_container)) {
                        bool is_muted = orion_webview_tab_container_get_is_muted(current_tab_container);
                        orion::gtk::menu_append_item(section, is_muted ? _("Unmute Tab") : _("Mute Tab"), "win.mute-current-tab");
                    } else {
                        orion::gtk::menu_append_item(section, _("Mute Tab"), "win.disabled");
                    }

                    OtherTabsAudioState other_tabs_audio_state = main_window_get_other_tabs_audio_state(win, current_tab.id);
                    if (other_tabs_audio_state.any_playing) {
                        const char* label = other_tabs_audio_state.any_playing_unmuted ? _("Mute Other Tabs") : _("Unmute Other Tabs");
                        orion::gtk::menu_append_item(section, label, "win.mute-other-tabs-than-current");
                    } else {
                        orion::gtk::menu_append_item(section, _("Mute Other Tabs"), "win.disabled");
                    }

                    orion::gtk::WeakRefWrapper section_weak(section);
                    is_merge_all_windows_item_enabled(win, [section_weak](bool enabled){
                        auto section_strong = section_weak.lock();
                        if (!section_strong) return;
                        GMenu* section = G_MENU(section_strong.get());

                        const char* action = enabled ? "win.merge-all-windows" : "win.disabled";
                        orion::gtk::menu_append_item(section, _("Merge All Windows"), action);
                    });

                    // insert the recreated section back to the Window submenu
                    g_menu_insert_section(window_menu, 2, NULL, G_MENU_MODEL(section));

                    if (tab_layout == orion::settings::TabLayout::vertical && !descendant_tab_ids.empty()) {
                        GMenu* section = g_menu_new();
                        defer([section]{ g_object_unref(section); });

                        orion::gtk::menu_append_item(section, _("Suspend Tree"), "win.suspend-current-tab-tree");

                        // Move Tree submenu
                        orion::gtk::append_move_tab_submenu(win, current_tab.id, section, orion::gtk::TabType::tree);

                        g_menu_insert_section(window_menu, 3, NULL, G_MENU_MODEL(section));
                    }

                    // list of open windows section
                    append_open_windows_section(win, window_menu);
                }); 
            });
        });
    });
}

static void
update_undo_redo_section(MainWindow* win, GtkBuilder* builder)
{
    GMenu* section = G_MENU(gtk_builder_get_object(builder, "undo_redo_section"));
    g_menu_remove_all(section);

    orion::WindowId window_id = main_window_get_window_id(win);

    orion::IsUndoRedoTabStepAvailableQuery query{
        .window_id = window_id,
    };

    orion::gtk::WeakRefWrapper section_weak(section);

    global_core_api->is_undo_redo_tab_step_available(query, [section_weak](orion::IsUndoRedoTabStepAvailableReply reply){
        VERIFY_LOG(reply.err);

        auto section_strong = section_weak.lock();
        if (!section_strong) return;
        GMenu* section = G_MENU(section_strong.get());

        if (reply.is_undo_available) {
            orion::gtk::menu_append_item(section, _("Undo Tab Close"), "win.undo-tab-close");
        } else {
            orion::gtk::menu_append_item(section, _("Undo"), "win.disabled");
        }

        if (reply.is_redo_available) {
            orion::gtk::menu_append_item(section, _("Redo Tab Close"), "win.redo-tab-close");
        } else {
            orion::gtk::menu_append_item(section, _("Redo"), "win.disabled");
        }
    });
}

static GMenuModel*
main_window_create_main_menu(MainWindow* win, GtkBuilder* builder, GtkPopoverMenu* popover_menu)
{
    GMenuModel* main_menu = G_MENU_MODEL(gtk_builder_get_object(builder, "main_menu"));
    main_window_set_main_menu_model(win, main_menu);

    GMenu* bookmarks_submenu = G_MENU(g_menu_model_get_item_link(main_menu, submenu_index_bookmarks, G_MENU_LINK_SUBMENU));
    defer([bookmarks_submenu]{ g_object_unref(bookmarks_submenu); });

    setup_recently_bookmarked_submenu(win, bookmarks_submenu);
    setup_all_bookmarks_submenu(win, bookmarks_submenu);

    GMenu* file_submenu = G_MENU(g_menu_model_get_item_link(main_menu, submenu_index_file, G_MENU_LINK_SUBMENU));
    create_file_submenu(win, file_submenu, popover_menu);

    GMenu* history_submenu = G_MENU(g_menu_model_get_item_link(main_menu, submenu_index_history, G_MENU_LINK_SUBMENU));
    create_history_submenu(win, history_submenu);

    GMenu* window_submenu = G_MENU(g_menu_model_get_item_link(main_menu, submenu_index_window, G_MENU_LINK_SUBMENU));
    update_window_submenu(win, window_submenu);

    update_undo_redo_section(win, builder);

#ifndef NDEBUG
    append_debug_submenu(G_MENU(main_menu), win);
#endif
    return main_menu;
}

static void
main_menu_popover_show(GtkWidget* widget, gpointer user_data)
{
    GtkPopoverMenu* popover_menu = GTK_POPOVER_MENU(widget);
    MainWindow* win = MAIN_WINDOW(user_data);
    orion::log_verbose("menu_button clicked: popover_menu=%p win=%p", popover_menu, win);
    
    GtkBuilder* builder = gtk_builder_new_from_resource(GRESOURCE_PREFIX "/src/window/main_menu/main_menu.ui");
    defer([builder]{ g_object_unref(builder); });

    GMenuModel* main_menu = main_window_create_main_menu(win, builder, popover_menu);
    gtk_popover_menu_set_menu_model(popover_menu, main_menu);
}

void
main_window_setup_main_menu_button(MainWindow* win)
{
    HeaderBar* header_bar = main_window_get_header_bar(win);
    GtkMenuButton* menu_button = header_bar_get_menu_button(header_bar);

    // we have to set some dummy menu model, then we replace it in the "show" signal handler
    GMenu* dummy = g_menu_new();
    defer([dummy]{ g_object_unref(dummy); });

    // When GtkPopoverMenu flags are set to GTK_POPOVER_MENU_NESTED and the menu has submenus (ie., is really nested),
    // it is possible to break its behavior for example:
    // open the menu, left-click submenu entry and hold, drag the mouse of out the menu, release the button.
    // It is possible that now the menu is visible but it cannot be closed by clicking outside of it.
    GtkWidget* popover_menu = gtk_popover_menu_new_from_model_full(G_MENU_MODEL(dummy), GTK_POPOVER_MENU_SLIDING);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu_button), popover_menu);
    
    orion::log_verbose("main_window_setup_main_menu_button: menu_button=%p popover_menu=%p",
        menu_button, popover_menu
    );

    // "activate" signal (on GtkMenuButton) does not work here, it is not emitted when the button is clicked
    g_signal_connect(popover_menu, "show", G_CALLBACK(main_menu_popover_show), win);
}

GListModel*
create_main_menu_list_model()
{
    GtkBuilder* builder = gtk_builder_new_from_resource(GRESOURCE_PREFIX "/src/window/main_menu/main_menu.ui");
    defer([builder]{ g_object_unref(builder); });

    struct ModelStackItem {
        GMenuModel* model;
        int index;
        int n_items;
        bool added_item_to_path;
    };
    GMenuModel* root_model = G_MENU_MODEL(g_object_ref(gtk_builder_get_object(builder, "main_menu")));
    //this vector is essentially a stack of the GMenuModel objects we use to traverse the hierarchy
    std::vector<ModelStackItem> models = {{root_model, 0, g_menu_model_get_n_items(root_model), false}};
    //this is our path we pass to the data items, maintained alongside the models stack
    GStrv path = g_new0(char*, 1);
    size_t path_length = 0;

    GListStore* store = g_list_store_new(APP_TYPE_MENU_ITEM);
    while(models.size()) //loop control logic largely handled at the end of the iteration, also model index increments/decrements
    {
        if(models.back().index == models.back().n_items)
        {
            if(models.back().added_item_to_path)
            {
                path = static_cast<GStrv>(g_realloc(path, sizeof(char*)*(path_length)));
                g_free(path[path_length-1]);
                path[path_length-1] = NULL;
                path_length--;
            }
            g_object_unref(models.back().model);
            models.pop_back();
            continue;
        }

        gchar* label = NULL;
        g_menu_model_get_item_attribute(models.back().model, models.back().index, G_MENU_ATTRIBUTE_LABEL, "s", &label);
        GMenuModel* submenu = g_menu_model_get_item_link(models.back().model, models.back().index, G_MENU_LINK_SUBMENU);
        GMenuModel* section = g_menu_model_get_item_link(models.back().model, models.back().index, G_MENU_LINK_SECTION);
        
        GMenuModel* child_menu = submenu ? submenu : section;

        //we're guaranteed to have models.size() >= 1 before this runs, meaning this re
        if(child_menu)
        {
            models.back().index++;
            models.push_back({child_menu, 0, g_menu_model_get_n_items(child_menu), label ? true : false});

            if(label)
            {
                path_length++;
                path = static_cast<GStrv>(g_realloc(path, sizeof(char*)*(path_length+1)));
                path[path_length-1] = label;
                path[path_length] = NULL;
            }
        }
        else
        {
            gchar* action = NULL;
            g_menu_model_get_item_attribute(models.back().model, models.back().index, G_MENU_ATTRIBUTE_ACTION, "s", &action);
            gchar* target = NULL;
            g_menu_model_get_item_attribute(models.back().model, models.back().index, G_MENU_ATTRIBUTE_TARGET, "s", &target);
            gpointer item = g_object_new(APP_TYPE_MENU_ITEM, "label", label, "path", path, "action", action, "target", target, NULL);
            g_list_store_append(store, item);
            g_object_unref(item);
            g_clear_pointer(&label, g_free);
            g_clear_pointer(&action, g_free);
            g_clear_pointer(&target, g_free);
            models.back().index++;
        }
    }
    g_free(path);

    return G_LIST_MODEL(store);
}
