#include <core/profiles_db/profiles_db.h>
#include <format>
#include <chrono>

namespace orion {

//-------------------------------------------------------------------------
// Snapshots
//-------------------------------------------------------------------------

std::vector<std::string>
ProfilesDb::get_snapshot_tab_columns() const
{
    return {
        "id",
        "profile_id",
        "created_at",
        "window_seq_num",
        "name",
        "icon_name",
        "seq_num",
        "is_pinned",
        "is_suspended",
        "uri",
        "session_state",
        "is_tab_group",
        "has_ui_window",
        "is_window_selected",
        "window_name",
        "window_icon_name",
        "window_keyboard_shortcut",
        "window_is_suspended",
        "is_tab_selected",
        "back_forward_list_index",
        "webkit_back_forward_list_index",
        "merged_back_forward_list_length",
    };
}

void
ProfilesDb::load_snapshot_tab_from_stmt(const DbStmt& stmt, SnapshotTab* snapshot_tab) const
{
    snapshot_tab->id = SnapshotTabId(stmt.integer("id"));
    snapshot_tab->profile_id = ProfileId(stmt.integer("profile_id"));
    snapshot_tab->created_at = UnixTimeUtc64(stmt.integer("created_at"));
    snapshot_tab->window_seq_num = stmt.real("window_seq_num");
    snapshot_tab->name = stmt.text("name");
    snapshot_tab->icon_name = stmt.text("icon_name");
    snapshot_tab->is_pinned = stmt.integer("is_pinned");
    snapshot_tab->is_suspended = stmt.integer("is_suspended");
    snapshot_tab->seq_num = stmt.real("seq_num");
    snapshot_tab->uri = stmt.text("uri");
    snapshot_tab->session_state = stmt.blob("session_state");
    snapshot_tab->is_tab_group = stmt.integer("is_tab_group");
    snapshot_tab->has_ui_window = stmt.integer("has_ui_window");
    snapshot_tab->is_window_selected = stmt.integer("is_window_selected");
    snapshot_tab->window_name = stmt.text("window_name");
    snapshot_tab->window_icon_name = stmt.text("window_icon_name");
    snapshot_tab->window_keyboard_shortcut = stmt.text("window_keyboard_shortcut");
    snapshot_tab->window_is_suspended = stmt.integer("window_is_suspended");
    snapshot_tab->is_tab_selected = stmt.integer("is_tab_selected");
    snapshot_tab->back_forward_list_index = stmt.integer("back_forward_list_index");
    snapshot_tab->webkit_back_forward_list_index = stmt.integer("webkit_back_forward_list_index");
    snapshot_tab->merged_back_forward_list_length = stmt.integer("merged_back_forward_list_length");
}

Error
ProfilesDb::create_snapshot(ProfileId profile_id, TxMode tx_mode, UnixTimeUtc64* created_at)
{
    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::vector<ProfileDto> profiles;
    VERIFY(get_profiles({profile_id}, &profiles));
    if (profiles.empty()) {
        RAISE(ErrorCode::not_found, "profile with id %lld not found", profile_id.value);
    }
    ProfileDto profile = profiles[0];

    std::vector<WindowId> window_ids;
    VERIFY(get_window_ids({.profile_id=profile_id}, &window_ids));

    std::vector<WindowDto> windows;
    VERIFY(get_windows(window_ids, &windows));

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(m_db->begin(&tx));
    }

    {
        // Delete any existing snapshot tabs for this profile
        std::string sql = R"(
            DELETE FROM snapshot_tabs
            WHERE profile_id = ?1;
        )";

