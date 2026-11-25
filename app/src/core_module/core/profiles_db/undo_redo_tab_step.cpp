#include <core/profiles_db/profiles_db.h>
#include <format>
#include <chrono>

namespace orion {

std::vector<std::string>
ProfilesDb::get_undo_redo_tab_step_columns() const
{
    return {
        "id",
        "window_id",
        "seq_num",
    };
}

void
ProfilesDb::load_undo_redo_tab_step_from_stmt(const DbStmt& stmt, UndoRedoTabStepDto* step) const
{
    step->id = UndoRedoTabStepId(stmt.integer("id"));
    step->window_id = WindowId(stmt.integer("window_id"));
    step->seq_num = stmt.integer("seq_num");
}

Error
ProfilesDb::create_undo_redo_tab_step(
    CreateUndoRedoTabStepQuery query, TxMode tx_mode, UndoRedoTabStepId* id, WindowsChange* windows_change)
{
    if (query.window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    // check that window exists
    WindowDto window{};
    {
        std::vector<WindowDto> windows;
        VERIFY(get_windows({query.window_id}, &windows));
        if (windows.empty()) {
            RAISE(ErrorCode::not_found, "window with id %lld not found", query.window_id.value);
        }
        window = windows[0];
    }

    // delete redo steps
    {
        std::string sql = R"(
            DELETE FROM undo_redo_tab_steps
            WHERE window_id = ?1 AND seq_num > ?2;
        )";

        std::vector<DbVariant> args = {
            query.window_id.value,
            window.undo_redo_tab_step_seq_num,
        };

        VERIFY(m_db->exec(sql, args));
    }

    // fix possible inconsistent seq_num in window (created by manual delete of undo_redo_tab_steps)
    {
        std::vector<UndoRedoTabStepId> step_ids;
        VERIFY(get_undo_redo_tab_step_ids(window.id, &step_ids));
        std::vector<UndoRedoTabStepDto> steps;
        VERIFY(get_undo_redo_tab_steps(step_ids, &steps));

        std::vector<UndoRedoTabStepDto> valid_steps;
        for (const auto& step : steps) {
            if (step.seq_num <= window.undo_redo_tab_step_seq_num) {
                valid_steps.push_back(step);
            }
        }

        if (valid_steps.empty()) {
            window.undo_redo_tab_step_seq_num = 0;
        } else {
            // set window's seq_num to the highest step's seq_num that 
            window.undo_redo_tab_step_seq_num = valid_steps.back().seq_num;
        }
    }

    // shift window's undo_redo_tab_step_seq_num
    {
        window.undo_redo_tab_step_seq_num += 1;

        UpdateWindowQuery query{
            .id = window.id,
            .undo_redo_tab_step_seq_num = window.undo_redo_tab_step_seq_num,
            .update_undo_redo_tab_step_seq_num = true,
        };

        VERIFY(update_window(query, windows_change));
    }

    std::string sql = std::format(R"(
        INSERT INTO undo_redo_tab_steps ({})
        VALUES ({})
        RETURNING {};
    )", Db::join_strings(get_undo_redo_tab_step_columns(), ", "),
        Db::make_placeholders(get_undo_redo_tab_step_columns().size()),
        Db::join_strings(get_undo_redo_tab_step_columns(), ", ")
    );

