#define ENABLE_VERBOSE_LOGS 0

#include <core/profiles_db/test.h>
#include <core/filter_list_metadata.h>
#include <common/src/error.h>

#include <algorithm>
#include <memory>

namespace orion {

Error
test_db(std::shared_ptr<Db> db)
{
    DbStmt stmt;
    VERIFY(db->query("SELECT 1 AS number, '2' AS name, 3.14 AS pi, ? AS custom FROM sqlite_master WHERE type='table';", {42.7}, &stmt));

    while (stmt.next()) {
        log_verbose("number:%lld name:'%s' pi:%f custom:%s custom_variant:%s",
            stmt.integer(0),
            stmt.text("name").c_str(),
            stmt.real("pi"),
            stmt.blob_optional("custom").value_or(std::vector<uint8_t>()).empty() ? "NULL" : "BLOB",
            stmt.variant("custom").to_string().c_str()
        );
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
test_profiles_db_internal(std::shared_ptr<ProfilesDb> db)
{
    // Clean up leftover test profiles from previous runs that failed
    // before reaching cleanup.
    {
        std::vector<ProfileId> ids;
        VERIFY(db->get_profile_ids(&ids));
        std::vector<ProfileDto> profiles;
        VERIFY(db->get_profiles(ids, &profiles));
        std::vector<ProfileId> to_delete;
        for (const auto& p : profiles) {
            // TODO: We could remove the p.name check later, but so far test profiles have been created without is_test.
            // Basically, if you see this many months later, you can probably remove the p.name check.
            if (p.is_test || p.name == "profiles_db_test: Test Profile") {
                to_delete.push_back(p.id);
            }
        }
        if (!to_delete.empty()) {
            VERIFY(db->delete_profiles(to_delete, nullptr, nullptr, nullptr));
        }
    }

    ProfileId profile_id;
    {
        CreateProfileQuery query{
            .name = "profiles_db_test: Test Profile",
            .icon_name = "test_icon",
            .keyboard_shortcut = "Ctrl+Alt+F1",
            .is_test = true,
        };
        
        VERIFY(db->create_profile(query, &profile_id, nullptr, nullptr, nullptr, nullptr, nullptr));
        log_verbose("created profile %lld", profile_id.value);
    }

    // Removing obsolete builtins must not affect accounted builtins or user-provided lists.
    {
        std::vector<std::string> accounted_builtin_names;
        for (const auto& metadata : filter_lists::get_metadata_list()) {
            if (metadata.filter_type == FilterListType::builtin) {
                accounted_builtin_names.push_back(metadata.name);
            }
        }

        CreateFilterListQuery query{
            .profile_id = profile_id,
            .name = "profiles_db_test: accounted builtin",
            .filter_type = FilterListType::builtin,
            .content_hash = "4a44dc15364204a80fe80e9039455cc1608281820fe2b24e6c98e4a1f5b2f9b1",
        };
        FilterListsChange create_change;
        FilterListId accounted_builtin_id{};
        VERIFY(db->create_or_update_filter_list(
            query, &accounted_builtin_id, &create_change));
        RAISE_CHECK(create_change.created.size() == 1);
        RAISE_CHECK(create_change.created[0].id == accounted_builtin_id);
        RAISE_CHECK(create_change.created[0].name == query.name);
        RAISE_CHECK(create_change.created[0].content_hash == query.content_hash);

        query.is_enabled = true;
        query.update_is_enabled = true;
        query.content_hash = std::string(64, 'b');
        query.update_content_hash = true;
        FilterListsChange update_change;
        VERIFY(db->create_or_update_filter_list(query, nullptr, &update_change));
        RAISE_CHECK(update_change.updated.size() == 1);
        RAISE_CHECK(update_change.updated[0].id == accounted_builtin_id);
        RAISE_CHECK(update_change.updated[0].is_enabled);
        RAISE_CHECK(update_change.updated[0].content_hash == query.content_hash);
        accounted_builtin_names.push_back(query.name);

        query.name = "profiles_db_test: unknown builtin";
        FilterListId unknown_builtin_id{};
        VERIFY(db->create_or_update_filter_list(query, &unknown_builtin_id, nullptr));

        query.name = "profiles_db_test: unknown user";
        query.filter_type = FilterListType::user;
        VERIFY(db->create_or_update_filter_list(query, nullptr, nullptr));

        FilterListsChange delete_change;
        VERIFY(db->delete_unknown_builtin_filter_lists(accounted_builtin_names, &delete_change));
        RAISE_CHECK(delete_change.deleted.size() == 1);
        RAISE_CHECK(delete_change.deleted[0] == unknown_builtin_id);

        std::vector<FilterListDto> filter_lists;
        VERIFY(db->get_filter_lists({.profile_id = profile_id}, &filter_lists));
        const auto contains_filter = [&filter_lists](const std::string& name) {
            return std::ranges::any_of(filter_lists, [&name](const FilterListDto& filter_list) {
                return filter_list.name == name;
            });
        };
        RAISE_CHECK(contains_filter("profiles_db_test: accounted builtin"));
        RAISE_CHECK(!contains_filter("profiles_db_test: unknown builtin"));
        RAISE_CHECK(contains_filter("profiles_db_test: unknown user"));
    }

    // [Root Folder]
    //   - [Folder F1]
    //       - [Bookmark B2]
    //   - [Bookmark B1]
    //   - [Folder F2]

    CreateBookmarkFolderQuery query{};
    query.profile_id = profile_id;
    query.name = "profiles_db_test: Root Folder";
    query.parent_folder_id = BookmarkFolderId(0); // create at the root level
    BookmarkFolderId root_folder_id;
    VERIFY(db->create_bookmark_folder(query, &root_folder_id, nullptr));
    
    BookmarkFolderId f1_id{};
    BookmarkFolderId f2_id{};
    BookmarkId b1_id{};
    BookmarkId b2_id{};

    {
        CreateBookmarkFolderQuery query{};
        query.profile_id = profile_id;
        query.name = "profiles_db_test: Folder F1";
        query.parent_folder_id = root_folder_id; // create under the root folder
        VERIFY(db->create_bookmark_folder(query, &f1_id, nullptr));
    }
    {
        CreateBookmarkQuery query{};
        query.profile_id = profile_id;
        query.name = "profiles_db_test: Bookmark B1";
        query.address = "https://example.com/b1";
        query.parent_folder_id = root_folder_id;
        VERIFY(db->create_bookmark(query, &b1_id, nullptr));
    }
    {
        CreateBookmarkQuery query{};
        query.profile_id = profile_id;
        query.name = "profiles_db_test: Bookmark B2";
        query.address = "https://example.com/b2";
        query.parent_folder_id = f1_id;
        VERIFY(db->create_bookmark(query, &b2_id, nullptr));
    }
    {
        CreateBookmarkFolderQuery query{};
        query.profile_id = profile_id;
        query.name = "profiles_db_test: Folder F2";
        query.parent_folder_id = root_folder_id; // create under the root folder
        VERIFY(db->create_bookmark_folder(query, &f2_id, nullptr));
    }
    {
        // Argument test
        CopyBookmarksQuery query{};
        query.source_bookmark_ids = {BookmarkId(1)}; // dummy id
        query.target_bookmark_id = BookmarkId(2); // dummy id
        query.target_folder_id = BookmarkFolderId(2); // dummy id
        Error err = db->copy_bookmarks(query, nullptr, nullptr, nullptr);
        RAISE_CHECK(err.code() == ErrorCode::argument
            && err.message() == "either target_folder_id or target_bookmark_id must be set, not both"
        );
    }
    {
        // Copy bookmarks test
        CopyBookmarksQuery query{};
        query.source_folder_ids = {root_folder_id, f1_id, f2_id};
        query.source_bookmark_ids = {b1_id};
        query.target_folder_id = root_folder_id;
        CopyBookmarksReply reply{};
        BookmarkFoldersChange folder_change{};
        BookmarksChange bookmark_change{};
        VERIFY(db->copy_bookmarks(query, &reply, &folder_change, &bookmark_change));
        RAISE_CHECK(folder_change.created.size() == 3);
        RAISE_CHECK(bookmark_change.created.size() == 2);
    }
    {
        // Tab test
        CreateWindowQuery window_query{};
        window_query.profile_id = profile_id;
        window_query.name = "profiles_db_test: Test Window";
        window_query.is_tab_group = false;
        window_query.seq_num = 1.0;
        WindowId window_id;
        VERIFY(db->create_window(window_query, &window_id, nullptr));

        TabId tab1_id{};
        {
            CreateTabQuery tab_query{};
            TabsChange tab_change{};
            WindowsChange window_change{};
            tab_query.window_id = window_id;
            tab_query.name = "profiles_db_test: Test Tab 1";
            tab_query.uri = "https://example.com/tab1";
            tab_query.icon_name = "tab1_icon";
            tab_query.is_pinned = true;
            tab_query.is_suspended = true;
            tab_query.seq_num = 1.0;
            VERIFY(db->create_tab(tab_query, &tab1_id, TxMode::use_tx, &tab_change, &window_change));
            RAISE_CHECK(tab_change.created.size() == 1);
            RAISE_CHECK(tab_change.created[0].index == 0);
            RAISE_CHECK(window_change.updated.size() == 1);
            RAISE_CHECK(window_change.updated[0].id == window_id);
            RAISE_CHECK(window_change.updated[0].num_tabs == 1);
        }

        TabId tab2_id{};
        {
            CreateTabQuery tab_query{};
            TabsChange tab_change{};
            WindowsChange window_change{};
            tab_query.window_id = window_id;
            tab_query.name = "profiles_db_test: Test Tab 2";
            tab_query.uri = "https://example.com/tab2";
            tab_query.icon_name = "tab2_icon";
            tab_query.is_pinned = false;
            tab_query.is_suspended = false;
            tab_query.seq_num = 2.0;
            VERIFY(db->create_tab(tab_query, &tab2_id, TxMode::use_tx, &tab_change, &window_change));
            RAISE_CHECK(tab_change.created.size() == 1);
            RAISE_CHECK(tab_change.created[0].index == 1);
            RAISE_CHECK(window_change.updated.size() == 1);
            RAISE_CHECK(window_change.updated[0].id == window_id);
            RAISE_CHECK(window_change.updated[0].num_tabs == 2);
        }

        TabDto dup_tab{};
        {
            TabsChange change{};
            WindowsChange window_change{};
            DuplicateTabQuery dup_query{};
            dup_query.id = tab1_id;
            VERIFY(db->duplicate_tab(dup_query, &dup_tab, &change, &window_change));
            RAISE_CHECK(dup_tab.id != TabId(0));
            RAISE_CHECK(dup_tab.id != tab1_id);
            RAISE_CHECK(dup_tab.id != tab2_id);
            RAISE_CHECK(dup_tab.window_id == window_id);
            RAISE_CHECK(dup_tab.name == "profiles_db_test: Test Tab 1");
            RAISE_CHECK(dup_tab.uri == "https://example.com/tab1");
            RAISE_CHECK(dup_tab.icon_name == "tab1_icon");
            RAISE_CHECK(dup_tab.is_pinned);
            RAISE_CHECK(dup_tab.is_suspended);
            RAISE_CHECK(dup_tab.seq_num == 1.5); // should be placed between tab1 and tab2
            RAISE_CHECK(dup_tab.index == 1);
            RAISE_CHECK(change.created.size() == 1);
            RAISE_CHECK(change.created[0].id.value == dup_tab.id.value);
            RAISE_CHECK(window_change.updated.size() == 1);
            RAISE_CHECK(window_change.updated[0].id == window_id);
            RAISE_CHECK(window_change.updated[0].num_tabs == 3);
        }

        {
            GetTabIdsQuery query{};
            query.window_id = window_id;
            std::vector<TabId> tab_ids;
            VERIFY(db->get_tab_ids(query, &tab_ids));
            RAISE_CHECK(tab_ids.size() == 3);
            RAISE_CHECK(tab_ids[0] == tab1_id);
            RAISE_CHECK(tab_ids[1] == dup_tab.id);
            RAISE_CHECK(tab_ids[2] == tab2_id);
        }
        {
            GetOtherTabIdsQuery query{};
            query.exclude_tab_id = dup_tab.id;
            std::vector<TabId> other_ids;
            std::vector<TabId> before_ids;
            std::vector<TabId> after_ids;
            VERIFY(db->get_other_tab_ids(query, &other_ids, &before_ids, &after_ids));
            RAISE_CHECK(other_ids.size() == 2);
            RAISE_CHECK(other_ids[0] == tab1_id);
            RAISE_CHECK(other_ids[1] == tab2_id);
            RAISE_CHECK(before_ids.size() == 1);
            RAISE_CHECK(before_ids[0] == tab1_id);
            RAISE_CHECK(after_ids.size() == 1);
            RAISE_CHECK(after_ids[0] == tab2_id);
        }
    }
    {
        ReadingListEntryId rle1_id{};
        {
            CreateReadingListEntryQuery query{};
            query.profile_id = profile_id;
            query.title = "profiles_db_test: Reading List Entry 1";
            query.address = "https://example.com/rle1";
            query.added_on = UnixTimeUtc64(1000);
            VERIFY(db->create_reading_list_entry(query, &rle1_id, nullptr));
        }

        ReadingListEntryId rle2_id{};
        {
            CreateReadingListEntryQuery query{};
            query.profile_id = profile_id;
            query.title = "profiles_db_test: Reading List Entry 2";
            query.address = "https://example.com/rle2";
            query.added_on = UnixTimeUtc64(2000);
            VERIFY(db->create_reading_list_entry(query, &rle2_id, nullptr));
        }

        ReadingListEntryId rle3_id{};
        {
            CreateReadingListEntryQuery query{};
            query.profile_id = profile_id;
            query.title = "profiles_db_test: Reading List Entry 3";
            query.address = "https://example.com/rle3";
            query.added_on = UnixTimeUtc64(3000);
            VERIFY(db->create_reading_list_entry(query, &rle3_id, nullptr));
        }
        {
            GetReadingListEntryIdsQuery query{};
            query.profile_id = profile_id;
            query.added_before = UnixTimeUtc64(9999);
            std::vector<ReadingListEntryId> ids;
            VERIFY(db->get_reading_list_entry_ids(query, &ids));
            RAISE_CHECK(ids.size() == 3);
            RAISE_CHECK(std::find(ids.begin(), ids.end(), rle1_id) != ids.end());
            RAISE_CHECK(std::find(ids.begin(), ids.end(), rle2_id) != ids.end());
            RAISE_CHECK(std::find(ids.begin(), ids.end(), rle3_id) != ids.end());
        }
        {
            GetReadingListEntryIdsQuery query{};
            query.profile_id = profile_id;
            query.added_before = UnixTimeUtc64(9999);
            query.search_filter = "rle2";
            std::vector<ReadingListEntryId> ids;
            VERIFY(db->get_reading_list_entry_ids(query, &ids));
            RAISE_CHECK(ids.size() == 1);
            RAISE_CHECK(ids[0] == rle2_id);
        }
    }
    {
        // Tab back-forward list entry test
        CreateWindowQuery window_query{};
        window_query.profile_id = profile_id;
        window_query.name = "profiles_db_test: Test Window for Tab BFL";
        window_query.is_tab_group = false;
        WindowId window_id;
        VERIFY(db->create_window(window_query, &window_id, nullptr));

        TabId tab1_id{};
        {
            CreateTabQuery tab_query{};
            tab_query.window_id = window_id;
            tab_query.name = "profiles_db_test: Test Tab for Tab BFL 1";
            tab_query.uri = "https://example.com/tab1";
            WindowsChange window_change{};
            VERIFY(db->create_tab(tab_query, &tab1_id, TxMode::use_tx, nullptr, &window_change));
            RAISE_CHECK(window_change.updated.size() == 1);
            RAISE_CHECK(window_change.updated[0].id == window_id);
            RAISE_CHECK(window_change.updated[0].num_tabs == 1);
        }

        TabBackForwardListEntryDto entry{};
        {
            CreateTabBackForwardListEntryQuery query{};
            query.tab_id = tab1_id;
            query.uri = "https://example.com/bfl1";
            query.webkit_index = 1;
            VERIFY(db->create_tab_back_forward_list_entry(query, &entry, nullptr));
            RAISE_CHECK(entry.id != TabBackForwardListEntryId(0));
            RAISE_CHECK(entry.tab_id == tab1_id);
            RAISE_CHECK(entry.uri == "https://example.com/bfl1");
            RAISE_CHECK(entry.webkit_index == 1);
        }
        {
            std::vector<TabBackForwardListEntryDto> entries;
            VERIFY(db->get_tab_back_forward_list_entries(tab1_id, &entries));
            RAISE_CHECK(entries.size() == 1);
            RAISE_CHECK(entries[0].id == entry.id);
            RAISE_CHECK(entries[0].tab_id == tab1_id);
            RAISE_CHECK(entries[0].uri == "https://example.com/bfl1");
            RAISE_CHECK(entries[0].webkit_index == 1);
        }
        {
            DeleteTabBackForwardListEntriesQuery query{};
            query.tab_id = tab1_id;
            query.start_webkit_index = 1;
            TabBackForwardListEntriesChange change{};
            VERIFY(db->delete_tab_back_forward_list_entries(query, &change));
            RAISE_CHECK(change.deleted.size() == 1);
            RAISE_CHECK(change.deleted[0].id == entry.id);
        }
        {
            std::vector<TabBackForwardListEntryDto> entries;
            VERIFY(db->get_tab_back_forward_list_entries(tab1_id, &entries));
            RAISE_CHECK(entries.empty());
        }
    }
    {
        // Overflow menu item test
        OverflowMenuItemId item_1_id{};
        {
            // Drop all existing default items
            {
                std::vector<OverflowMenuItemDto> items;
                VERIFY(db->get_overflow_menu_items({profile_id}, &items));
                std::vector<OverflowMenuItemId> ids;
                for (const auto& item : items) {
                    ids.push_back(item.id);
                }
                db->delete_overflow_menu_items(ids, TxMode::no_tx, nullptr);
            }

            CreateOverflowMenuItemQuery query{
                .profile_id = profile_id,
                .action = "test_action_1",
                .command_id = 101,
                .idx = -1,
            };
            OverflowMenuItemsChange change{};
            VERIFY(db->create_overflow_menu_item(query, TxMode::no_tx, &item_1_id, &change));
            RAISE_CHECK(item_1_id.value != 0);
            RAISE_CHECK(change.created.size() == 1);
            RAISE_CHECK(change.created[0].id == item_1_id);
            RAISE_CHECK(change.created[0].profile_id == profile_id);
            RAISE_CHECK(change.created[0].action == "test_action_1");
            RAISE_CHECK(change.created[0].command_id == 101);
            RAISE_CHECK(change.created[0].idx == 0);
            RAISE_CHECK(change.updated.empty());
        }
        OverflowMenuItemId item_2_id{};
        {
            CreateOverflowMenuItemQuery query{
                .profile_id = profile_id,
                .action = "test_action_2",
                .command_id = 102,
                .idx = -1, // should be appended to the end of the list
            };
            OverflowMenuItemsChange change{};
            VERIFY(db->create_overflow_menu_item(query, TxMode::no_tx, &item_2_id, &change));
            RAISE_CHECK(item_2_id.value != 0);
            RAISE_CHECK(change.created.size() == 1);
            RAISE_CHECK(change.created[0].id == item_2_id);
            RAISE_CHECK(change.created[0].profile_id == profile_id);
            RAISE_CHECK(change.created[0].action == "test_action_2");
            RAISE_CHECK(change.created[0].command_id == 102);
            RAISE_CHECK(change.created[0].idx == 1);
            RAISE_CHECK(change.updated.empty());
        }
        OverflowMenuItemId item_3_id{};
        {
            CreateOverflowMenuItemQuery query{
                .profile_id = profile_id,
                .action = "test_action_3",
                .command_id = 103,
                .idx = 666, // should be appended to the end of the list
            };
            OverflowMenuItemsChange change{};
            VERIFY(db->create_overflow_menu_item(query, TxMode::no_tx, &item_3_id, &change));
            RAISE_CHECK(item_3_id.value != 0);
            RAISE_CHECK(change.created.size() == 1);
            RAISE_CHECK(change.created[0].id == item_3_id);
            RAISE_CHECK(change.created[0].profile_id == profile_id);
            RAISE_CHECK(change.created[0].action == "test_action_3");
            RAISE_CHECK(change.created[0].command_id == 103);
            RAISE_CHECK(change.created[0].idx == 2);
            RAISE_CHECK(change.updated.empty());
        }
        OverflowMenuItemId item_2b_id{};
        {
            CreateOverflowMenuItemQuery query{
                .profile_id = profile_id,
                .action = "test_action_2b",
                .command_id = 104,
                .idx = 0, // should be inserted at the beginning, and other items should be shifted
            };
            OverflowMenuItemsChange change{};
            VERIFY(db->create_overflow_menu_item(query, TxMode::no_tx, &item_2b_id, &change));
            RAISE_CHECK(item_2b_id.value != 0);
            RAISE_CHECK(change.created.size() == 1);
            RAISE_CHECK(change.created[0].id == item_2b_id);
            RAISE_CHECK(change.created[0].profile_id == profile_id);
            RAISE_CHECK(change.created[0].action == "test_action_2b");
            RAISE_CHECK(change.created[0].command_id == 104);
            RAISE_CHECK(change.created[0].idx == 0);
            RAISE_CHECK(change.updated.size() == 3);
            RAISE_CHECK(change.updated[0].id.value != 0);
            RAISE_CHECK(change.updated[0].profile_id == profile_id);
            RAISE_CHECK(change.updated[0].action == "test_action_1");
            RAISE_CHECK(change.updated[0].command_id == 101);
            RAISE_CHECK(change.updated[0].idx == 1);
            RAISE_CHECK(change.updated[1].id.value != 0);
            RAISE_CHECK(change.updated[1].profile_id == profile_id);
            RAISE_CHECK(change.updated[1].action == "test_action_2");
            RAISE_CHECK(change.updated[1].command_id == 102);
            RAISE_CHECK(change.updated[1].idx == 2);
            RAISE_CHECK(change.updated[2].id.value != 0);
            RAISE_CHECK(change.updated[2].profile_id == profile_id);
            RAISE_CHECK(change.updated[2].action == "test_action_3");
            RAISE_CHECK(change.updated[2].command_id == 103);
            RAISE_CHECK(change.updated[2].idx == 3);
        }
        {
            std::vector<OverflowMenuItemDto> items;
            GetOverflowMenuItemsQuery query{
                .profile_id = profile_id,
            };
            VERIFY(db->get_overflow_menu_items(query, &items));
            RAISE_CHECK(items.size() == 4);
            RAISE_CHECK(items[0].id.value != 0);
            RAISE_CHECK(items[0].profile_id == profile_id);
            RAISE_CHECK(items[0].action == "test_action_2b");
            RAISE_CHECK(items[0].command_id == 104);
            RAISE_CHECK(items[0].idx == 0);
            RAISE_CHECK(items[1].id.value != 0);
            RAISE_CHECK(items[1].profile_id == profile_id);
            RAISE_CHECK(items[1].action == "test_action_1");
            RAISE_CHECK(items[1].command_id == 101);
            RAISE_CHECK(items[1].idx == 1);
            RAISE_CHECK(items[2].id.value != 0);
            RAISE_CHECK(items[2].profile_id == profile_id);
            RAISE_CHECK(items[2].action == "test_action_2");
            RAISE_CHECK(items[2].command_id == 102);
            RAISE_CHECK(items[2].idx == 2);
            RAISE_CHECK(items[3].id.value != 0);
            RAISE_CHECK(items[3].profile_id == profile_id);
            RAISE_CHECK(items[3].action == "test_action_3");
            RAISE_CHECK(items[3].command_id == 103);
            RAISE_CHECK(items[3].idx == 3);
        }
        {
            OverflowMenuItemsChange change{};
            VERIFY(db->delete_overflow_menu_items({item_2b_id}, TxMode::no_tx, &change));
             // Verify that the remaining items have been shifted correctly
            std::vector<OverflowMenuItemDto> items;
            GetOverflowMenuItemsQuery query{
                .profile_id = profile_id,
            };
            VERIFY(db->get_overflow_menu_items(query, &items));
            RAISE_CHECK(items.size() == 3);
            RAISE_CHECK(items[0].id.value != 0);
            RAISE_CHECK(items[0].profile_id == profile_id);
            RAISE_CHECK(items[0].action == "test_action_1");
            RAISE_CHECK(items[0].command_id == 101);
            RAISE_CHECK(items[0].idx == 0);
            RAISE_CHECK(items[1].id.value != 0);
            RAISE_CHECK(items[1].profile_id == profile_id);
            RAISE_CHECK(items[1].action == "test_action_2");
            RAISE_CHECK(items[1].command_id == 102);
            RAISE_CHECK(items[1].idx == 1);
            RAISE_CHECK(items[2].id.value != 0);
            RAISE_CHECK(items[2].profile_id == profile_id);
            RAISE_CHECK(items[2].action == "test_action_3");
            RAISE_CHECK(items[2].command_id == 103);
            RAISE_CHECK(items[2].idx == 2);
        }
        {
            OverflowMenuItemsChange change{};
            VERIFY(db->move_overflow_menu_item({.id=item_1_id, .next_id=item_3_id}, TxMode::no_tx, &change));
            // Verify the changes
            RAISE_CHECK(change.updated.size() == 2);
            // Firstly, other items that were between the old and new index should be shifted
            RAISE_CHECK(change.updated[0].id.value != 0);
            RAISE_CHECK(change.updated[0].profile_id == profile_id);
            RAISE_CHECK(change.updated[0].action == "test_action_2");
            RAISE_CHECK(change.updated[0].command_id == 102);
            RAISE_CHECK(change.updated[0].idx == 0);
            // Secondly, the moved item should have the new index
            RAISE_CHECK(change.updated[1].id.value != 0);
            RAISE_CHECK(change.updated[1].profile_id == profile_id);
            RAISE_CHECK(change.updated[1].action == "test_action_1");
            RAISE_CHECK(change.updated[1].command_id == 101);
            RAISE_CHECK(change.updated[1].idx == 1);

            // Verify that the item has been moved correctly
            std::vector<OverflowMenuItemDto> items;
            GetOverflowMenuItemsQuery query{
                .profile_id = profile_id,
            };
            VERIFY(db->get_overflow_menu_items(query, &items));
            RAISE_CHECK(items.size() == 3);
            RAISE_CHECK(items[0].id.value != 0);
            RAISE_CHECK(items[0].profile_id == profile_id);
            RAISE_CHECK(items[0].action == "test_action_2");
            RAISE_CHECK(items[0].command_id == 102);
            RAISE_CHECK(items[0].idx == 0);
            RAISE_CHECK(items[1].id.value != 0);
            RAISE_CHECK(items[1].profile_id == profile_id);
            RAISE_CHECK(items[1].action == "test_action_1");
            RAISE_CHECK(items[1].command_id == 101);
            RAISE_CHECK(items[1].idx == 1);
            RAISE_CHECK(items[2].id.value != 0);
            RAISE_CHECK(items[2].profile_id == profile_id);
            RAISE_CHECK(items[2].action == "test_action_3");
            RAISE_CHECK(items[2].command_id == 103);
            RAISE_CHECK(items[2].idx == 2);
        }
    }

    // Toolbar Item test
    {
        ToolbarItemId item_1_id{};
        {
            // Drop all existing default items
            {
                std::vector<ToolbarItemDto> items;
                GetToolbarItemsQuery query{
                    .profile_id = profile_id,
                    .is_compact_tab_layout = false,
                };
                VERIFY(db->get_toolbar_items(query, &items));
                std::vector<ToolbarItemId> ids;
                for (const auto& item : items) {
                    ids.push_back(item.id);
                }
                db->delete_toolbar_items(ids, TxMode::no_tx, nullptr, nullptr);
            }

            CreateToolbarItemQuery query{
                .profile_id = profile_id,
                .item_type = ToolbarItemType::summarize_page,
                .next_item_id = ToolbarItemId(0), // append to the end
                .in_overflow_menu = false,
            };
            ToolbarItemsChange change{};
            VERIFY(db->create_toolbar_item(query, TxMode::no_tx, &item_1_id, &change, nullptr));
            RAISE_CHECK(item_1_id.value != 0);
            RAISE_CHECK(change.created.size() == 1);
            RAISE_CHECK(change.created[0].id == item_1_id);
            RAISE_CHECK(change.created[0].profile_id == profile_id);
            RAISE_CHECK(change.created[0].item_type == ToolbarItemType::summarize_page);
            RAISE_CHECK(change.created[0].position == 0);
            RAISE_CHECK(!change.created[0].in_overflow_menu);
            RAISE_CHECK(change.updated.empty());
        }
        ToolbarItemId item_2_id{};
        {
            CreateToolbarItemQuery query{
                .profile_id = profile_id,
                .item_type = ToolbarItemType::programmable_button,
                .next_item_id = ToolbarItemId(0), // append to the end
                .in_overflow_menu = false,
            };
            ToolbarItemsChange change{};
            ProgrammableButtonsChange prog_buttons_change{};
            VERIFY(db->create_toolbar_item(query, TxMode::no_tx, &item_2_id, &change, &prog_buttons_change));
            RAISE_CHECK(item_2_id.value != 0);
            RAISE_CHECK(change.created.size() == 1);
            RAISE_CHECK(change.created[0].id == item_2_id);
            RAISE_CHECK(change.created[0].profile_id == profile_id);
            RAISE_CHECK(change.created[0].item_type == ToolbarItemType::programmable_button);
            RAISE_CHECK(change.created[0].position == 1);
            RAISE_CHECK(!change.created[0].in_overflow_menu);
            RAISE_CHECK(change.updated.empty());
            RAISE_CHECK(prog_buttons_change.created.size() == 1);
            RAISE_CHECK(prog_buttons_change.created.at(0).id.value() == PROGRAMMABLE_BUTTON_ID(item_2_id));
            RAISE_CHECK(prog_buttons_change.created.at(0).profile_id.value() == profile_id);
            RAISE_CHECK(prog_buttons_change.updated.empty());
            RAISE_CHECK(prog_buttons_change.deleted.empty());
        }
        ToolbarItemId item_3_id{};
        {
            CreateToolbarItemQuery query{
                .profile_id = profile_id,
                .item_type = ToolbarItemType::privacy,
                .next_item_id = ToolbarItemId(0), // append to the end
                .in_overflow_menu = false,
            };
            ToolbarItemsChange change{};
            ProgrammableButtonsChange prog_buttons_change{};
            VERIFY(db->create_toolbar_item(query, TxMode::no_tx, &item_3_id, &change, &prog_buttons_change));
            RAISE_CHECK(item_3_id.value != 0);
            RAISE_CHECK(change.created.size() == 1);
            RAISE_CHECK(change.created[0].id == item_3_id);
            RAISE_CHECK(change.created[0].profile_id == profile_id);
            RAISE_CHECK(change.created[0].item_type == ToolbarItemType::privacy);
            RAISE_CHECK(change.created[0].position == 2);
            RAISE_CHECK(!change.created[0].in_overflow_menu);
            RAISE_CHECK(change.updated.empty());
            RAISE_CHECK(prog_buttons_change.created.empty());
            RAISE_CHECK(prog_buttons_change.updated.empty());
            RAISE_CHECK(prog_buttons_change.deleted.empty());
        }
        ToolbarItemId item_2b_id{};
        {
            CreateToolbarItemQuery query{
                .profile_id = profile_id,
                .item_type = ToolbarItemType::toggle_reader, // later test relies on this not being a programmable button
                .next_item_id = item_1_id, // should be inserted before item_1_id, and other items should be shifted
                .in_overflow_menu = true,
            };
            ToolbarItemsChange change{};
            ProgrammableButtonsChange prog_buttons_change{};
            VERIFY(db->create_toolbar_item(query, TxMode::no_tx, &item_2b_id, &change, &prog_buttons_change));
            RAISE_CHECK(item_2b_id.value != 0);
            RAISE_CHECK(change.created.size() == 1);
            RAISE_CHECK(change.created[0].id == item_2b_id);
            RAISE_CHECK(change.created[0].profile_id == profile_id);
            RAISE_CHECK(change.created[0].item_type == ToolbarItemType::toggle_reader);
            RAISE_CHECK(change.created[0].in_overflow_menu);
            RAISE_CHECK(change.updated.size() == 3);
            RAISE_CHECK(change.updated[0].id == item_1_id);
            RAISE_CHECK(change.updated[0].profile_id == profile_id);
            RAISE_CHECK(change.updated[0].item_type == ToolbarItemType::summarize_page);
            RAISE_CHECK(!change.updated[0].in_overflow_menu);
            RAISE_CHECK(change.updated[0].position == 1);
            RAISE_CHECK(change.updated[1].id == item_2_id);
            RAISE_CHECK(change.updated[1].profile_id == profile_id);
            RAISE_CHECK(change.updated[1].item_type == ToolbarItemType::programmable_button);
            RAISE_CHECK(!change.updated[1].in_overflow_menu);
            RAISE_CHECK(change.updated[1].position == 2);
            RAISE_CHECK(change.updated[2].id == item_3_id);
            RAISE_CHECK(change.updated[2].profile_id == profile_id);
            RAISE_CHECK(change.updated[2].item_type == ToolbarItemType::privacy);
            RAISE_CHECK(!change.updated[2].in_overflow_menu);
            RAISE_CHECK(change.updated[2].position == 3);
            RAISE_CHECK(prog_buttons_change.created.empty());
            RAISE_CHECK(prog_buttons_change.updated.empty());
            RAISE_CHECK(prog_buttons_change.deleted.empty());
        }
        {
            std::vector<ToolbarItemDto> items;
            GetToolbarItemsQuery query{
                .profile_id = profile_id,
            };
            VERIFY(db->get_toolbar_items(query, &items));
            RAISE_CHECK(items.size() == 4);
            RAISE_CHECK(items[0].id == item_2b_id);
            RAISE_CHECK(items[0].profile_id == profile_id);
            RAISE_CHECK(items[0].item_type == ToolbarItemType::toggle_reader);
            RAISE_CHECK(items[0].in_overflow_menu);
            RAISE_CHECK(items[0].position == 0);
            RAISE_CHECK(items[1].id == item_1_id);
            RAISE_CHECK(items[1].profile_id == profile_id);
            RAISE_CHECK(items[1].item_type == ToolbarItemType::summarize_page);
            RAISE_CHECK(!items[1].in_overflow_menu);
            RAISE_CHECK(items[1].position == 1);
            RAISE_CHECK(items[2].id == item_2_id);
            RAISE_CHECK(items[2].profile_id == profile_id);
            RAISE_CHECK(items[2].item_type == ToolbarItemType::programmable_button);
            RAISE_CHECK(!items[2].in_overflow_menu);
            RAISE_CHECK(items[2].position == 2);
            RAISE_CHECK(items[3].id == item_3_id);
            RAISE_CHECK(items[3].profile_id == profile_id);
            RAISE_CHECK(items[3].item_type == ToolbarItemType::privacy);
            RAISE_CHECK(!items[3].in_overflow_menu);
            RAISE_CHECK(items[3].position == 3);
        }
        {
            ToolbarItemsChange change{};
            ProgrammableButtonsChange prog_buttons_change{};
            VERIFY(db->delete_toolbar_items({item_2b_id}, TxMode::no_tx, &change, &prog_buttons_change));
             // Verify that the remaining items have been shifted correctly
            std::vector<ToolbarItemDto> items;
            GetToolbarItemsQuery query{
                .profile_id = profile_id,
            };
            VERIFY(db->get_toolbar_items(query, &items));
            RAISE_CHECK(items.size() == 3);
            RAISE_CHECK(items[0].id == item_1_id);
            RAISE_CHECK(items[0].profile_id == profile_id);
            RAISE_CHECK(items[0].item_type == ToolbarItemType::summarize_page);
            RAISE_CHECK(!items[0].in_overflow_menu);
            RAISE_CHECK(items[0].position == 0);
            RAISE_CHECK(items[1].id == item_2_id);
            RAISE_CHECK(items[1].profile_id == profile_id);
            RAISE_CHECK(items[1].item_type == ToolbarItemType::programmable_button);
            RAISE_CHECK(!items[1].in_overflow_menu);
            RAISE_CHECK(items[1].position == 1);
            RAISE_CHECK(items[2].id == item_3_id);
            RAISE_CHECK(items[2].profile_id == profile_id);
            RAISE_CHECK(items[2].item_type == ToolbarItemType::privacy);
            RAISE_CHECK(!items[2].in_overflow_menu);
            RAISE_CHECK(items[2].position == 2);
            RAISE_CHECK(prog_buttons_change.created.empty());
            RAISE_CHECK(prog_buttons_change.updated.empty());
            RAISE_CHECK(prog_buttons_change.deleted.empty());
        }
        {
            // Move item down the list
            ToolbarItemsChange change{};
            VERIFY(db->move_toolbar_item({.id=item_1_id, .next_item_id=item_3_id}, TxMode::no_tx, &change));
            // Verify the changes
            RAISE_CHECK(change.updated.size() == 2);
            // Firstly, other items that were between the old and new index should be shifted
            RAISE_CHECK(change.updated[0].id == item_2_id);
            RAISE_CHECK(change.updated[0].profile_id == profile_id);
            RAISE_CHECK(change.updated[0].item_type == ToolbarItemType::programmable_button);
            RAISE_CHECK(change.updated[0].position == 0);
            RAISE_CHECK(!change.updated[0].in_overflow_menu);
            // Secondly, the moved item should have the new index
            RAISE_CHECK(change.updated[1].id == item_1_id);
            RAISE_CHECK(change.updated[1].profile_id == profile_id);
            RAISE_CHECK(change.updated[1].item_type == ToolbarItemType::summarize_page);
            RAISE_CHECK(change.updated[1].position == 1);
            RAISE_CHECK(!change.updated[1].in_overflow_menu);

            // Verify that the item has been moved correctly
            std::vector<ToolbarItemDto> items;
            GetToolbarItemsQuery query{
                .profile_id = profile_id,
            };
            VERIFY(db->get_toolbar_items(query, &items));
            RAISE_CHECK(items.size() == 3);
            RAISE_CHECK(items[0].id == item_2_id);
            RAISE_CHECK(items[0].profile_id == profile_id);
            RAISE_CHECK(items[0].item_type == ToolbarItemType::programmable_button);
            RAISE_CHECK(items[0].position == 0);
            RAISE_CHECK(!items[0].in_overflow_menu);
            RAISE_CHECK(items[1].id == item_1_id);
            RAISE_CHECK(items[1].profile_id == profile_id);
            RAISE_CHECK(items[1].item_type == ToolbarItemType::summarize_page);
            RAISE_CHECK(items[1].position == 1);
            RAISE_CHECK(!items[1].in_overflow_menu);
            RAISE_CHECK(items[2].id == item_3_id);
            RAISE_CHECK(items[2].profile_id == profile_id);
            RAISE_CHECK(items[2].item_type == ToolbarItemType::privacy);
            RAISE_CHECK(items[2].position == 2);
            RAISE_CHECK(!items[2].in_overflow_menu);
        }
        {
            // Get Programmable Buttons based on Profile ID check
            GetProgrammableButtonsQuery query{
                .profile_id = profile_id,
                // Programmable Button ID is the same as Toolbar Item ID
                .ids = std::vector<ProgrammableButtonId>{static_cast<ProgrammableButtonId>(item_2_id.value)},
            };
            std::vector<ProgrammableButtonDto> buttons{};
            VERIFY(db->get_programmable_buttons(query, &buttons));
            RAISE_CHECK(buttons.size() == 1);
        }
        {
            //TODO: FIXME: Update Programmable Button check?
        }
        {
            // Update Toolbar Item check
            UpdateToolbarItemQuery query{
                .id = item_2_id,
                .item_type = ToolbarItemType::bookmark, // change item type from programmable_button to bookmark
                .update_item_type = true,
                .in_overflow_menu = true, // change overflow menu status
                .update_in_overflow_menu = true,
            };
            ToolbarItemsChange change{};
            ProgrammableButtonsChange prog_buttons_change{};
            VERIFY(db->update_toolbar_item(query, TxMode::no_tx, &change, &prog_buttons_change));
            RAISE_CHECK(change.updated.size() == 3);
            // Find the new position of the updated item
            auto it = std::find_if(change.updated.begin(), change.updated.end(), [&](const ToolbarItemDto& item) {
                return item.id == item_2_id;
            });
            RAISE_CHECK(it != change.updated.end());
            RAISE_CHECK(it->id == item_2_id);
            RAISE_CHECK(it->profile_id == profile_id);
            RAISE_CHECK(it->item_type == ToolbarItemType::bookmark);
            RAISE_CHECK(it->in_overflow_menu);
            RAISE_CHECK(it->position == 2); // position must change as a side effect
            RAISE_CHECK(prog_buttons_change.created.empty());
            RAISE_CHECK(prog_buttons_change.updated.empty());
            RAISE_CHECK(prog_buttons_change.deleted.size() == 1);
            RAISE_CHECK(prog_buttons_change.deleted.at(0) == PROGRAMMABLE_BUTTON_ID(item_2_id));
        }
        {
             // Verify that the item has been updated correctly
            std::vector<ToolbarItemDto> items;
            GetToolbarItemsQuery get_query{
                .profile_id = profile_id,
            };
            VERIFY(db->get_toolbar_items(get_query, &items));
            RAISE_CHECK(items.size() == 3);
            RAISE_CHECK(items[0].id == item_1_id);
            RAISE_CHECK(items[0].profile_id == profile_id);
            RAISE_CHECK(items[0].item_type == ToolbarItemType::summarize_page);
            RAISE_CHECK(items[0].position == 0);
            RAISE_CHECK(!items[0].in_overflow_menu);
            RAISE_CHECK(items[1].id == item_3_id);
            RAISE_CHECK(items[1].profile_id == profile_id);
            RAISE_CHECK(items[1].item_type == ToolbarItemType::privacy);
            RAISE_CHECK(items[1].position == 1);
            RAISE_CHECK(!items[1].in_overflow_menu);
            RAISE_CHECK(items[2].id == item_2_id);
            RAISE_CHECK(items[2].profile_id == profile_id);
            RAISE_CHECK(items[2].item_type == ToolbarItemType::bookmark);
            RAISE_CHECK(items[2].position == 2);
            RAISE_CHECK(items[2].in_overflow_menu);
        }
        {
            // Move item up the list
            ToolbarItemsChange change{};
            VERIFY(db->move_toolbar_item({.id=item_3_id, .next_item_id=item_1_id}, TxMode::no_tx, &change));
            RAISE_CHECK(change.updated.size() == 2);
            RAISE_CHECK(change.updated[0].id == item_1_id);
            RAISE_CHECK(change.updated[0].profile_id == profile_id);
            RAISE_CHECK(change.updated[0].item_type == ToolbarItemType::summarize_page);
            RAISE_CHECK(change.updated[0].position == 1);
            RAISE_CHECK(!change.updated[0].in_overflow_menu);
            RAISE_CHECK(change.updated[1].id == item_3_id);
            RAISE_CHECK(change.updated[1].profile_id == profile_id);
            RAISE_CHECK(change.updated[1].item_type == ToolbarItemType::privacy);
            RAISE_CHECK(change.updated[1].position == 0);
            RAISE_CHECK(!change.updated[1].in_overflow_menu);
        }
    }

    {
        // move_window test
        WindowId w1_id{}, w2_id{}, w3_id{};
        {
            CreateWindowQuery q{};
            q.profile_id = profile_id;
            q.name = "profiles_db_test: Window W1";
            q.is_tab_group = false;
            q.seq_num = 10.0;
            VERIFY(db->create_window(q, &w1_id, nullptr));
        }
        {
            CreateWindowQuery q{};
            q.profile_id = profile_id;
            q.name = "profiles_db_test: Window W2";
            q.is_tab_group = false;
            q.seq_num = 20.0;
            VERIFY(db->create_window(q, &w2_id, nullptr));
        }
        {
            CreateWindowQuery q{};
            q.profile_id = profile_id;
            q.name = "profiles_db_test: Window W3";
            q.is_tab_group = false;
            q.seq_num = 30.0;
            VERIFY(db->create_window(q, &w3_id, nullptr));
        }

        // Move W3 before W2 (next_window_id = W2)
        {
            MoveWindowQuery q{};
            q.window_id = w3_id;
            q.next_window_id = w2_id;
            WindowsChange change{};
            VERIFY(db->move_window(q, &change));
            RAISE_CHECK(change.updated.size() == 1);
            RAISE_CHECK(change.updated[0].id == w3_id);
            // W3 should now have seq_num between W1 (10.0) and W2 (20.0)
            RAISE_CHECK(change.updated[0].seq_num > 10.0);
            RAISE_CHECK(change.updated[0].seq_num < 20.0);
        }

        // Verify order: W1, W3, W2
        {
            std::vector<WindowDto> windows;
            VERIFY(db->get_windows({w1_id, w2_id, w3_id}, &windows));
            RAISE_CHECK(windows.size() == 3);
            RAISE_CHECK(windows[0].id == w1_id);
            RAISE_CHECK(windows[1].id == w3_id);
            RAISE_CHECK(windows[2].id == w2_id);
        }

        // Move W1 to end (next_window_id = 0)
        {
            MoveWindowQuery q{};
            q.window_id = w1_id;
            q.next_window_id = WindowId(0);
            WindowsChange change{};
            VERIFY(db->move_window(q, &change));
            RAISE_CHECK(change.updated.size() == 1);
            RAISE_CHECK(change.updated[0].id == w1_id);
        }

        // Verify order: W3, W2, W1
        {
            std::vector<WindowDto> windows;
            VERIFY(db->get_windows({w1_id, w2_id, w3_id}, &windows));
            RAISE_CHECK(windows.size() == 3);
            RAISE_CHECK(windows[0].id == w3_id);
            RAISE_CHECK(windows[1].id == w2_id);
            RAISE_CHECK(windows[2].id == w1_id);
        }

        // Move W1 before W3 (first position)
        {
            MoveWindowQuery q{};
            q.window_id = w1_id;
            q.next_window_id = w3_id;
            WindowsChange change{};
            VERIFY(db->move_window(q, &change));
            RAISE_CHECK(change.updated.size() == 1);
            RAISE_CHECK(change.updated[0].id == w1_id);
            RAISE_CHECK(change.updated[0].seq_num < 15.0); // before W3's seq_num
        }

        // Verify order: W1, W3, W2
        {
            std::vector<WindowDto> windows;
            VERIFY(db->get_windows({w1_id, w2_id, w3_id}, &windows));
            RAISE_CHECK(windows.size() == 3);
            RAISE_CHECK(windows[0].id == w1_id);
            RAISE_CHECK(windows[1].id == w3_id);
            RAISE_CHECK(windows[2].id == w2_id);
        }

        // Error: window_id is zero
        {
            MoveWindowQuery q{};
            q.window_id = WindowId(0);
            q.next_window_id = w1_id;
            Error err = db->move_window(q, nullptr);
            RAISE_CHECK(err.code() == ErrorCode::argument);
        }

        // Error: window_id does not exist
        {
            MoveWindowQuery q{};
            q.window_id = WindowId(999999);
            q.next_window_id = w1_id;
            Error err = db->move_window(q, nullptr);
            RAISE_CHECK(err.code() != ErrorCode::ok);
        }

        // Error: next_window_id does not exist
        {
            MoveWindowQuery q{};
            q.window_id = w1_id;
            q.next_window_id = WindowId(999999);
            Error err = db->move_window(q, nullptr);
            RAISE_CHECK(err.code() != ErrorCode::ok);
        }

        TabsChange tabs_change{};
        ProfilesChange profiles_change{};
        VERIFY(db->delete_windows({w1_id, w2_id, w3_id}, TxMode::use_tx, nullptr, &tabs_change, &profiles_change));
    }

    {
        // move_profile test
        ProfileId p1_id{}, p2_id{}, p3_id{};
        {
            CreateProfileQuery q{};
            q.name = "profiles_db_test: Profile P1";
            q.icon_name = "p1_icon";
            q.is_test = true;
            VERIFY(db->create_profile(q, &p1_id, nullptr, nullptr, nullptr, nullptr, nullptr));
        }
        {
            CreateProfileQuery q{};
            q.name = "profiles_db_test: Profile P2";
            q.icon_name = "p2_icon";
            q.is_test = true;
            VERIFY(db->create_profile(q, &p2_id, nullptr, nullptr, nullptr, nullptr, nullptr));
        }
        {
            CreateProfileQuery q{};
            q.name = "profiles_db_test: Profile P3";
            q.icon_name = "p3_icon";
            q.is_test = true;
            VERIFY(db->create_profile(q, &p3_id, nullptr, nullptr, nullptr, nullptr, nullptr));
        }

        // Initial order: P1, P2, P3 (ascending seq_num from create_profile)
        {
            std::vector<ProfileDto> profiles;
            VERIFY(db->get_profiles({p1_id, p2_id, p3_id}, &profiles));
            RAISE_CHECK(profiles.size() == 3);
            RAISE_CHECK(profiles[0].id == p1_id);
            RAISE_CHECK(profiles[1].id == p2_id);
            RAISE_CHECK(profiles[2].id == p3_id);
        }

        // Move P3 before P2 (next_profile_id = P2)
        {
            MoveProfileQuery q{};
            q.profile_id = p3_id;
            q.next_profile_id = p2_id;
            ProfilesChange change{};
            VERIFY(db->move_profile(q, &change));
            RAISE_CHECK(change.updated.size() == 1);
            RAISE_CHECK(change.updated[0].id == p3_id);
        }

        // Verify order: P1, P3, P2
        {
            std::vector<ProfileDto> profiles;
            VERIFY(db->get_profiles({p1_id, p2_id, p3_id}, &profiles));
            RAISE_CHECK(profiles.size() == 3);
            RAISE_CHECK(profiles[0].id == p1_id);
            RAISE_CHECK(profiles[1].id == p3_id);
            RAISE_CHECK(profiles[2].id == p2_id);
        }

        // Move P1 to end (next_profile_id = 0)
        {
            MoveProfileQuery q{};
            q.profile_id = p1_id;
            q.next_profile_id = ProfileId(0);
            ProfilesChange change{};
            VERIFY(db->move_profile(q, &change));
            RAISE_CHECK(change.updated.size() == 1);
            RAISE_CHECK(change.updated[0].id == p1_id);
        }

        // Verify order: P3, P2, P1
        {
            std::vector<ProfileDto> profiles;
            VERIFY(db->get_profiles({p1_id, p2_id, p3_id}, &profiles));
            RAISE_CHECK(profiles.size() == 3);
            RAISE_CHECK(profiles[0].id == p3_id);
            RAISE_CHECK(profiles[1].id == p2_id);
            RAISE_CHECK(profiles[2].id == p1_id);
        }

        // Move P1 before P3 (first position)
        {
            MoveProfileQuery q{};
            q.profile_id = p1_id;
            q.next_profile_id = p3_id;
            ProfilesChange change{};
            VERIFY(db->move_profile(q, &change));
            RAISE_CHECK(change.updated.size() == 1);
            RAISE_CHECK(change.updated[0].id == p1_id);
        }

        // Verify order: P1, P3, P2
        {
            std::vector<ProfileDto> profiles;
            VERIFY(db->get_profiles({p1_id, p2_id, p3_id}, &profiles));
            RAISE_CHECK(profiles.size() == 3);
            RAISE_CHECK(profiles[0].id == p1_id);
            RAISE_CHECK(profiles[1].id == p3_id);
            RAISE_CHECK(profiles[2].id == p2_id);
        }

        // Error: profile_id is zero
        {
            MoveProfileQuery q{};
            q.profile_id = ProfileId(0);
            q.next_profile_id = p1_id;
            Error err = db->move_profile(q, nullptr);
            RAISE_CHECK(err.code() == ErrorCode::argument);
        }

        // Error: next_profile_id does not exist
        {
            MoveProfileQuery q{};
            q.profile_id = p1_id;
            q.next_profile_id = ProfileId(999999);
            Error err = db->move_profile(q, nullptr);
            RAISE_CHECK(err.code() != ErrorCode::ok);
        }

        VERIFY(db->delete_profiles({p1_id, p2_id, p3_id}, nullptr, nullptr, nullptr));
    }

    {
        // move_bookmark / move_bookmark_folder test
        // Create a folder with mixed children: F_A, B_A, B_B, F_B
        BookmarkFolderId test_folder_id{};
        {
            CreateBookmarkFolderQuery q{};
            q.profile_id = profile_id;
            q.name = "profiles_db_test: Move Test Folder";
            q.parent_folder_id = root_folder_id;
            VERIFY(db->create_bookmark_folder(q, &test_folder_id, nullptr));
        }

        BookmarkFolderId fa_id{}, fb_id{};
        BookmarkId ba_id{}, bb_id{};
        {
            CreateBookmarkFolderQuery q{};
            q.profile_id = profile_id;
            q.name = "profiles_db_test: FA";
            q.parent_folder_id = test_folder_id;
            VERIFY(db->create_bookmark_folder(q, &fa_id, nullptr));
        }
        {
            CreateBookmarkQuery q{};
            q.profile_id = profile_id;
            q.name = "profiles_db_test: BA";
            q.address = "https://example.com/ba";
            q.parent_folder_id = test_folder_id;
            VERIFY(db->create_bookmark(q, &ba_id, nullptr));
        }
        {
            CreateBookmarkQuery q{};
            q.profile_id = profile_id;
            q.name = "profiles_db_test: BB";
            q.address = "https://example.com/bb";
            q.parent_folder_id = test_folder_id;
            VERIFY(db->create_bookmark(q, &bb_id, nullptr));
        }
        {
            CreateBookmarkFolderQuery q{};
            q.profile_id = profile_id;
            q.name = "profiles_db_test: FB";
            q.parent_folder_id = test_folder_id;
            VERIFY(db->create_bookmark_folder(q, &fb_id, nullptr));
        }

        // Initial order: FA, BA, BB, FB
        {
            std::vector<BookmarkItem> children;
            VERIFY(db->get_bookmark_folder_children(profile_id, &test_folder_id, &children));
            RAISE_CHECK(children.size() == 4);
            RAISE_CHECK(children[0].id == fa_id.value && children[0].type == BookmarkItemType::folder);
            RAISE_CHECK(children[1].id == ba_id.value && children[1].type == BookmarkItemType::bookmark);
            RAISE_CHECK(children[2].id == bb_id.value && children[2].type == BookmarkItemType::bookmark);
            RAISE_CHECK(children[3].id == fb_id.value && children[3].type == BookmarkItemType::folder);
        }

        // Move BB before FA (first position) — bookmark before a folder
        {
            MoveBookmarkQuery q{};
            q.bookmark_id = bb_id;
            q.target_parent_folder_id = test_folder_id;
            q.next_folder_id = fa_id;
            BookmarksChange change{};
            VERIFY(db->move_bookmark(q, &change));
            RAISE_CHECK(change.updated.size() == 1);
            RAISE_CHECK(change.updated[0].id == bb_id);
        }

        // Verify order: BB, FA, BA, FB
        {
            std::vector<BookmarkItem> children;
            VERIFY(db->get_bookmark_folder_children(profile_id, &test_folder_id, &children));
            RAISE_CHECK(children.size() == 4);
            RAISE_CHECK(children[0].id == bb_id.value);
            RAISE_CHECK(children[1].id == fa_id.value);
            RAISE_CHECK(children[2].id == ba_id.value);
            RAISE_CHECK(children[3].id == fb_id.value);
        }

        // Move FB before BA — folder before a bookmark
        {
            MoveBookmarkFolderQuery q{};
            q.folder_id = fb_id;
            q.target_parent_folder_id = test_folder_id;
            q.next_bookmark_id = ba_id;
            BookmarkFoldersChange change{};
            VERIFY(db->move_bookmark_folder(q, &change));
            RAISE_CHECK(change.updated.size() == 1);
            RAISE_CHECK(change.updated[0].id == fb_id);
        }

        // Verify order: BB, FA, FB, BA
        {
            std::vector<BookmarkItem> children;
            VERIFY(db->get_bookmark_folder_children(profile_id, &test_folder_id, &children));
            RAISE_CHECK(children.size() == 4);
            RAISE_CHECK(children[0].id == bb_id.value);
            RAISE_CHECK(children[1].id == fa_id.value);
            RAISE_CHECK(children[2].id == fb_id.value);
            RAISE_CHECK(children[3].id == ba_id.value);
        }

        // Move BA to end
        {
            MoveBookmarkQuery q{};
            q.bookmark_id = ba_id;
            q.target_parent_folder_id = test_folder_id;
            BookmarksChange change{};
            VERIFY(db->move_bookmark(q, &change));
            RAISE_CHECK(change.updated.size() == 1);
        }

        // Verify order: BB, FA, FB, BA (BA was already last, stays last)
        {
            std::vector<BookmarkItem> children;
            VERIFY(db->get_bookmark_folder_children(profile_id, &test_folder_id, &children));
            RAISE_CHECK(children.size() == 4);
            RAISE_CHECK(children[3].id == ba_id.value);
        }

        // Move FA to end
        {
            MoveBookmarkFolderQuery q{};
            q.folder_id = fa_id;
            q.target_parent_folder_id = test_folder_id;
            BookmarkFoldersChange change{};
            VERIFY(db->move_bookmark_folder(q, &change));
            RAISE_CHECK(change.updated.size() == 1);
        }

        // Verify order: BB, FB, BA, FA
        {
            std::vector<BookmarkItem> children;
            VERIFY(db->get_bookmark_folder_children(profile_id, &test_folder_id, &children));
            RAISE_CHECK(children.size() == 4);
            RAISE_CHECK(children[0].id == bb_id.value);
            RAISE_CHECK(children[1].id == fb_id.value);
            RAISE_CHECK(children[2].id == ba_id.value);
            RAISE_CHECK(children[3].id == fa_id.value);
        }

        // Cross-folder move: move BA into FA
        {
            MoveBookmarkQuery q{};
            q.bookmark_id = ba_id;
            q.target_parent_folder_id = fa_id;
            BookmarksChange change{};
            VERIFY(db->move_bookmark(q, &change));
            RAISE_CHECK(change.updated.size() == 1);
            RAISE_CHECK(change.updated[0].parent_folder_id == fa_id);
        }

        // Verify BA is now inside FA, and test_folder has 3 children
        {
            std::vector<BookmarkItem> children;
            VERIFY(db->get_bookmark_folder_children(profile_id, &test_folder_id, &children));
            RAISE_CHECK(children.size() == 3);

            std::vector<BookmarkItem> fa_children;
            VERIFY(db->get_bookmark_folder_children(profile_id, &fa_id, &fa_children));
            RAISE_CHECK(fa_children.size() == 1);
            RAISE_CHECK(fa_children[0].id == ba_id.value);
        }

        // Error: bookmark_id is zero
        {
            MoveBookmarkQuery q{};
            q.bookmark_id = BookmarkId(0);
            q.target_parent_folder_id = test_folder_id;
            Error err = db->move_bookmark(q, nullptr);
            RAISE_CHECK(err.code() == ErrorCode::argument);
        }

        // Error: folder_id is zero
        {
            MoveBookmarkFolderQuery q{};
            q.folder_id = BookmarkFolderId(0);
            q.target_parent_folder_id = test_folder_id;
            Error err = db->move_bookmark_folder(q, nullptr);
            RAISE_CHECK(err.code() == ErrorCode::argument);
        }

        // Error: bookmark_id does not exist
        {
            MoveBookmarkQuery q{};
            q.bookmark_id = BookmarkId(999999);
            q.target_parent_folder_id = test_folder_id;
            Error err = db->move_bookmark(q, nullptr);
            RAISE_CHECK(err.code() != ErrorCode::ok);
        }

        // Cleanup
        VERIFY(db->delete_bookmark_folders({test_folder_id}, nullptr, nullptr));
    }

    // Extension storage permits empty-string keys.
    {
        ExtensionId extension_id;
        VERIFY(db->install_extension({
            .profile_id = profile_id,
            .sku = ExtensionSku("profiles-db-test:firefox:empty-storage-key"),
            .type = ExtensionType::firefox,
        }, &extension_id, nullptr));

        std::vector<ExtensionStorageAreas::Change> changes;
        VERIFY(db->extension_storage_areas_set({
            .extension_id = extension_id,
            .storage_id = "local",
            .key_values = {{.key = "", .value = "empty-key-value"}},
        }, &changes));
        RAISE_CHECK(changes.size() == 1);
        RAISE_CHECK(changes.front().key.empty());
        RAISE_CHECK(!changes.front().old_value);
        RAISE_CHECK(changes.front().new_value == "empty-key-value");

        VERIFY(db->extension_storage_areas_set({
            .extension_id = extension_id,
            .storage_id = "local",
            .key_values = {{.key = "", .value = "empty-key-value"}},
        }, &changes));
        RAISE_CHECK(changes.empty());

        VERIFY(db->extension_storage_areas_set({
            .extension_id = extension_id,
            .storage_id = "local",
            .key_values = {{.key = "", .value = "updated-empty-key-value"}},
        }, &changes));
        RAISE_CHECK(changes.size() == 1);
        RAISE_CHECK(changes.front().key.empty());
        RAISE_CHECK(changes.front().old_value == "empty-key-value");
        RAISE_CHECK(changes.front().new_value == "updated-empty-key-value");

        const uint64_t expected_bytes = std::string("updated-empty-key-value").size();
        uint64_t bytes_in_use = 0;
        VERIFY(db->extension_storage_areas_get_bytes_in_use({
            .extension_id = extension_id,
            .storage_id = "local",
        }, &bytes_in_use));
        RAISE_CHECK(bytes_in_use == expected_bytes);

        VERIFY(db->extension_storage_areas_get_bytes_in_use({
            .extension_id = extension_id,
            .storage_id = "local",
            .keys = std::vector<std::string>{""},
        }, &bytes_in_use));
        RAISE_CHECK(bytes_in_use == expected_bytes);

        VERIFY(db->extension_storage_areas_get_bytes_in_use({
            .extension_id = extension_id,
            .storage_id = "local",
            .keys = std::vector<std::string>{},
        }, &bytes_in_use));
        RAISE_CHECK(bytes_in_use == 0);

        VERIFY(db->extension_storage_areas_get_bytes_in_use({
            .extension_id = extension_id,
            .storage_id = "local",
            .keys = std::vector<std::string>{"missing"},
        }, &bytes_in_use));
        RAISE_CHECK(bytes_in_use == 0);

        std::vector<ExtensionStorageAreas::KeyValue> values;
        VERIFY(db->extension_storage_areas_get({
            .extension_id = extension_id,
            .storage_id = "local",
            .key_values = {{.key = ""}},
        }, &values));
        RAISE_CHECK(values.size() == 1);
        RAISE_CHECK(values.front().key.empty());
        RAISE_CHECK(values.front().value == "updated-empty-key-value");

        VERIFY(db->extension_storage_areas_remove({
            .extension_id = extension_id,
            .storage_id = "local",
            .keys = {""},
        }, &changes));
        RAISE_CHECK(changes.size() == 1);
        RAISE_CHECK(changes.front().key.empty());
        RAISE_CHECK(changes.front().old_value == "updated-empty-key-value");
        RAISE_CHECK(!changes.front().new_value);
        values.clear();
        VERIFY(db->extension_storage_areas_get_all({
            .extension_id = extension_id,
            .storage_id = "local",
        }, &values));
        RAISE_CHECK(values.empty());
        VERIFY(db->extension_storage_areas_get_bytes_in_use({
            .extension_id = extension_id,
            .storage_id = "local",
        }, &bytes_in_use));
        RAISE_CHECK(bytes_in_use == 0);

        VERIFY(db->uninstall_extension({.id = extension_id}, nullptr));
    }

    // Extension storage quotas are checked against the complete post-write
    // state and rejected writes are atomic.
    {
        ExtensionId extension_id;
        VERIFY(db->install_extension({
            .profile_id = profile_id,
            .sku = ExtensionSku("profiles-db-test:firefox:storage-quota"),
            .type = ExtensionType::firefox,
        }, &extension_id, nullptr));

        const ExtensionStorageAreas::Quota quota{
            .bytes = 6,
            .bytes_per_item = 4,
            .max_items = 2,
        };
        std::vector<ExtensionStorageAreas::Change> changes;
        VERIFY(db->extension_storage_areas_set({
            .extension_id = extension_id,
            .storage_id = "sync",
            .key_values = {{.key = "a", .value = "123"}},
            .quota = quota,
        }, &changes));
        RAISE_CHECK(changes.size() == 1);
        VERIFY(db->extension_storage_areas_set({
            .extension_id = extension_id,
            .storage_id = "sync",
            .key_values = {{.key = "b", .value = "1"}},
            .quota = quota,
        }, &changes));
        RAISE_CHECK(changes.size() == 1);

        Error err = db->extension_storage_areas_set({
            .extension_id = extension_id,
            .storage_id = "sync",
            .key_values = {{.key = "c", .value = ""}},
            .quota = quota,
        }, &changes);
        RAISE_CHECK(err.code() == ErrorCode::invalid_state);
        RAISE_CHECK(err.message() == "QUOTA_BYTES quota exceeded");
        RAISE_CHECK(changes.empty());

        err = db->extension_storage_areas_set({
            .extension_id = extension_id,
            .storage_id = "sync",
            .key_values = {{.key = "c", .value = "1234"}},
            .quota = ExtensionStorageAreas::Quota{
                .bytes = 100,
                .bytes_per_item = 4,
            },
        }, &changes);
        RAISE_CHECK(err.code() == ErrorCode::invalid_state);
        RAISE_CHECK(err.message() == "QUOTA_BYTES_PER_ITEM quota exceeded");
        RAISE_CHECK(changes.empty());

        err = db->extension_storage_areas_set({
            .extension_id = extension_id,
            .storage_id = "sync",
            .key_values = {
                {.key = "a", .value = ""},
                {.key = "c", .value = ""},
            },
            .quota = ExtensionStorageAreas::Quota{
                .bytes = 100,
                .max_items = 2,
            },
        }, &changes);
        RAISE_CHECK(err.code() == ErrorCode::invalid_state);
        RAISE_CHECK(err.message() == "MAX_ITEMS quota exceeded");
        RAISE_CHECK(changes.empty());

        std::vector<ExtensionStorageAreas::KeyValue> values;
        VERIFY(db->extension_storage_areas_get({
            .extension_id = extension_id,
            .storage_id = "sync",
            .key_values = {{.key = "a"}, {.key = "c"}},
        }, &values));
        RAISE_CHECK(values.size() == 1);
        RAISE_CHECK(values.front().key == "a");
        RAISE_CHECK(values.front().value == "123");

        VERIFY(db->extension_storage_areas_set({
            .extension_id = extension_id,
            .storage_id = "sync",
            .key_values = {{.key = "a", .value = ""}},
            .quota = quota,
        }, &changes));
        RAISE_CHECK(changes.size() == 1);

        VERIFY(db->uninstall_extension({.id = extension_id}, nullptr));
    }

    // Extension session storage lifecycle.
    {
        ExtensionId extension_id;
        VERIFY(db->install_extension({
            .profile_id = profile_id,
            .sku = ExtensionSku("profiles-db-test:firefox:session-lifecycle"),
            .type = ExtensionType::firefox,
        }, &extension_id, nullptr));

        const auto populate_session = [&] {
            return db->extension_storage_areas_set({
                .extension_id = extension_id,
                .storage_id = "session",
                .key_values = {{.key = "key", .value = "value"}},
            });
        };
        const auto verify_session_empty = [&]() -> Error {
            std::vector<ExtensionStorageAreas::KeyValue> values;
            VERIFY(db->extension_storage_areas_get_all({
                .extension_id = extension_id,
                .storage_id = "session",
            }, &values));
            RAISE_CHECK(values.empty());
            SUCCEED();
        };

        VERIFY(populate_session());
        VERIFY(db->update_extension({
            .id = extension_id,
            .is_enabled = false,
            .update_is_enabled = true,
        }, nullptr));
        VERIFY(verify_session_empty());

        VERIFY(populate_session());
        VERIFY(db->update_extension({
            .id = extension_id,
            .file_path = "updated-extension.zip",
            .update_file_path = true,
        }, nullptr));
        VERIFY(verify_session_empty());

        VERIFY(populate_session());
        VERIFY(db->extension_session_storage_clear(extension_id));
        VERIFY(verify_session_empty());

        VERIFY(populate_session());
        VERIFY(db->uninstall_extension({.id = extension_id}, nullptr));
        VERIFY(verify_session_empty());
    }

    // test cleanup
    VERIFY(db->delete_bookmark_folders({root_folder_id}, nullptr, nullptr));
    VERIFY(db->delete_profiles({profile_id}, nullptr, nullptr, nullptr));
    SUCCEED();
}

Error
test_null_values(std::shared_ptr<ProfilesDb> db)
{
    DbStmt stmt;
    VERIFY(db->db()->query("SELECT NULL AS null_value;", {}, &stmt));
    auto has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);
    RAISE_CHECK(stmt.integer("null_value") == 0, "DbStmt::integer() should return int64_t(0) when NULL value is queried");
    SUCCEED();
}

void
test_profiles_db(std::shared_ptr<ProfilesDb> db)
{
    VERIFY_LOG(test_profiles_db_internal(db));
    VERIFY_LOG(test_null_values(db));
}

} // namespace orion