        VERIFY(m_db->exec(sql, {profile_id.value}));
    }

    UnixTimeUtc64 now = static_cast<UnixTimeUtc64>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    for (const auto& window : windows) {
        if (window.is_private) {
            // Never include private windows in the snapshot
            continue;
        }

        std::vector<TabId> tab_ids;
        GetTabIdsQuery query{
            .window_id = window.id,
        };
        VERIFY(get_tab_ids(query, &tab_ids));

        std::vector<TabDto> tabs;
        VERIFY(get_tabs(tab_ids, &tabs));
        bool is_window_selected = (profile.selected_window_id == window.id);

        for (const auto& tab : tabs) {
            bool is_tab_selected = (window.selected_tab_id == tab.id);

            std::vector<DbVariant> args{
                nullptr, // id - autoincrement
                profile_id.value,
                now.value,
                window.seq_num,
                tab.name,
                tab.icon_name,
                tab.seq_num,
                tab.is_pinned,
                tab.is_suspended,
                tab.uri,
                std::vector<uint8_t>{}, // session_state
                window.is_tab_group,
                window.has_ui_window,
                is_window_selected,
                window.name,
                window.icon_name,
                window.keyboard_shortcut,
                window.is_suspended,
                is_tab_selected,
                tab.back_forward_list_index,
                tab.webkit_back_forward_list_index,
                tab.merged_back_forward_list_length,
            };

            std::string sql = std::format(R"(
                INSERT INTO snapshot_tabs ({})
                VALUES ({})
                RETURNING {};
            )", Db::join_strings(get_snapshot_tab_columns(), ", "),
                Db::make_placeholders(args.size()),
                Db::join_strings(get_snapshot_tab_columns(), ", ")
            );

            DbStmt stmt;
            VERIFY(m_db->query(sql, args, &stmt));

            bool has_row = stmt.next();
            VERIFY(stmt.error());
            RAISE_CHECK(has_row);

            SnapshotTab snapshot_tab{};
            load_snapshot_tab_from_stmt(stmt, &snapshot_tab);

            if (created_at) {
                *created_at = snapshot_tab.created_at;
            }

            // also save snapshot of back-forward list entries for the tab
            std::vector<TabBackForwardListEntryDto> back_forward_entries;
            VERIFY(get_tab_back_forward_list_entries(tab.id, &back_forward_entries));
            for (const auto& entry : back_forward_entries) {
                SnapshotBackForwardEntry snapshot_entry{
                    .snapshot_tab_id = snapshot_tab.id,
                    .webkit_index = entry.webkit_index,
                    .uri = entry.uri,
                };
                VERIFY(create_snapshot_tab_back_forward_list_entry(&snapshot_entry));
            }
        }
    }

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::get_last_snapshot(ProfileId profile_id, SnapshotDto* snapshot)
{
    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }
    RAISE_CHECK(snapshot);

    std::string sql = std::format(R"(
        SELECT id, profile_id, created_at
        FROM snapshot_tabs
        WHERE profile_id = ?1
        ORDER BY created_at DESC, window_seq_num, seq_num
        LIMIT 1;
    )", Db::join_strings(get_snapshot_tab_columns(), ", "));

    DbStmt stmt;
    VERIFY(m_db->query(sql, {profile_id.value}, &stmt));
    bool has_row = stmt.next();
    VERIFY(stmt.error());
    if (!has_row) {
        log_warning("get_last_snapshot: no snapshot found for profile_id %lld", profile_id.value);
        SUCCEED();
    }

    snapshot->profile_id = ProfileId(stmt.integer("profile_id"));
    snapshot->created_at = UnixTimeUtc64(stmt.integer("created_at"));

    SUCCEED();
}

Error
ProfilesDb::delete_snapshot(ProfileId profile_id)
{
    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::string sql = R"(
        DELETE FROM snapshot_tabs
        WHERE profile_id = ?1;
    )";

    VERIFY(m_db->exec(sql, {profile_id.value}));
    SUCCEED();
}