    std::vector<DbVariant> args = {
        nullptr, // id will be auto-generated
        query.window_id.value,
        window.undo_redo_tab_step_seq_num,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    UndoRedoTabStepDto new_step{};
    load_undo_redo_tab_step_from_stmt(stmt, &new_step);
    VERIFY(stmt.finalize());

    if (id) {
        *id = new_step.id;
    }

    // load back-forward list entries for all tabs
    std::map<TabId, std::vector<TabBackForwardListEntryDto>> tab_bf_lists;
    for (const auto& tab_id : query.tab_ids) {
        std::vector<TabBackForwardListEntryDto> entries;
        VERIFY(get_tab_back_forward_list_entries(tab_id, &entries));
        tab_bf_lists[tab_id] = entries;
    }

    // create undo_redo_tabs
    for (const auto& tab_id : query.tab_ids) {
        // load original tab info
        TabDto tab{};
        {
            std::vector<TabDto> tabs;
            VERIFY(get_tabs({tab_id}, &tabs));
            RAISE_CHECK(!tabs.empty());
            tab = tabs[0];
        }
        UndoRedoTabId undo_redo_tab_id{};
        {
            CreateUndoRedoTabQuery query{
                .undo_redo_step_id = new_step.id,
                .original_tab_id = tab_id,
                .original_parent_tab_id = tab.parent_tab_id,
                .original_seq_num = tab.seq_num,
                .name = tab.name,
                .uri = tab.uri,
                .is_pinned = tab.is_pinned,
                .icon_name = tab.icon_name,
                .session_state = tab.session_state,
            };

            VERIFY(create_undo_redo_tab(query, &undo_redo_tab_id));
        }

        // create back-forward list entries
        auto it = tab_bf_lists.find(tab_id);
        if (it == tab_bf_lists.end()) {
            RAISE(ErrorCode::assert, "back-forward list entries for tab id %lld not found", tab_id.value);
        }
        for (const TabBackForwardListEntryDto& entry : it->second) {
            CreateUndoRedoTabBackForwardListEntryQuery query{
                .undo_redo_tab_id = undo_redo_tab_id,
                .webkit_index = entry.webkit_index,
                .uri = entry.uri,
            };

            UndoRedoTabBackForwardListEntryId entry_id;
            VERIFY(create_undo_redo_tab_back_forward_list_entry(query, &entry_id));
        }
    }

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::is_undo_redo_tab_step_available(
    IsUndoRedoTabStepAvailableQuery query, IsUndoRedoTabStepAvailableReply* reply)
{
    if (query.window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }
    RAISE_CHECK(reply != nullptr);

    // get window info
    WindowDto window{};
    {
        std::vector<WindowDto> windows;
        VERIFY(get_windows({query.window_id}, &windows));
        if (windows.empty()) {
            RAISE(ErrorCode::not_found, "window not found");
        }
        window = windows[0];
    }

    for (bool is_undo : {true, false}) {
        std::string where_clause = "seq_num > ?2"; // redo
        if (is_undo) {
            where_clause = "seq_num <= ?2";
        }

        std::string sql = std::format(R"(
            SELECT 1
            FROM undo_redo_tab_steps
            WHERE window_id = ?1 AND {};
        )", where_clause);

        std::vector<DbVariant> args = {
            query.window_id.value,
            window.undo_redo_tab_step_seq_num,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        if (is_undo) {
            reply->is_undo_available = stmt.next();
        } else {
            reply->is_redo_available = stmt.next();
        }
        VERIFY(stmt.error());
    }

    SUCCEED();
}

Error
ProfilesDb::change_undo_redo_tab_step(
    ChangeUndoRedoTabStepQuery query, TxMode tx_mode, WindowsChange* windows_change, TabsChange* tabs_change,
    TabBackForwardListEntriesChange* back_forward_entries_change)
{
    if (query.window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    // get window info
    WindowDto window{};
    {
        std::vector<WindowDto> windows;
        VERIFY(get_windows({query.window_id}, &windows));
        if (windows.empty()) {
            RAISE(ErrorCode::not_found, "window not found");
        }
        window = windows[0];
    }

    // check that corresponding undo/redo step is available
    {
        IsUndoRedoTabStepAvailableQuery avail_query{
            .window_id = query.window_id,
        };
        IsUndoRedoTabStepAvailableReply reply{};
        VERIFY(is_undo_redo_tab_step_available(avail_query, &reply));
        bool is_available = query.is_undo ? reply.is_undo_available : reply.is_redo_available;
        if (!is_available) {
            RAISE(ErrorCode::not_found, "corresponding undo/redo step not available for window id %lld",
                query.window_id.value
            );
        }
    }

    // adjust seq_num
    int64_t target_seq_num{};
    if (query.is_undo) {
        target_seq_num = window.undo_redo_tab_step_seq_num; // undo: before decrement
        window.undo_redo_tab_step_seq_num -= 1;
    } else {
        window.undo_redo_tab_step_seq_num += 1;
        target_seq_num = window.undo_redo_tab_step_seq_num; // redo: after increment
    }

    // load the corresponding undo_redo_tab_step
    UndoRedoTabStepDto step{};
    {
        UndoRedoTabStepId step_id{};
        bool exists{};
        VERIFY(get_undo_redo_tab_step_id(window.id, target_seq_num, &step_id, &exists));
        if (!exists) {
            RAISE(ErrorCode::not_found, "undo_redo_tab_step not found for window id %lld and seq_num %lld",
                window.id.value, target_seq_num
            );
        }
        std::vector<UndoRedoTabStepDto> steps;
        VERIFY(get_undo_redo_tab_steps({step_id}, &steps));
        RAISE_CHECK(!steps.empty());
        step = steps[0];
    }

    // load undo_redo_tabs for the step
    std::vector<UndoRedoTabDto> undo_redo_tabs;
    {
        std::vector<UndoRedoTabId> undo_redo_tab_ids;
        VERIFY(get_undo_redo_tab_ids(step.id, &undo_redo_tab_ids));
        VERIFY(get_undo_redo_tabs(undo_redo_tab_ids, &undo_redo_tabs));
    }

    if (query.is_undo) {
        // restore tabs from undo tabs
        std::map<TabId, TabId> old_to_new_tab_ids;
        for (const UndoRedoTabDto& undo_redo_tab : undo_redo_tabs) {
            TabId new_tab_id{};

            // determine parent_tab_id
            {
                std::vector<TabDto> parent_tabs;
                VERIFY(get_tabs({undo_redo_tab.original_parent_tab_id}, &parent_tabs));
                TabId parent_tab_id = TabId(0);

                auto it = old_to_new_tab_ids.find(undo_redo_tab.original_parent_tab_id);
                if (it != old_to_new_tab_ids.end()) {
                    parent_tab_id = it->second;
                } else if (!parent_tabs.empty()) {
                    parent_tab_id = parent_tabs[0].id;
                }
                
                CreateTabQuery create_tab_query{
                    .window_id = window.id,
                    .name = undo_redo_tab.name,
                    .icon_name = undo_redo_tab.icon_name,
                    .is_pinned = undo_redo_tab.is_pinned,
                    .seq_num = undo_redo_tab.original_seq_num,
                    .uri = undo_redo_tab.uri,
                    .session_state = undo_redo_tab.session_state,
                    .parent_tab_id = parent_tab_id,
                    .back_forward_list_index = undo_redo_tab.back_forward_list_index,
                    .webkit_back_forward_list_index = undo_redo_tab.webkit_back_forward_list_index,
                    .merged_back_forward_list_length = undo_redo_tab.merged_back_forward_list_length,
                };

                VERIFY(create_tab(create_tab_query, &new_tab_id, TxMode::no_tx, tabs_change, windows_change));
                old_to_new_tab_ids[undo_redo_tab.original_tab_id] = new_tab_id;

                // update original_tab_id in undo_redo_tab (needed for potential redo)
                UpdateUndoRedoTabQuery update_query{
                    .id = undo_redo_tab.id,
                    .original_tab_id = new_tab_id,
                    .update_original_tab_id = true,
                };
                VERIFY(update_undo_redo_tab(update_query));
            }

            // restore back-forward list entries
            {
                std::vector<UndoRedoTabBackForwardListEntryDto> brf_entries;
                VERIFY(get_undo_redo_tab_back_forward_list_entries(undo_redo_tab.id, &brf_entries));
                for (const auto& brf_entry : brf_entries) {
                    CreateTabBackForwardListEntryQuery create_brf_entry_query{
                        .tab_id = new_tab_id,
                        .webkit_index = brf_entry.webkit_index,
                        .uri = brf_entry.uri,
                    };

                    TabBackForwardListEntryDto new_entry{};
                    VERIFY(create_tab_back_forward_list_entry(
                        create_brf_entry_query, &new_entry, back_forward_entries_change
                    ));
                }
            }
        }
    } else {
        // close tabs corresponding to redo tabs
        std::vector<TabId> delete_tab_ids;
        for (const UndoRedoTabDto& undo_redo_tab : undo_redo_tabs) {
            delete_tab_ids.push_back(undo_redo_tab.original_tab_id);
        }

        // avoid starting a nested transaction
        TxMode delete_tabs_tx_mode = tx_mode == TxMode::use_tx ? TxMode::no_tx : TxMode::no_tx;
        VERIFY(delete_tabs(delete_tab_ids, delete_tabs_tx_mode, tabs_change, windows_change));
    }

    {
        UpdateWindowQuery update_query{
            .id = window.id,
            .undo_redo_tab_step_seq_num = window.undo_redo_tab_step_seq_num,
            .update_undo_redo_tab_step_seq_num = true,
        };

        VERIFY(update_window(update_query, windows_change));
    }

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::get_undo_redo_tab_steps(
    const std::vector<UndoRedoTabStepId>& ids, std::vector<UndoRedoTabStepDto>* steps)
{
    RAISE_CHECK(steps != nullptr);

    std::string sql = std::format(R"(
        SELECT {}
        FROM undo_redo_tab_steps
        WHERE id IN ({});
    )", Db::join_strings(get_undo_redo_tab_step_columns(), ", "),
        Db::make_placeholders(ids.size())
    );

    std::vector<DbVariant> args;
    for (const auto& id : ids) {
        args.push_back(id.value);
    }

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        UndoRedoTabStepDto step{};
        load_undo_redo_tab_step_from_stmt(stmt, &step);
        steps->push_back(step);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_undo_redo_tab_step_id(
    WindowId window_id, int64_t seq_num, UndoRedoTabStepId* id, bool* exists)
{
    RAISE_CHECK(id != nullptr);

    std::string sql = std::format(R"(
        SELECT id
        FROM undo_redo_tab_steps
        WHERE window_id = ?1 AND seq_num = ?2;
    )");

    std::vector<DbVariant> args = {
        window_id.value,
        seq_num,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    bool has_row = stmt.next();
    VERIFY(stmt.error());
    if (exists) {
        *exists = has_row;
    }
    if (has_row) {
        *id = UndoRedoTabStepId(stmt.integer("id"));
    }
    SUCCEED();
}

Error
ProfilesDb::get_undo_redo_tab_step_ids(
    WindowId window_id, std::vector<UndoRedoTabStepId>* ids)
{
    RAISE_CHECK(ids != nullptr);

    std::string sql = std::format(R"(
        SELECT id
        FROM undo_redo_tab_steps
        WHERE window_id = ?1
        ORDER BY seq_num ASC;
    )");

    std::vector<DbVariant> args = {
        window_id.value,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        UndoRedoTabStepId id{stmt.integer("id")};
        ids->push_back(id);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

} // namespace orion