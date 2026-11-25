#define VERIFY_WINDOWS_CHANGE 1

#include <core/profiles_db/profiles_db.h>
#include <format>
#include <chrono>

namespace orion {

//-------------------------------------------------------------------------
// Windows
//-------------------------------------------------------------------------

std::vector<std::string>
ProfilesDb::get_window_columns() const
{
    return {
        "id",
        "profile_id",
        "name",
        "icon_name",
        "keyboard_shortcut",
        "is_suspended",
        "seq_num",
        "is_tab_group",
        "selected_tab_id",
        "num_tabs",
        "has_ui_window",
        "undo_redo_tab_step_seq_num",
        "prev_selected_tab_id",
        "is_focus_mode",
        "is_private",
    };
}

void
ProfilesDb::load_window_from_stmt(const DbStmt& stmt, WindowDto* window) const
{
    window->id = WindowId(stmt.integer("id"));
    window->profile_id = ProfileId(stmt.integer("profile_id"));
    window->name = stmt.text("name");
    window->icon_name = stmt.text("icon_name");
    window->keyboard_shortcut = stmt.text("keyboard_shortcut");
    window->is_suspended = stmt.integer("is_suspended");
    window->seq_num = stmt.real("seq_num");
    window->is_tab_group = stmt.integer("is_tab_group");
    window->selected_tab_id = TabId(stmt.integer("selected_tab_id")); // will be 0 if no tab is selected
    window->num_tabs = stmt.integer("num_tabs");
    window->has_ui_window = stmt.integer("has_ui_window");
    window->undo_redo_tab_step_seq_num = stmt.integer("undo_redo_tab_step_seq_num");
    window->prev_selected_tab_id = TabId(stmt.integer("prev_selected_tab_id"));
    window->is_focus_mode = stmt.integer("is_focus_mode");
    window->is_private = stmt.integer("is_private");
}

Error
ProfilesDb::get_window_ids(GetWindowIdsQuery query, std::vector<WindowId>* ids)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    WindowId shared_pinned_tabs_window_id{};
    VERIFY(profile_id_get_shared_pinned_tabs_window_id(query.profile_id, &shared_pinned_tabs_window_id));

    std::string where_clause = "profile_id = ?1";

    if (!query.include_private_windows) {
        where_clause += " AND is_private = 0";
    }

    std::string sql = std::format(R"(
        SELECT id FROM windows
        WHERE {}
        ORDER BY profile_id, seq_num;
    )", where_clause);

    DbStmt stmt;
    VERIFY(m_db->query(sql, {query.profile_id.value}, &stmt));