Error
ProfilesDb::restore_snapshot(
    RestoreSnapshotQuery query, WindowsChange* windows_change, TabsChange* tabs_change,
    ProfilesChange* profiles_change, TabBackForwardListEntriesChange* back_forward_entries_change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }
    if (query.created_at == UnixTimeUtc64(0)) {
        RAISE(ErrorCode::argument, "created_at is not set");
    }

    // Load all snapshot tabs in the snapshot
    std::vector<SnapshotTab> snapshot_tabs;
    {
        std::string sql = std::format(R"(
            SELECT {}
            FROM snapshot_tabs
            WHERE profile_id = ?1 AND created_at = ?2
            ORDER BY window_seq_num, seq_num;
        )", Db::join_strings(get_snapshot_tab_columns(), ", "));

        std::vector<DbVariant> args = {
            query.profile_id.value,
            query.created_at.value,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        while (stmt.next()) {
            SnapshotTab snapshot_tab{};
            load_snapshot_tab_from_stmt(stmt, &snapshot_tab);

            snapshot_tabs.push_back(snapshot_tab);
        }
        VERIFY(stmt.error());

        if (snapshot_tabs.empty()) {
            RAISE(ErrorCode::not_found, "no snapshot found for profile_id %lld at created_at %lld",
                query.profile_id.value, query.created_at.value
            );
        }
    }

    DbTx tx;
    VERIFY(m_db->begin(&tx));

    if (query.create_snapshot_before_restore) {
        // Create new snapshot from current state
        UnixTimeUtc64 created_at{};
        VERIFY(create_snapshot(query.profile_id, TxMode::no_tx, &created_at));
        log_debug("restore_snapshot: created new snapshot for profile_id %lld at created_at %lld before restoring old snapshot at created_at %lld",
            query.profile_id.value, created_at.value, query.created_at.value
        );
    }

    // Delete all existing windows (both Saved and Unsaved Tab Groups) and their tabs in the profile
    {
        std::vector<WindowId> window_ids;
        VERIFY(get_window_ids({.profile_id=query.profile_id}, &window_ids));

        std::vector<WindowDto> windows;
        VERIFY(get_windows(window_ids, &windows));

        std::vector<WindowId> tab_group_ids;
        for (const auto& window : windows) {
            tab_group_ids.push_back(window.id);
        }

        VERIFY(delete_windows(tab_group_ids, TxMode::no_tx, windows_change, tabs_change, profiles_change));
    }

    struct WindowInfo {
        WindowId id{};
        bool is_tab_group{};
    };

    // Restore tab groups and tabs from the snapshot
    std::map<double, WindowInfo> window_seq_num_to_info;
    WindowId selected_window_id{};

    for (const auto& snapshot_tab : snapshot_tabs) {
        WindowInfo window_info{
            .is_tab_group = snapshot_tab.is_tab_group,
        };
        auto it = window_seq_num_to_info.find(snapshot_tab.window_seq_num);
        if (it == window_seq_num_to_info.end()) {
            // Create a new tab group window. Each new window is never private.
            CreateWindowQuery create_query{
                .profile_id = query.profile_id,
                .name = snapshot_tab.window_name,
                .icon_name = snapshot_tab.window_icon_name,
                .keyboard_shortcut = snapshot_tab.window_keyboard_shortcut,
                .is_suspended = snapshot_tab.window_is_suspended,
                .seq_num = snapshot_tab.window_seq_num, // will be refined in create_window()
                .is_tab_group = snapshot_tab.is_tab_group,
                .selected_tab_id = TabId(0), // will be set later
                .has_ui_window = snapshot_tab.has_ui_window,
            };
            VERIFY(create_window(create_query, &window_info.id, windows_change));
            window_seq_num_to_info[snapshot_tab.window_seq_num] = window_info;

            if (!selected_window_id.value && snapshot_tab.is_window_selected) {
                selected_window_id = window_info.id;
            }
        } else {
            window_info = it->second;
        }
        RAISE_CHECK(window_info.id != WindowId(0));

        // Create a new tab in the tab group window
        CreateTabQuery create_query{
            .window_id = window_info.id,
            .name = snapshot_tab.name,
            .icon_name = snapshot_tab.icon_name,
            .is_pinned = snapshot_tab.is_pinned,
            .is_suspended = snapshot_tab.is_suspended,
            .seq_num = snapshot_tab.seq_num, // will be refined in create_tab()
            .uri = snapshot_tab.uri,
            .session_state = snapshot_tab.session_state,
            .back_forward_list_index = snapshot_tab.back_forward_list_index,
            .webkit_back_forward_list_index = snapshot_tab.webkit_back_forward_list_index,
            .merged_back_forward_list_length = snapshot_tab.merged_back_forward_list_length,
        };
        TabId new_tab_id{};
        VERIFY(create_tab(create_query, &new_tab_id, TxMode::no_tx, tabs_change, windows_change));
        RAISE_CHECK(new_tab_id.value);

        // Select the tab that should be selected in the window
        if (snapshot_tab.is_tab_selected) {
            UpdateWindowQuery update_query{
                .id = window_info.id,
                .selected_tab_id = new_tab_id,
                .update_selected_tab_id = true,
            };
            VERIFY(update_window(update_query, windows_change));
        }

        // Restore back-forward list entries for the tab
        std::vector<SnapshotBackForwardEntry> back_forward_entries;
        VERIFY(get_snapshot_tab_back_forward_list_entries(snapshot_tab.id, &back_forward_entries));

        for (const auto& snapshot_entry : back_forward_entries) {
            CreateTabBackForwardListEntryQuery query{
                .tab_id = new_tab_id,
                .webkit_index = snapshot_entry.webkit_index,
                .uri = snapshot_entry.uri,
            };
            TabBackForwardListEntryDto entry{};
            VERIFY(create_tab_back_forward_list_entry(query, &entry, back_forward_entries_change));
        }
    }

    // Set the selected_window_id in the profile
    if (selected_window_id != WindowId(0)) {
        SetSelectedWindowIdQuery set_selected_query{
            .profile_id = query.profile_id,
            .window_id = selected_window_id,
        };
        VERIFY(set_selected_window_id(set_selected_query, profiles_change));
    }

    VERIFY(tx.commit());
    SUCCEED();
}

} // namespace orion