    while (stmt.next()) {
        WindowId id = WindowId(stmt.integer("id"));
        if(id != shared_pinned_tabs_window_id)
        {
            ids->push_back(id);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_windows(const std::vector<WindowId>& ids, std::vector<WindowDto>* windows)
{
    auto args = to_db_variant_vector(ids);
    std::string sql = std::format(R"(
        SELECT {}
        FROM windows
        WHERE id IN ({})
        ORDER BY profile_id, seq_num;
    )", Db::join_strings(get_window_columns(), ", "),
        Db::make_placeholders(ids.size())
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto window = WindowDto{};
        load_window_from_stmt(stmt, &window);
        windows->emplace_back(window);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

// On collisions, tries to insert after the existing item with the specified seq_num
Error
ProfilesDb::refine_window_seq_num(RefineWindowSeqNumQuery query, double* seq_num)
{
    RAISE_CHECK(seq_num);

    // selects 3 columns:
    // 1. id_exists - whether the seq_num exists in the table
    // 2. lower_seq_num - sequence number of existing item that directly precedes seq_num (NULL if no such item exists)
    // 3. higher_seq_num - sequence number of existing item that directly follows seq_num (NULL if no such item exists)
    std::string sql = R"(
        WITH combined AS (
            SELECT seq_num FROM windows
            WHERE profile_id = ?1
        )
        SELECT
            EXISTS(
                SELECT 1 FROM combined WHERE seq_num = ?2
            ) AS id_exists,
            (
                SELECT MAX(seq_num) FROM combined WHERE seq_num < ?2
            ) AS lower_seq_num,
            (
                SELECT MIN(seq_num) FROM combined WHERE seq_num > ?2
            ) AS higher_seq_num;
    )";

    std::vector<DbVariant> args = {
        query.profile_id.value,
        query.seq_num,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);
    
    bool id_exists = stmt.integer("id_exists") != 0;
    std::optional<double> higher_seq_num = stmt.real_optional("higher_seq_num");
    std::optional<double> lower_seq_num = stmt.real_optional("lower_seq_num");

    if (!id_exists) {
        // seq_num is not used by any existing item, so it can be used as is
        *seq_num = query.seq_num;
        SUCCEED();
    }

    if (query.collision_mode == SeqNumCollisionMode::after) {
        // try to insert after the existing item
        if (higher_seq_num.has_value()) {
            // set seq_num to midpoint between the existing seq_num and the one after it
            *seq_num = average(query.seq_num, higher_seq_num.value());
        } else {
            // no items after the existing one
            *seq_num = query.seq_num + 1.0;
        }
    } else {
        // try to insert before the existing item
        if (lower_seq_num.has_value()) {
            // set seq_num to midpoint between the existing seq_num and the one before it
            *seq_num = average(query.seq_num, lower_seq_num.value());
        } else {
            // no items before the existing one
            *seq_num = query.seq_num - 1.0;
        }
    }
    SUCCEED();
}

Error
ProfilesDb::create_window(CreateWindowQuery query, WindowId* window_id, WindowsChange* change)
{
    return create_window(allocate_window_id(), std::move(query), window_id, change);
}

Error
ProfilesDb::create_window(
    WindowId new_window_id, CreateWindowQuery query, WindowId* window_id, WindowsChange* change)
{
    if (new_window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }
    track_window_id(new_window_id);

    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    if (!query.seq_num.has_value()) {
        // new Saved Window should be appended to the end of the list, new Tab Group should be appended
        // to the end of the list of Tab Groups

        std::optional<double> max_tab_group_seq_num{};
        VERIFY(m_db->query_real_or_null(R"(
            SELECT MAX(seq_num) AS max_tab_group_seq_num
            FROM windows
            WHERE profile_id = ?1 AND is_tab_group != 0;
        )", {query.profile_id.value}, &max_tab_group_seq_num));

        std::optional<double> min_saved_window_seq_num{};
        VERIFY(m_db->query_real_or_null(R"(
            SELECT MIN(seq_num) AS min_saved_window_seq_num
            FROM windows
            WHERE profile_id = ?1 AND is_tab_group = 0;
        )", {query.profile_id.value}, &min_saved_window_seq_num));

        std::optional<double> max_seq_num{};
        VERIFY(m_db->query_real_or_null(R"(
            SELECT MAX(seq_num) AS max_seq_num
            FROM windows
            WHERE profile_id = ?1;
        )", {query.profile_id.value}, &max_seq_num));
        
        if (query.is_tab_group) {
            // if there are no existing Tab Groups, append after the last Saved Window
            if (max_tab_group_seq_num.has_value()) {
                query.seq_num = max_tab_group_seq_num.value();
            } else {
                if (min_saved_window_seq_num.has_value()) {
                    // prepend before the first Saved Window
                    query.seq_num = min_saved_window_seq_num.value() - 1.0;
                } else {
                    // no existing windows at all
                    query.seq_num = 0.0;
                }
            }
        } else {
            if (max_seq_num.has_value()) {
                // append after the last existing window (will be refined below)
                query.seq_num = max_seq_num.value();
            } else {
                // no existing windows at all
                query.seq_num = 0.0;
            }
        }
    }
    
    // refine the seq_num to avoid collisions (always refine even if seq_num was not specified
    // in the original query)
    {
        // insert the window at the specified position
        auto refine_query = RefineWindowSeqNumQuery{
            .profile_id = query.profile_id,
            .seq_num = query.seq_num.value(),
        };
        double refined_seq_num = 0.0;
        VERIFY(refine_window_seq_num(refine_query, &refined_seq_num));
        query.seq_num = refined_seq_num;
    }

    if (!query.template_name.empty()) {
        // If template_name is set, use predefined template instead of the name specified in the query
        // (eg., if template_name is "Tab Group", and there are already 2 Tab Groups named "Tab Group 1"
        // and "Tab Group 2", then the new window will be named "Tab Group 3" even if the name field in
        // the query is set to something else).
        std::string sql = R"(
            SELECT CAST(TRIM(name, ?3 || ' ') AS INTEGER) AS num FROM windows
            WHERE profile_id = ?1 AND name LIKE ?2
            ORDER BY num DESC
            LIMIT 1;
        )";
        std::string like_pattern = Db::escape_like(query.template_name, '\\') + " %";
        DbStmt stmt{};
        VERIFY(m_db->query(sql, {query.profile_id.value, like_pattern, query.template_name}, &stmt));
        bool has_row = stmt.next();
        VERIFY(stmt.error());
        int64_t num = 1;
        if (has_row) {
            num = stmt.integer("num");
            num++; // pick the next number in the sequence
        }
        query.name = std::format("{} {}", query.template_name, num);
    }

    std::string sql = std::format(R"(
        INSERT INTO windows ({})
        VALUES ({})
        RETURNING {};
    )", Db::join_strings(get_window_columns(), ", "),
        Db::make_placeholders(get_window_columns().size()),
        Db::join_strings(get_window_columns(), ", ")
    );

    std::vector<DbVariant> args = {
        new_window_id.value,
        query.profile_id.value,
        query.name,
        query.icon_name,
        query.keyboard_shortcut,
        query.is_suspended,
        query.seq_num.value(),
        query.is_tab_group,
        query.selected_tab_id.value,
        int64_t(0), // num_tabs is always 0 when creating a new window
        query.has_ui_window,
        query.undo_redo_tab_step_seq_num,
        TabId(0).value, // prev_selected_tab_id is 0 when creating a new window
        query.is_focus_mode,
        query.is_private,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    WindowDto window{};
    load_window_from_stmt(stmt, &window);

    if (change) {
        change->created.emplace_back(window);
    }

    if (window_id) {
        *window_id = window.id;
    }
    SUCCEED();
}

Error
ProfilesDb::save_window(
    SaveWindowQuery query, WindowsChange* window_change, TabsChange* tab_change,
    TabBackForwardListEntriesChange* back_forward_list_change)
{
    TabsChange tab_change_internal{};
    WindowsChange window_change_internal{};
    TabBackForwardListEntriesChange back_forward_list_change_internal{};
    {
        std::vector<WindowDto> windows{};
        VERIFY(get_windows({query.window_id}, &windows));
        if(!windows.size())
        {
            RAISE(ErrorCode::not_found, "window with id %ld not found", query.window_id.value);
        }
        if(!windows.at(0).is_tab_group)
        {
            log_debug("save_window(): attempted to save window with id %ld but it is already saved", query.window_id.value);
            SUCCEED();
            //RAISE(ErrorCode::assert, "widow with id %ld is already a saved window", query.window_id.value);
        }
    }
    DbTx tx{};
    VERIFY(m_db->begin(&tx));
    {
        UpdateWindowQuery update_window_query{
            .id = query.window_id,
            .is_tab_group = false,
            .update_is_tab_group = true,
        };
        VERIFY(update_window(update_window_query, &window_change_internal));
    }
    {
        ProfileId profile_id{};
        window_id_get_profile_id(query.window_id, &profile_id);
        WindowId shared_pinned_tabs_window_id{};
        profile_id_get_shared_pinned_tabs_window_id(profile_id, &shared_pinned_tabs_window_id);
        // NOTE: when saving a window, we should duplicate the tabs in shared pinned tabs
        std::string sql = R"(
            SELECT id FROM tabs
            WHERE window_id = ?1
            ORDER BY window_id, seq_num;
        )";

        DbStmt stmt;
        VERIFY(m_db->query(sql, {shared_pinned_tabs_window_id.value}, &stmt));

        while (stmt.next()) {
            DuplicateTabWithNewWindowIdQuery duplicate_tab_query{};
            duplicate_tab_query.id = TabId(stmt.integer("id"));
            duplicate_tab_query.window_id = query.window_id;

            TabDto tab{};
            VERIFY(duplicate_tab_with_new_window_id(duplicate_tab_query, &tab, &tab_change_internal, &window_change_internal, &back_forward_list_change_internal, TxMode::no_tx));
        }
        VERIFY(stmt.error());
    }
    VERIFY(tx.commit());
    tab_change->created.insert(tab_change->created.cend(), tab_change_internal.created.begin(), tab_change_internal.created.end());
    tab_change->deleted.insert(tab_change->deleted.cend(), tab_change_internal.deleted.begin(), tab_change_internal.deleted.end());
    tab_change->updated.insert(tab_change->updated.cend(), tab_change_internal.updated.begin(), tab_change_internal.updated.end());
    tab_change->updated_back_forward_list_index |= tab_change_internal.updated_back_forward_list_index;
    tab_change->updated_icon_name |= tab_change_internal.updated_icon_name;
    tab_change->updated_is_collapsed |= tab_change_internal.updated_is_collapsed;
    tab_change->updated_is_pinned |= tab_change_internal.updated_is_pinned;
    tab_change->updated_is_suspended |= tab_change_internal.updated_is_suspended;
    tab_change->updated_merged_back_forward_list_length |= tab_change_internal.updated_merged_back_forward_list_length;
    tab_change->updated_name |= tab_change_internal.updated_name;
    tab_change->updated_parent_tab_id |= tab_change_internal.updated_parent_tab_id;
    tab_change->updated_seq_num |= tab_change_internal.updated_seq_num;
    tab_change->updated_session_state |= tab_change_internal.updated_session_state;
    tab_change->updated_snapshot_image |= tab_change_internal.updated_snapshot_image;
    tab_change->updated_uri |= tab_change_internal.updated_uri;
    tab_change->updated_webkit_back_forward_list_index |= tab_change_internal.updated_webkit_back_forward_list_index;
    tab_change->updated_window_id |= tab_change_internal.updated_window_id;
    window_change->created.insert(window_change->created.cend(), window_change_internal.created.begin(), window_change_internal.created.end());
    window_change->deleted.insert(window_change->deleted.cend(), window_change_internal.deleted.begin(), window_change_internal.deleted.end());
    window_change->updated.insert(window_change->updated.cend(), window_change_internal.updated.begin(), window_change_internal.updated.end());
    window_change->updated_has_ui_window |= window_change_internal.updated_has_ui_window;
    window_change->updated_icon_name |= window_change_internal.updated_icon_name;
    window_change->updated_is_suspended |= window_change_internal.updated_is_suspended;
    window_change->updated_is_tab_group |= window_change_internal.updated_is_tab_group;
    window_change->updated_keyboard_shortcut |= window_change_internal.updated_keyboard_shortcut;
    window_change->updated_name |= window_change_internal.updated_name;
    window_change->updated_selected_tab_id |= window_change_internal.updated_selected_tab_id;
    window_change->updated_seq_num |= window_change_internal.updated_seq_num;
    window_change->updated_ui_window_id |= window_change_internal.updated_ui_window_id;
    window_change->updated_undo_redo_tab_step_seq_num |= window_change_internal.updated_undo_redo_tab_step_seq_num;
    window_change->updated_is_focus_mode |= window_change_internal.updated_is_focus_mode;
    window_change->updated_is_private |= window_change_internal.updated_is_private;
    back_forward_list_change->created.insert(back_forward_list_change->created.cend(), back_forward_list_change_internal.created.begin(), back_forward_list_change_internal.created.end());
    back_forward_list_change->deleted.insert(back_forward_list_change->deleted.cend(), back_forward_list_change_internal.deleted.begin(), back_forward_list_change_internal.deleted.end());
    back_forward_list_change->updated.insert(back_forward_list_change->updated.cend(), back_forward_list_change_internal.updated.begin(), back_forward_list_change_internal.updated.end());
    SUCCEED();
}

Error
ProfilesDb::update_window(UpdateWindowQuery query, WindowsChange* window_change)
{
    if (query.id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window id is not set");
    }

    WindowDto window{};
    {
        std::vector<WindowDto> windows;
        VERIFY(get_windows({query.id}, &windows));
        if (windows.empty()) {
            RAISE(ErrorCode::not_found, "window with id %lld not found", query.id.value);
        }
        window = windows[0];
    }

    if (query.selected_tab_id.value != 0) {
        std::vector<TabDto> tabs;
        VERIFY(get_tabs({query.selected_tab_id}, &tabs));
        if (tabs.empty()) {
            RAISE(ErrorCode::not_found, "tab with id %lld not found", query.selected_tab_id.value);
        }
    }

    if (query.update_is_suspended && query.is_suspended && !query.update_selected_tab_id) {
        // if the window is being suspended, select the first unpinned tab in it (if any)
        GetTabIdsQuery get_tab_ids_query{
            .window_id = query.id,
        };
        std::vector<TabId> tab_ids;
        VERIFY(get_tab_ids(get_tab_ids_query, &tab_ids));
        std::vector<TabDto> tabs;
        VERIFY(get_tabs(tab_ids, &tabs));
        if (!tabs.empty()) {
            TabId new_selected_tab_id{};
            for (const auto& tab : tabs) {
                if (!tab.is_pinned) {
                    new_selected_tab_id = tab.id;
                    break;
                }
            }
            if (!new_selected_tab_id.value) {
                new_selected_tab_id = tabs[0].id; // all tabs are pinned, select the first one
            }
            if (new_selected_tab_id != window.selected_tab_id) {
                query.update_selected_tab_id = true;
                query.selected_tab_id = new_selected_tab_id;
            }
        }
    }


    if (window_change) {
        window_change->updated_name = query.update_name;
        window_change->updated_icon_name = query.update_icon_name;
        window_change->updated_keyboard_shortcut = query.update_keyboard_shortcut;
        window_change->updated_ui_window_id = query.update_ui_window_id;
        window_change->updated_selected_tab_id = query.update_selected_tab_id;
        window_change->updated_is_tab_group = query.update_is_tab_group;
        window_change->updated_is_suspended = query.update_is_suspended;
        window_change->updated_seq_num = query.update_seq_num;
        window_change->updated_has_ui_window = query.update_has_ui_window;
        window_change->updated_undo_redo_tab_step_seq_num = query.update_undo_redo_tab_step_seq_num;
        window_change->updated_is_focus_mode = query.update_is_focus_mode;
        window_change->updated_is_private = query.update_is_private;
    }

    std::vector<std::string> set_columns;
    std::vector<DbVariant> args;

    if (query.update_name) {
        args.push_back(query.name);
        set_columns.push_back("name");
    }

    if (query.update_icon_name) {
        args.push_back(query.icon_name);
        set_columns.push_back("icon_name");
    }

    if (query.update_keyboard_shortcut) {
        args.push_back(query.keyboard_shortcut);
        set_columns.push_back("keyboard_shortcut");
    }

    if (query.update_selected_tab_id) {
        if (query.selected_tab_id.value) {
            args.push_back(query.selected_tab_id.value);
        } else {
            args.push_back(DbVariant(nullptr)); // use NULL if no tab is selected
        }
        set_columns.push_back("selected_tab_id");

        args.push_back(window.selected_tab_id.value); // use current tab id for prev_selected_tab_id
        set_columns.push_back("prev_selected_tab_id");
    }

    if (query.update_is_tab_group) {
        args.push_back(query.is_tab_group);
        set_columns.push_back("is_tab_group");
    }

    if (query.update_is_suspended) {
        args.push_back(query.is_suspended);
        set_columns.push_back("is_suspended");
    }

    if (query.update_seq_num) {
        double refined_seq_num = 0.0;
        auto refine_query = RefineWindowSeqNumQuery{
            .profile_id = window.profile_id,
            .seq_num = query.seq_num,
            .collision_mode = query.seq_num_collision_mode,
        };
        VERIFY(refine_window_seq_num(refine_query, &refined_seq_num));
        args.push_back(refined_seq_num);
        set_columns.push_back("seq_num");
    }

    if (query.update_has_ui_window) {
        args.push_back(query.has_ui_window);
        set_columns.push_back("has_ui_window");
    }

    if (query.update_undo_redo_tab_step_seq_num) {
        args.push_back(query.undo_redo_tab_step_seq_num);
        set_columns.push_back("undo_redo_tab_step_seq_num");
    }

    if (query.update_is_focus_mode) {
        args.push_back(query.is_focus_mode);
        set_columns.push_back("is_focus_mode");
    }

    if (query.update_is_private) {
        args.push_back(query.is_private);
        set_columns.push_back("is_private");
    }

    if (set_columns.empty()) {
        SUCCEED(); // nothing to update
    }

    args.push_back(query.id.value);

    std::string sql = std::format(R"(
        UPDATE windows SET {}
        WHERE id = ?
        RETURNING {};
    )", Db::join_into_set_clause(set_columns),
        Db::join_strings(get_window_columns(), ", ")
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    WindowDto updated_window{};
    while (stmt.next()) {
        load_window_from_stmt(stmt, &updated_window);

        if (window_change) {
            window_change->updated.emplace_back(updated_window);
        }
    }
    VERIFY(stmt.error());

    // If the window was just given a UI (has_ui_window set to true) and it's a saved
    // window (not a tab group), ensure it contains at least one tab. If it has no
    // tabs, create a Start Page tab (`orion://newtab/`). This mirrors the behavior
    // used when closing windows to ensure saved windows always have a Start Page.
    if (updated_window.id != WindowId(0) && query.update_has_ui_window && query.has_ui_window && !updated_window.is_tab_group) {
        GetTabIdsQuery get_tab_ids_query{ .window_id = query.id };
        std::vector<TabId> tab_ids;
        VERIFY(get_tab_ids(get_tab_ids_query, &tab_ids));
        if (tab_ids.empty()) {
            CreateTabQuery create_tab_query{
                .window_id = query.id,
                .uri = "orion://newtab/",
            };
            TabId new_tab_id{};
            TabsChange tabs_change_internal{};
            VERIFY(create_tab(create_tab_query, &new_tab_id, TxMode::no_tx, &tabs_change_internal, window_change));
        }
    }

    SUCCEED();
}

Error
ProfilesDb::move_window(MoveWindowQuery query, WindowsChange* window_change)
{
    if (query.window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }

    ProfileId profile_id{};
    VERIFY(window_id_get_profile_id(query.window_id, &profile_id));

    double new_seq_num = 0;
    if (query.next_window_id == WindowId(0)) {
        std::string sql = R"(
            SELECT MAX(seq_num) as prev
            FROM windows
            WHERE profile_id = ?1;
        )";

        std::vector<DbVariant> args = {profile_id.value};

        DbStmt stmt{};
        VERIFY(m_db->query(sql, args, &stmt));
        int rc = SQLITE_OK;
        if (stmt.next(&rc)) {
            auto prev = stmt.real_optional("prev");
            new_seq_num = prev.value_or(0) + 8;
        } else {
            RAISE(ErrorCode::db_step, "failed to execute database step, sqlite error code: %d", rc);
        }
    } else {
        std::string sql = R"(
            SELECT
            i1.seq_num as next,
            (
                SELECT MAX(i2.seq_num)
                FROM windows i2
                WHERE i2.profile_id = i1.profile_id
                AND i2.seq_num < i1.seq_num
            ) as prev
            FROM windows i1
            WHERE i1.id = ?1;
        )";

        std::vector<DbVariant> args = {query.next_window_id.value};
        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        if (stmt.next()) {
            double next_seq_num = stmt.real("next");
            auto prev_optional = stmt.real_optional("prev");
            double prev_seq_num = prev_optional.value_or(next_seq_num - 8);
            new_seq_num = (prev_seq_num + next_seq_num) / 2.0;
        } else {
            RAISE(ErrorCode::argument, "failed to find seq nums for inserting before window %lld", query.next_window_id.value);
        }
    }

    UpdateWindowQuery update_query{
        .id = query.window_id,
        .seq_num = new_seq_num,
        .update_seq_num = true,
    };
    VERIFY(update_window(update_query, window_change));
    SUCCEED();
}

Error
ProfilesDb::delete_windows(
    const std::vector<WindowId>& ids, TxMode tx_mode, WindowsChange* change, TabsChange* tabs_change,
    ProfilesChange* profiles_change)
{
    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    std::vector<WindowId> filtered_ids{};
    for(const WindowId& id : ids)
    {
        ProfileId profile_id;
        window_id_get_profile_id(id, &profile_id);
        WindowId shared_pinned_tabs_window_id;
        VERIFY(profile_id_get_shared_pinned_tabs_window_id(profile_id, &shared_pinned_tabs_window_id));
        
        if(shared_pinned_tabs_window_id != id)
        {
            filtered_ids.push_back(id);
        }
    }
    

    {
        // Gather all tab ids that will be deleted along with the windows
        std::string sql = std::format(R"(
            SELECT id FROM tabs
            WHERE window_id IN ({})
            ORDER BY window_id, id;
        )", Db::make_placeholders(filtered_ids.size()));

        DbStmt stmt;
        VERIFY(m_db->query(sql, to_db_variant_vector(filtered_ids), &stmt));
        
        if(tabs_change)
        {
            while (stmt.next()) {
                tabs_change->deleted.push_back(static_cast<TabId>(stmt.integer("id")));
            }
        }
        VERIFY(stmt.error());
    }

    std::string sql = std::format(R"(
        DELETE FROM windows WHERE id IN ({})
        RETURNING id, profile_id, is_private;
    )", Db::make_placeholders(filtered_ids.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, to_db_variant_vector(filtered_ids), &stmt));

    while (stmt.next()) {
        WindowId id = WindowId(stmt.integer("id"));
        ProfileId profile_id = ProfileId(stmt.integer("profile_id"));
        bool is_private = stmt.integer("is_private") != 0;

        if (change) {
            change->deleted.push_back({
                .id = id,
                .profile_id = profile_id,
                .is_private = is_private,
            });
        }
    }
    VERIFY(stmt.error());

    // Reset selection of deleted windows in all applicable profiles
    VERIFY(reset_selected_window_ids(filtered_ids, profiles_change));

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::create_recently_closed_tab_group(WindowDto window, RecentlyClosedTabsChange* change)
{
    UnixTimeUtc64 now = static_cast<UnixTimeUtc64>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    // create recently closed window entry
    RecentlyClosedWindowId new_window_id{};
    {
        CreateRecentlyClosedWindowQuery query{
            .profile_id = window.profile_id,
            .name = window.name,
            .closed_at = now,
            .icon_name = window.icon_name,
        };
        VERIFY(create_recently_closed_window(query, &new_window_id));
    }

    // create recently closed tab entries for all tabs in the window
    {
        GetTabIdsQuery query{
            .window_id = window.id,
        };
        std::vector<TabId> tab_ids;
        VERIFY(get_tab_ids(query, &tab_ids));
        std::vector<TabDto> tabs;
        VERIFY(get_tabs(tab_ids, &tabs));
        for (const auto& tab : tabs) {
            CreateRecentlyClosedTabQuery tab_query{
                .profile_id = window.profile_id,
                .name = tab.name,
                .uri = tab.uri,
                .closed_at = now,
                .icon_name = tab.icon_name,
                .window_id = new_window_id,
            };
            RecentlyClosedTabId new_tab_id{};
            VERIFY(create_recently_closed_tab(tab_query, &new_tab_id, change));
        }
    }
    SUCCEED();
}

Error
ProfilesDb::close_window(
    WindowId window_id, TxMode tx_mode, WindowsChange* change, TabsChange* tabs_change, ProfilesChange* profiles_change,
    RecentlyClosedTabsChange* recently_closed_tabs_change)
{
    if (window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }

    std::vector<WindowDto> windows;
    VERIFY(get_windows({window_id}, &windows));

    if (windows.empty()) {
        log_warning("window with id %lld not found, nothing to close", window_id.value);
        SUCCEED();
    }

    auto window = windows[0];

    DbTx tx;
    if(tx_mode == TxMode::use_tx)
    {
        VERIFY(tx.begin(m_db)); 
    }
    
    if (window.is_tab_group) {
        // Unsaved Tab Group
        
        // Never add private windows to recently closed list
        if (!window.is_private) {
            VERIFY(create_recently_closed_tab_group(window, recently_closed_tabs_change));
        }

        VERIFY(delete_windows({window_id}, TxMode::no_tx, change, tabs_change, profiles_change));
    } else {
        // Saved Tab Group

        // Ensure that Start Page tab is present in it
        GetTabIdsQuery get_tab_ids_query{
            .window_id = window_id,
        };
        std::vector<TabId> tab_ids;
        VERIFY(get_tab_ids(get_tab_ids_query, &tab_ids));
        if (tab_ids.empty()) {
            // no tabs in the window, create a Start Page tab
            CreateTabQuery create_tab_query{
                .window_id = window_id,
                .uri = "orion://newtab/",
            };
            TabId new_tab_id;
            VERIFY(create_tab(create_tab_query, &new_tab_id, TxMode::no_tx, tabs_change, change));
        }

        // Set has_ui_window property of the window to false
        UpdateWindowQuery update_window_query{
            .id = window_id,
            .has_ui_window = false,
            .update_has_ui_window = true,
        };
        VERIFY(update_window(update_window_query, change));
    }

    if(tx_mode == TxMode::use_tx)
    {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::refresh_window_num_tabs(WindowId window_id, WindowsChange* windows_change)
{
    if (window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }

    int64_t actual_num_tabs = 0;
    {
        // ensure the window exists
        std::vector<WindowDto> windows;
        VERIFY(get_windows({window_id}, &windows));
        if (windows.empty()) {
            RAISE(ErrorCode::not_found, "window with id %lld not found", window_id.value);
        }
        int64_t existing_num_tabs = windows[0].num_tabs;
        
        // get current number of tabs in the window
        GetTabIdsQuery get_tab_ids_query{};
        get_tab_ids_query.window_id = window_id;
        std::vector<TabId> tab_ids;
        VERIFY(get_tab_ids(get_tab_ids_query, &tab_ids));
        actual_num_tabs = static_cast<int64_t>(tab_ids.size());

        if (existing_num_tabs == actual_num_tabs) {
            // no change in the number of tabs, nothing to do
            SUCCEED();
        }
    }

    std::string sql = std::format(R"(
        UPDATE windows
        SET num_tabs = ?1
        WHERE id = ?2
        RETURNING {};
    )", Db::join_strings(get_window_columns(), ", "));

    std::vector<DbVariant> args = {
        actual_num_tabs,
        window_id.value,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);
    
    WindowDto window{};
    load_window_from_stmt(stmt, &window);

    if (windows_change) {
        windows_change->updated.emplace_back(window);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::duplicate_window(
    DuplicateWindowQuery query, WindowId* new_window_id, WindowsChange* windows_change, TabsChange* tabs_change)
{
    if (query.window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }

    std::vector<WindowDto> windows;
    VERIFY(get_windows({query.window_id}, &windows));
    if (windows.empty()) {
        RAISE(ErrorCode::not_found, "window with id %lld not found", query.window_id.value);
    }
    auto window = windows[0];

    DbTx tx;
    VERIFY(tx.begin(m_db));

    std::string icon_name = "window-flat-symbolic"; // default icon for tab groups

    if (window.is_private) {
        icon_name = window.icon_name;
    }

    // The new window must be private if and only if the original window is private
    CreateWindowQuery create_window_query{
        .profile_id = window.profile_id,
        .name = query.name,
        .icon_name = icon_name,
        .keyboard_shortcut = "",
        .is_suspended = false,
        .is_tab_group = true, // duplicated window is always a tab group
        .selected_tab_id = TabId(0), // no tab is selected in the new window
        // seq_num will be refined in create_window function
        .has_ui_window = false,
        .undo_redo_tab_step_seq_num = 0,
        .is_focus_mode = false,
        .is_private = window.is_private,
    };

    VERIFY(create_window(create_window_query, new_window_id, windows_change));

    {
        // duplicate all tabs from the original window into the new one
        GetTabIdsQuery get_tab_ids_query{};
        get_tab_ids_query.window_id = window.id;
        std::vector<TabId> tab_ids;
        VERIFY(get_tab_ids(get_tab_ids_query, &tab_ids));

        TabId new_selected_tab_id{};

        for (const auto& tab_id : tab_ids) {
            std::vector<TabDto> tabs;
            VERIFY(get_tabs({tab_id}, &tabs));
            if (tabs.empty()) {
                RAISE(ErrorCode::assert, "tab with id %lld not found", tab_id.value);
            }
            auto tab = tabs[0];

            //skip duplicating all pinned tabs
            if(tab.is_pinned) continue;
            
            // do not copy history/session state
            CreateTabQuery create_tab_query{
                .window_id = *new_window_id,
                .name = tab.name,
                .icon_name = tab.icon_name,
                .is_pinned = tab.is_pinned,
                .is_suspended = tab.is_suspended,
                .seq_num = tab.seq_num, // will be refined in create_tab function
                .uri = tab.uri,
                .session_state = {}, // do not copy session state
            };

            TabId new_tab_id{};
            VERIFY(create_tab(create_tab_query, &new_tab_id, TxMode::no_tx, tabs_change, windows_change));

            if (tab.id == window.selected_tab_id) {
                // remember the new tab id that corresponds to the selected tab in the original window
                new_selected_tab_id = new_tab_id;
            }
        }

        if (new_selected_tab_id != TabId(0)) {
            // select the corresponding tab in the new window
            UpdateWindowQuery update_window_query{
                .id = *new_window_id,
                .selected_tab_id = new_selected_tab_id,
                .update_selected_tab_id = true,
            };

            VERIFY(update_window(update_window_query, windows_change));
        } else {
            log_warning("duplicate_window: selected tab %lld in the original window not found among its tabs",
                window.selected_tab_id.value
            );
        }
    }

    VERIFY(tx.commit());
    SUCCEED();
}

Error
ProfilesDb::ensure_window_state_after_tab_move(
    WindowId window_id, TxMode tx_mode, WindowsChange* change, TabsChange* tabs_change,
    ProfilesChange* profiles_change, RecentlyClosedTabsChange* recently_closed_tabs_change)
{
    DbTx tx{};
    if(tx_mode == TxMode::use_tx)
    {
        VERIFY(m_db->begin(&tx));
    }

    bool is_tab_group = false;
    bool is_empty = false;
    {
        std::string sql = R"(
            SELECT is_tab_group
            FROM windows
            WHERE id = ?1;
        )";
        std::vector<DbVariant> args{window_id.value};

        VERIFY(m_db->query_bool(sql, args, &is_tab_group));
    }
    {
        int64_t tab_count = 0;
        std::string sql = R"(
            SELECT COUNT(*)
            FROM tabs
            WHERE window_id = ?1;
        )";
        std::vector<DbVariant> args{window_id.value};
        
        VERIFY(m_db->query_integer(sql, args, &tab_count));

        if(is_tab_group)
        {
            int64_t shared_pinned_tabs_count = 0;

            orion::ProfileId profile_id{};
            orion::WindowId shared_pinned_window_id{};
            VERIFY(window_id_get_profile_id(window_id, &profile_id));
            VERIFY(profile_id_get_shared_pinned_tabs_window_id(profile_id, &shared_pinned_window_id));
            std::string sql = R"(
                SELECT COUNT(*)
                FROM tabs
                WHERE window_id = ?1;
            )";
            std::vector<DbVariant> args{shared_pinned_window_id.value};

            VERIFY(m_db->query_integer(sql, args, &shared_pinned_tabs_count));
            tab_count += shared_pinned_tabs_count;
        }
        if(tab_count == 0)
        {
            is_empty = true;
        }
    }
    if(is_empty)
    {
        if(is_tab_group)
        {
            VERIFY(close_window(window_id, TxMode::no_tx, change, tabs_change, profiles_change, recently_closed_tabs_change));
        }
        else
        {
            CreateTabQuery query{};
            query.window_id = window_id;
            query.uri = "orion://newtab/";
            VERIFY(create_tab(query, nullptr, TxMode::no_tx, tabs_change, change));
        }
    }


    if(tx_mode == TxMode::use_tx)
    {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::verify_windows_change(WindowsChange* change)
{
#if VERIFY_WINDOWS_CHANGE
    if(change->updated.empty())
    {
        SUCCEED();
    }
    std::ostringstream json_array_builder;
    json_array_builder << "[";
    json_array_builder << change->updated.at(0).id.value;
    for(size_t i = 0; i<change->updated.size(); i++)
    {
        json_array_builder << "," << change->updated.at(i).id.value;
    }
    json_array_builder << "]";
    std::string json_array = json_array_builder.str();
    
    std::string sql = std::format(R"(
        SELECT EXISTS
        (
            SELECT *
            FROM profiles, json_each(?1)
            WHERE profiles.shared_pinned_tabs_window_id = json_each.value
        );
    )");
    std::vector<DbVariant> args = {json_array};
    bool includes_shared_pinned_tabs = false;
    VERIFY(m_db->query_bool(sql, args, &includes_shared_pinned_tabs));
    if(includes_shared_pinned_tabs)
    {
        RAISE(ErrorCode::assert, "shared pinned tabs window found in updated windows");
    }
#endif
    SUCCEED();
}

} // namespace orion
