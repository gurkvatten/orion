#define EXTRA_VALIDATION 1
#define ENABLE_VERBOSE_LOGS 0

#include <core/profiles_db/profiles_db.h>
#include <chrono>
#include <format>
#include <unordered_set>
#include <inttypes.h>

namespace orion {

//-------------------------------------------------------------------------
// Tabs
//-------------------------------------------------------------------------

std::vector<std::string>
ProfilesDb::get_tab_columns() const
{
    return {
        "id",
        "window_id",
        "name",
        "icon_name",
        "is_pinned",
        "is_suspended",
        "seq_num",
        "uri",
        "session_state",
        "parent_tab_id",
        "is_collapsed",
        "back_forward_list_index",
        "webkit_back_forward_list_index",
        "merged_back_forward_list_length",
        "snapshot_image",
    };
}

void
ProfilesDb::load_tab_from_stmt(const DbStmt& stmt, TabDto* tab) const
{
    tab->id = TabId(stmt.integer("id"));
    tab->window_id = WindowId(stmt.integer("window_id"));
    tab->name = stmt.text("name");
    tab->icon_name = stmt.text("icon_name");
    tab->is_pinned = stmt.integer("is_pinned");
    tab->is_suspended = stmt.integer("is_suspended");
    tab->seq_num = stmt.real("seq_num");
    tab->uri = stmt.text("uri");
    tab->session_state = stmt.blob("session_state");
    tab->parent_tab_id = TabId(stmt.integer("parent_tab_id"));
    tab->is_collapsed = stmt.integer("is_collapsed");
    tab->back_forward_list_index = stmt.integer("back_forward_list_index");
    tab->webkit_back_forward_list_index = stmt.integer("webkit_back_forward_list_index");
    tab->merged_back_forward_list_length = stmt.integer("merged_back_forward_list_length");
    tab->snapshot_image = stmt.blob("snapshot_image");
}

Error
ProfilesDb::get_tab_ids(GetTabIdsQuery query, std::vector<TabId>* ids)
{
    if (query.window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }

    std::string sql = R"(
        SELECT id FROM tabs
        WHERE window_id = ?1
        ORDER BY window_id, seq_num;
    )";

    DbStmt stmt;
    VERIFY(m_db->query(sql, {query.window_id.value}, &stmt));

    while (stmt.next()) {
        auto tab_id = TabId(stmt.integer("id"));
        ids->push_back(tab_id);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_other_tab_ids(
    GetOtherTabIdsQuery query, std::vector<TabId>* other_ids, std::vector<TabId>* before_ids,
    std::vector<TabId>* after_ids)
{
    if (query.exclude_tab_id == TabId(0)) {
        RAISE(ErrorCode::argument, "exclude_tab_id is not set");
    }
    RAISE_CHECK(other_ids);
    RAISE_CHECK(before_ids);
    RAISE_CHECK(after_ids);

    WindowId window_id{};
    {
        // get corresponding window_id of the exclude_tab_id
        std::vector<TabDto> tabs;
        VERIFY(get_tabs({query.exclude_tab_id}, &tabs));
        if (tabs.empty()) {
            RAISE(ErrorCode::not_found, "tab with id %lld not found", query.exclude_tab_id.value);
        }
        window_id = tabs[0].window_id;
    }

    std::string where_clause = "WHERE window_id = ?1";

    if (query.exclude_pinned_tabs) {
        where_clause += " AND is_pinned = 0";
    }

    if (query.exclude_suspended_tabs) {
        where_clause += " AND is_suspended = 0";
    }

    std::string sql = std::format(R"(
        SELECT id FROM tabs
        {}
        ORDER BY window_id, seq_num;
    )", where_clause);

    DbStmt stmt;
    VERIFY(m_db->query(sql, {window_id.value}, &stmt));

    bool is_exclude_tab_id_found = false;

    while (stmt.next()) {
        auto tab_id = TabId(stmt.integer("id"));

        if (tab_id == query.exclude_tab_id) {
            is_exclude_tab_id_found = true;
            continue; // skip the exclude_tab_id
        }

        other_ids->push_back(tab_id);

        if (!is_exclude_tab_id_found) {
            before_ids->push_back(tab_id);
        } else {
            after_ids->push_back(tab_id);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_descendant_tab_ids(TabId tab_id, std::vector<TabId>* descendant_ids)
{
    if (tab_id == TabId(0)) {
        RAISE(ErrorCode::argument, "tab_id is not set");
    }
    RAISE_CHECK(descendant_ids);

    std::string sql = R"(
        WITH RECURSIVE descendant_tabs AS (
            SELECT id, seq_num FROM tabs WHERE parent_tab_id = ?1
            UNION ALL
            SELECT t.id, t.seq_num FROM tabs t
            INNER JOIN descendant_tabs dt ON t.parent_tab_id = dt.id
        )
        SELECT id, seq_num FROM descendant_tabs
		ORDER BY seq_num;
    )";

    DbStmt stmt;
    VERIFY(m_db->query(sql, {tab_id.value}, &stmt));

    while (stmt.next()) {
        TabId descendant_id{stmt.integer("id")};
        descendant_ids->push_back(descendant_id);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_tab_index(WindowId window_id, double seq_num, int64_t* index)
{
    if (window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }
    RAISE_CHECK(index);

    // count how many tabs in the same window have a lower seq_num
    int64_t count = 0;
    VERIFY(m_db->query_integer(R"(
        SELECT COUNT(*) AS count FROM tabs
        WHERE window_id = ?1 AND seq_num < ?2;
    )", {window_id.value, seq_num}, &count));

    *index = count;
    SUCCEED();
}

Error
ProfilesDb::get_tabs(const std::vector<TabId>& ids, std::vector<TabDto>* tabs)
{
    auto args = to_db_variant_vector(ids);
    std::string sql = std::format(R"(
        SELECT {}
        FROM tabs
        WHERE id IN ({})
        ORDER BY window_id, seq_num;
    )", Db::join_strings(get_tab_columns(), ", "),
        Db::make_placeholders(ids.size())
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto tab = TabDto{};
        load_tab_from_stmt(stmt, &tab);
        int64_t tab_index = 0;
        VERIFY(get_tab_index(tab.window_id, tab.seq_num, &tab_index));
        tab.index = tab_index;

        tabs->emplace_back(tab);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

// See also ProfilesDb::normalize_bookmark_seq_num
Error
ProfilesDb::normalize_tab_seq_nums(WindowId window_id, TxMode tx_mode, TabsChange* change)
{
    std::chrono::steady_clock::time_point started_at = std::chrono::steady_clock::now();

    if (window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }

    DbTx tx{};
    if (tx_mode == TxMode::use_tx) {
        VERIFY(m_db->begin(&tx));
    }

    // We have to drop the index before updating the seq_num in bulk, because otherwise we could violate
    // the unique constraint on seq_num midway through the update. This cannot be solved easily by using
    // a temporary column, nor it can be solved by using a temporary table because the order of updates
    // of each record is not guaranteed (and cannot be ordered easily). Then after the update, we recreate the
    // dropped index - all this can be done atomically in a transaction.

    // Query the index SQL to be able to recreate it after the update
    std::string index_sql;
    VERIFY(m_db->query_text(R"(
        SELECT sql FROM sqlite_master WHERE type = 'index' AND name = 'idx_unique_tab_seq_num';
    )", {}, &index_sql));

    // Also note that all statements that happened before this (potential) transaction started must already
    // be finalized, otherwise we will get "database table is locked" error when trying to drop the index.
    VERIFY(m_db->exec(R"(
        DROP INDEX idx_unique_tab_seq_num;
    )"));

    std::string sql = R"(
        SELECT id, seq_num FROM tabs
        WHERE window_id = ?1
        ORDER BY seq_num;
    )";

    DbStmt stmt{};
    VERIFY(m_db->query(sql, {window_id.value}, &stmt));

    double next_seq_num = -1.0;
    int change_tab_count = 0;

    while (stmt.next()) {
        // The increment value can be adjusted but 1.0 is probably a good default
        next_seq_num += 1.0;

        TabId tab_id{stmt.integer("id")};
        double old_seq_num = stmt.real("seq_num");

        if (old_seq_num == next_seq_num) {
            // Nothing to do, continue with the next tab
            continue;
        }

        // update the tab with the new normalized seq_num
        std::string sql = std::format(R"(
            UPDATE tabs SET seq_num = ?1 WHERE id = ?2
            RETURNING {};
        )", Db::join_strings(get_tab_columns(), ", "));

        DbStmt update_stmt{};
        VERIFY(m_db->query(sql, {next_seq_num, tab_id.value}, &update_stmt));

        // update_stmt should return exactly one row, which is the updated tab
        bool has_row = update_stmt.next();
        VERIFY(update_stmt.error());
        RAISE_CHECK(has_row);

        TabDto updated_tab{};
        load_tab_from_stmt(update_stmt, &updated_tab);

        if (change) {
            // The tab change record may already be present
            auto it = std::find_if(change->updated.begin(), change->updated.end(), [&](const TabDto& t) {
                return t.id == updated_tab.id;
            });
            if (it != change->updated.end()) {
                // update the existing change record's seq_num
                it->seq_num = updated_tab.seq_num;
            } else {
                // add a new change record
                change->updated.push_back(updated_tab);
            }
            change->updated_seq_num = true;
            change_tab_count++;
        }
    }
    VERIFY(stmt.error());

    // Recreate the index after the update
    VERIFY(m_db->exec(index_sql));

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }

    orion::log_verbose("normalize_tab_seq_nums: window_id=%lld changed_tabs=%d took_ms=%lld",
        window_id.value,
        change_tab_count,
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at).count()
    );

    SUCCEED();
}

// On collisions, tries to insert after the existing item with the specified seq_num
Error
ProfilesDb::refine_tab_seq_num(RefineTabSeqNumQuery query, double* seq_num)
{
    RAISE_CHECK(seq_num);

    // selects 3 columns:
    // 1. id_exists - whether the seq_num exists in the table
    // 2. lower_seq_num - sequence number of existing item that directly precedes seq_num (NULL if no such item exists)
    // 3. higher_seq_num - sequence number of existing item that directly follows seq_num (NULL if no such item exists)
    std::string sql = R"(
        WITH combined AS (
            SELECT seq_num FROM tabs
            WHERE window_id = ?1
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
        query.window_id.value,
        query.seq_num,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    bool has_row = stmt.next();
    VERIFY(stmt.error());
    if (!has_row) {
        RAISE(ErrorCode::assert, "refine_tab_seq_num query returned no rows");
    }
    bool id_exists = stmt.integer("id_exists") != 0;
    std::optional<double> higher_seq_num = stmt.real_optional("higher_seq_num");

    if (!id_exists) {
        // seq_num is not used by any existing item, so it can be used as is
        *seq_num = query.seq_num;
        SUCCEED();
    }

    // try to insert after the existing item
    if (higher_seq_num.has_value()) {
        // set seq_num to midpoint between the existing seq_num and the one after it
        *seq_num = average(query.seq_num, higher_seq_num.value());
    } else {
        // no items after the existing one
        *seq_num = query.seq_num + 1.0;
    }
    SUCCEED();
}

Error
ProfilesDb::create_tab(
    CreateTabQuery query, TabId* tab_id, TxMode tx_mode, TabsChange* change, WindowsChange* window_change)
{
    return create_tab(
        allocate_tab_id(), std::move(query), tab_id, tx_mode, change, window_change);
}

Error
ProfilesDb::create_tab(
    TabId new_tab_id, CreateTabQuery query, TabId* tab_id, TxMode tx_mode,
    TabsChange* change, WindowsChange* window_change)
{
    if (new_tab_id == TabId(0)) {
        RAISE(ErrorCode::argument, "tab_id is not set");
    }
    track_tab_id(new_tab_id);

    if (query.window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window_id is not set");
    }

    if(query.is_pinned)
    {
        bool is_tab_group;

        VERIFY(m_db->query_bool(R"(
            SELECT is_tab_group
            FROM windows
            WHERE id = ?1;
        )", {query.window_id.value}, &is_tab_group));

        if(is_tab_group)
        {
            ProfileId profile_id;
            VERIFY(window_id_get_profile_id(query.window_id, &profile_id));
            VERIFY(profile_id_get_shared_pinned_tabs_window_id(profile_id, &query.window_id));
        }
    }

    std::vector<TabDto> parent_tabs;
    const TabDto* parent_tab = nullptr;
    if (query.parent_tab_id != TabId(0)) {
        VERIFY(get_tabs({query.parent_tab_id}, &parent_tabs));
        if (parent_tabs.empty()) {
            RAISE(ErrorCode::not_found, "parent tab with id %lld not found", query.parent_tab_id.value);
        }
        parent_tab = &parent_tabs[0];
        if (parent_tab->is_pinned) {
            RAISE(ErrorCode::argument, "cannot set parent_tab_id to a pinned tab (tab id %lld)", parent_tab->id.value);
        }
    }

    if (!query.seq_num.has_value()) {
        // find the next sequence number for the tab (max of existing + 1, or 0 if no tabs exist)
        if(!parent_tab)
        {
            double next_seq_num = 0.0;
            VERIFY(m_db->query_real(R"(
                SELECT IFNULL(MAX(seq_num) + 1, 0.0) AS next_seq_num
                FROM tabs
                WHERE window_id = ?1;
            )", {query.window_id.value}, &next_seq_num));
            query.seq_num = next_seq_num;
        }
        else
        {
            double parent_seq_num = 0.0;
            VERIFY(m_db->query_real(R"(
                    SELECT seq_num AS parent_seq_num
                    FROM tabs
                    WHERE id = ?1;
                )", {query.parent_tab_id.value}, &parent_seq_num));
            double next_tab_seq_num = 0.0;
            VERIFY(m_db->query_real(R"(
                    SELECT IFNULL(MIN(seq_num), 0.0) AS next_tab_seq_num
                    FROM tabs
                    WHERE seq_num > ?1;
                )", {parent_seq_num}, &next_tab_seq_num));
            
            if(parent_seq_num < next_tab_seq_num)
            {
                query.seq_num = (parent_seq_num+next_tab_seq_num)/2;
            }
            else
            {
                //this means that there is no tab after the parent
                query.seq_num = (parent_seq_num + 1);
            }
        }
    } else {
        // insert the tab at the specified position
        auto refine_query = RefineTabSeqNumQuery{
            .window_id = query.window_id,
            .seq_num = query.seq_num.value(),
        };
        double refined_seq_num = 0.0;
        VERIFY(refine_tab_seq_num(refine_query, &refined_seq_num));
        query.seq_num = refined_seq_num;

        if(query.parent_tab_id == TabId(0))
        {
            TabId next_tab_parent_id{};
            {
                std::string sql = std::format(R"(
                    SELECT parent_tab_id from tabs
                    WHERE seq_num > ?1 and window_id = ?2
                    ORDER BY seq_num ASC
                    LIMIT 1;
                )");

                orion::Error err = m_db->query_integer(sql, {refined_seq_num, query.window_id.value}, &next_tab_parent_id.value);
                if(err.code() != orion::ErrorCode::db_no_rows) VERIFY(err);
            }

            if(next_tab_parent_id != TabId(0))
            {
                orion::log_debug("create_tab(): attempted to create tab with no parent id but the tab after does have a parent id set");
                query.parent_tab_id = next_tab_parent_id;
            }
        }
    }


    std::string sql = std::format(R"(
        INSERT INTO tabs ({})
        VALUES ({})
        RETURNING {};
    )",
        Db::join_strings(get_tab_columns(), ", "),
        Db::make_placeholders(get_tab_columns().size()),
        Db::join_strings(get_tab_columns(), ", ")
    );

    std::vector<DbVariant> args = {
        new_tab_id.value,
        query.window_id.value,
        query.name,
        query.icon_name,
        query.is_pinned,
        query.is_suspended,
        query.seq_num.value(),
        query.uri,
        query.session_state,
        query.parent_tab_id.value,
        query.is_collapsed,
        query.back_forward_list_index,
        query.webkit_back_forward_list_index,
        query.merged_back_forward_list_length,
        query.snapshot_image,
    };

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(m_db->begin(&tx));
    }

    TabDto tab{};
    {
        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        bool has_row = stmt.next();
        VERIFY(stmt.error());
        RAISE_CHECK(has_row);

        VERIFY(refresh_window_num_tabs(query.window_id, window_change));

        load_tab_from_stmt(stmt, &tab);
        int64_t tab_index = 0;
        VERIFY(get_tab_index(tab.window_id, tab.seq_num, &tab_index));
        tab.index = tab_index;

        if (change) {
            change->created.emplace_back(tab);
        }

        if (tab_id) {
            *tab_id = tab.id;
        }
    }
    
    // Careful: setting parent tab ID could lead to a cycle in tab IDs resulting later in infinite recursion
    // resulting in sqlite3_step call hanging indefinitely (while allocating memory indefinitely) resulting in
    // CPU spike, and hanging the application when trying to gracefully shutdown.
    VERIFY(check_tab_circular_reference(tab.id, tab.parent_tab_id));

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

// This could be enforced with a database constraint/trigger (see bookmark folders approach)
Error
ProfilesDb::check_tab_circular_reference(TabId tab_id, TabId new_parent_tab_id)
{
    if (tab_id == new_parent_tab_id) {
        RAISE(ErrorCode::circular_reference,
            "tab %lld cannot have itself as parent_tab_id",
            tab_id.value
        );
    }

    // check for cycles in the nested tab hierarchy
    std::string sql = R"(
        WITH RECURSIVE descendants(id) AS (
            SELECT id FROM tabs WHERE parent_tab_id = ?1
            UNION ALL
            SELECT t.id
            FROM tabs t
            JOIN descendants d ON t.parent_tab_id = d.id
        )
        SELECT 1
        FROM descendants
        WHERE id = ?2
        LIMIT 1;
    )";

    std::vector<DbVariant> args = {
        tab_id.value,            // current tab_id
        new_parent_tab_id.value, // new parent_tab_id
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
    bool has_row = stmt.next();
    VERIFY(stmt.error());
    if (has_row) {
        RAISE(ErrorCode::circular_reference,
            "updating tab %lld to have parent_tab_id %lld would create a cycle in tab hierarchy",
            tab_id.value, new_parent_tab_id.value
        );
    }
    SUCCEED();
}

Error
ProfilesDb::update_tab(
    UpdateTabQuery query, TxMode tx_mode, TabsChange* change, WindowsChange* window_change)
{
    if(query.update_seq_num)
    {
        orion::log_debug("update %ld seq %lf", query.id.value, query.seq_num);
    }
    if(query.update_parent_tab_id)
    {
        orion::log_debug("update %ld parent %ld", query.id.value, query.parent_tab_id.value);
    }
    if(query.update_window_id)
    {
        orion::log_debug("update %ld window %ld", query.id.value, query.window_id.value);
    }
    if (query.id == TabId(0)) {
        RAISE(ErrorCode::argument, "query.id is not set");
    }
    if (query.id == TabId(0)) {
        RAISE(ErrorCode::argument, "query.id is not set");
    }

    if (query.update_is_pinned && query.window_id == WindowId(0))
    {
        RAISE(ErrorCode::argument, "query.window_id is not set when attempting to update is pinned");
    }

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(m_db->begin(&tx));
    }

    // check if the tab exists and get its original window_id
    WindowId original_window_id{};
    TabDto tab{};
    {
        std::vector<TabDto> tabs;
        VERIFY(get_tabs({query.id}, &tabs));
        if (tabs.empty()) {
            RAISE(ErrorCode::not_found, "tab with id %lld not found", query.id.value);
        }
        original_window_id = tabs[0].window_id;
        tab = tabs[0];
    }

    bool new_window_is_tab_group = false;

    if(query.update_is_pinned || query.update_window_id)
    {
        {
            std::vector<WindowDto> windows;
            VERIFY(get_windows({query.window_id}, &windows));
            if(windows.empty()) {
                RAISE(ErrorCode::not_found, "window with id %ld not found", query.window_id.value);
            }
            const auto& target_window = windows[0];
            new_window_is_tab_group = target_window.is_tab_group;
            if (query.update_is_pinned && query.is_pinned && target_window.is_private) {
                RAISE(ErrorCode::argument, "cannot pin tab in private window %lld", query.window_id.value);
            }
        }
        if(new_window_is_tab_group)
        {
            //forcefully keep tab in shared pinned tabs window if we're either pinning this tab or we're attempting to move a pinned tab into a different window
            if((query.update_is_pinned && query.is_pinned) || (query.update_window_id && tab.is_pinned))
            {
                ProfileId profile_id{};
                VERIFY(window_id_get_profile_id(query.window_id, &profile_id));
                VERIFY(profile_id_get_shared_pinned_tabs_window_id(profile_id, &query.window_id));
                query.update_window_id = true;
            }
            else if(query.update_is_pinned && !query.is_pinned)
            {
                //if we are unpinning into tab group, we should always update the window id in order to not share tab
                query.update_window_id = true;
            }
        }
    }
    bool source_is_shared_pinned = false;
    bool target_is_shared_pinned = false; 
    {
        ProfileId tmp_profile_id{0};
        
        WindowId target_shared_pinned{0};
        VERIFY(window_id_get_profile_id(query.window_id, &tmp_profile_id));
        VERIFY(profile_id_get_shared_pinned_tabs_window_id(tmp_profile_id, &target_shared_pinned));
        target_is_shared_pinned = query.window_id == target_shared_pinned;
        
        WindowId source_shared_pinned{0};
        VERIFY(window_id_get_profile_id(original_window_id, &tmp_profile_id));
        VERIFY(profile_id_get_shared_pinned_tabs_window_id(tmp_profile_id, &source_shared_pinned));
        source_is_shared_pinned = original_window_id == source_shared_pinned;
    }
    
    WindowId target_window_id = query.update_window_id ? query.window_id : original_window_id;
    double target_seq_num = query.update_seq_num ? query.seq_num : tab.seq_num;

    if (!query.update_seq_num && query.update_is_pinned) {
        // when is_pinned is changed, we must also update seq_num
        
        std::optional<double> last_pinned_seq_num{};
        VERIFY(m_db->query_real_or_null(R"(
            SELECT MAX(seq_num) AS next_seq_num
            FROM tabs
            WHERE window_id = ?1 AND is_pinned;
        )", {target_window_id.value}, &last_pinned_seq_num));

        std::optional<double> first_unpinned_seq_num{};
        VERIFY(m_db->query_real_or_null(R"(
            SELECT MIN(seq_num) AS next_seq_num
            FROM tabs
            WHERE window_id = ?1 AND NOT is_pinned;
        )", {target_window_id.value}, &first_unpinned_seq_num));

        if (query.is_pinned != tab.is_pinned) {
            // changing is_pinned state
            if (last_pinned_seq_num.has_value()) {
                // move the tab to the end of the pinned tabs
                query.seq_num = last_pinned_seq_num.value(); // will be refined below
            } else if (first_unpinned_seq_num.has_value()) {
                // no pinned tabs, move the tab before the first unpinned tab
                query.seq_num = first_unpinned_seq_num.value() - 1.0;
            } else {
                // no other tabs in the window, seq_num does not matter
                query.seq_num = 0.0;
            }
            query.update_seq_num = true;
            
            RefineTabSeqNumQuery refine_query{
                .window_id = query.update_window_id ? query.window_id : original_window_id,
                .seq_num = query.update_seq_num ? query.seq_num : 0.0, // seq_num is ignored if update_seq_num is false
            };
            VERIFY(refine_tab_seq_num(refine_query, &query.seq_num));

            if (query.is_pinned) {
                VERIFY(remove_tab_from_nested_hierarchy(tab.id, {}, change));
            }
        }
    }

    if (query.update_window_id && !query.update_seq_num) {
        std::vector<WindowDto> windows;
        VERIFY(get_windows({query.window_id}, &windows));
        if (windows.empty()) {
            RAISE(ErrorCode::not_found, "window with id %lld not found", query.window_id.value);
        }
        const WindowDto& new_window = windows[0];

        std::vector<TabId> tab_ids;
        VERIFY(get_tab_ids({new_window.id}, &tab_ids));

        std::vector<TabDto> tabs;
        VERIFY(get_tabs(tab_ids, &tabs)); // ordered by seq_num

        if (!tabs.empty()) {
            // append to the end of the new window
            query.update_seq_num = true;
            query.seq_num = tabs.back().seq_num + 1.0;
        }
    }

    if (query.move_children_to_parent) {
        // move all child tabs of this tab to the current parent of this tab (if it exists)
        std::string sql = std::format(R"(
            UPDATE tabs
            SET parent_tab_id = (
                SELECT parent_tab_id FROM tabs WHERE id = ?1
            )
            WHERE parent_tab_id = ?1
            RETURNING {};
        )", Db::join_strings(get_tab_columns(), ", "));

        std::vector<DbVariant> args = {
            query.id.value,
        };

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        // report updated child tabs
        while (stmt.next()) {
            TabDto tab{};
            load_tab_from_stmt(stmt, &tab);
            int64_t tab_index = 0;
            VERIFY(get_tab_index(tab.window_id, tab.seq_num, &tab_index));
            tab.index = tab_index;

            if (change) {
                change->updated.emplace_back(tab);
            }
        }
        VERIFY(stmt.error());
    }

    std::vector<std::string> set_columns;
    std::vector<DbVariant> args;

    if (query.update_is_pinned && query.is_pinned && !query.update_parent_tab_id) {
        // automatically unparent the tab if it is being pinned
        query.parent_tab_id = TabId(0);
        query.update_parent_tab_id = true;
    }

    if (query.update_parent_tab_id && query.parent_tab_id.value != 0) {
        std::vector<TabDto> tabs;
        VERIFY(get_tabs({query.parent_tab_id}, &tabs));
        if (tabs.empty()) {
            RAISE(ErrorCode::not_found, "parent tab with id %lld not found", query.parent_tab_id.value);
        }
        const TabDto& parent_tab = tabs[0];
        if (parent_tab.is_pinned) {
            RAISE(ErrorCode::argument, "cannot set parent_tab_id to a pinned tab (tab id %lld)", parent_tab.id.value);
        }
    }

    if (change) {
        change->updated_window_id = query.update_window_id;
        change->updated_name = query.update_name;
        change->updated_icon_name = query.update_icon_name;
        change->updated_is_pinned = query.update_is_pinned;
        change->updated_is_suspended = query.update_is_suspended;
        change->updated_seq_num = query.update_seq_num;
        change->updated_uri = query.update_uri;
        change->updated_session_state = query.update_session_state;
        change->updated_parent_tab_id = query.update_parent_tab_id;
        change->updated_is_collapsed = query.update_is_collapsed;
        change->updated_back_forward_list_index = query.update_back_forward_list_index;
        change->updated_webkit_back_forward_list_index = query.update_webkit_back_forward_list_index;
        change->updated_merged_back_forward_list_length = query.update_merged_back_forward_list_length;
        change->updated_snapshot_image = query.update_snapshot_image;
    }

    if (query.update_window_id) {
        args.push_back(query.window_id.value);
        set_columns.push_back("window_id");
    }

    if (query.update_name) {
        args.push_back(query.name);
        set_columns.push_back("name");
    }

    if (query.update_icon_name) {
        args.push_back(query.icon_name);
        set_columns.push_back("icon_name");
    }

    if (query.update_is_pinned) {
        args.push_back(query.is_pinned);
        set_columns.push_back("is_pinned");
    }

    if (query.update_is_suspended) {
        args.push_back(query.is_suspended);
        set_columns.push_back("is_suspended");
    }

    if (query.update_seq_num) {
        args.push_back(query.seq_num);
        set_columns.push_back("seq_num");
    }

    if (query.update_uri) {
        args.push_back(query.uri);
        set_columns.push_back("uri");
    }

    if (query.update_session_state) {
        args.push_back(query.session_state);
        set_columns.push_back("session_state");
    }

    if (query.update_parent_tab_id) {
        args.push_back(query.parent_tab_id.value);
        set_columns.push_back("parent_tab_id");
    }

    if (query.update_is_collapsed) {
        args.push_back(query.is_collapsed);
        set_columns.push_back("is_collapsed");
    }

    if (query.update_back_forward_list_index) {
        args.push_back(query.back_forward_list_index);
        set_columns.push_back("back_forward_list_index");
    }

    if (query.update_webkit_back_forward_list_index) {
        args.push_back(query.webkit_back_forward_list_index);
        set_columns.push_back("webkit_back_forward_list_index");
    }

    if (query.update_merged_back_forward_list_length) {
        args.push_back(query.merged_back_forward_list_length);
        set_columns.push_back("merged_back_forward_list_length");
    }

    if (query.update_snapshot_image) {
        args.push_back(query.snapshot_image);
        set_columns.push_back("snapshot_image");
    }

    if (set_columns.empty()) {
        SUCCEED(); // nothing to update
    }

    // Careful: setting parent tab ID could lead to a cycle in tab IDs resulting later in infinite recursion
    // resulting in sqlite3_step call hanging indefinitely (while allocating memory indefinitely) resulting in
    // CPU spike, and hanging the application when trying to gracefully shutdown.
    if (query.update_parent_tab_id) {
        VERIFY(check_tab_circular_reference(query.id, query.parent_tab_id));
    }

    args.push_back(query.id.value);

    std::string sql = std::format(R"(
        UPDATE tabs SET {}
        WHERE id = ?
        RETURNING {};
    )", Db::join_into_set_clause(set_columns),
        Db::join_strings(get_tab_columns(), ", ")
    );

    //Db::log_sql(sql, args, "ProfilesDb::update_tab: ");

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        TabDto tab{};
        load_tab_from_stmt(stmt, &tab);
        int64_t tab_index = 0;
        VERIFY(get_tab_index(tab.window_id, tab.seq_num, &tab_index));
        tab.index = tab_index;

        if (change) {
            change->updated.emplace_back(tab);
        }
    }
    VERIFY(stmt.error());

    if (query.update_window_id && original_window_id != query.window_id) {
        // refresh num_tabs for both the original and the new window
        if(!source_is_shared_pinned)
        {
            VERIFY(refresh_window_num_tabs(original_window_id, window_change));
        }
        if(!target_is_shared_pinned)
        {
            VERIFY(refresh_window_num_tabs(query.window_id, window_change));
        }

        // unselect the tab if it was selected in the original window
        if(!target_is_shared_pinned)
        {
            std::vector<WindowDto> windows;
            VERIFY(get_windows({original_window_id}, &windows));
            if (windows.empty()) {
                RAISE(ErrorCode::not_found, "window with id %lld not found", original_window_id.value);
            }
            const WindowDto& original_window = windows[0];
            if (original_window.selected_tab_id == query.id) {
                UpdateWindowQuery query{
                    .id = original_window_id,
                    .selected_tab_id = TabId(0), // unselect
                    .update_selected_tab_id = true,
                };
                VERIFY(update_window(query, window_change));
            }
        }
    }

    if (query.reorder_descendants && query.update_seq_num) {
        // Reorder all descendant tabs of this tab according to the new seq_num of this tab.
        // This has to be done after the tab itself is updated above so that new seq_num
        // of each descendant can be easily determined.
        std::vector<TabId> descendant_ids;
        VERIFY(get_descendant_tab_ids(query.id, &descendant_ids));
        log_verbose("update_tab: reordering descendants of tab %lld with %zu descendants",
            query.id.value, descendant_ids.size()
        );

        double last_seq_num = target_seq_num;
        for (TabId descendant_id : descendant_ids) {
            RefineTabSeqNumQuery refine_query{
                .window_id = target_window_id,
                .seq_num = last_seq_num, // tries to insert after the last_seq_num on collision
            };
            double seq_num{};
            VERIFY(refine_tab_seq_num(refine_query, &seq_num));

            UpdateTabQuery update_query{
                .id = descendant_id,
                .window_id = target_window_id,
                .seq_num = seq_num,
                .update_window_id = true,
                .update_seq_num = true,
            };

            // Recursive call to update_tab for each descendant
            VERIFY(update_tab(update_query, TxMode::no_tx, change, window_change));
            last_seq_num = seq_num;
        }
        log_verbose("update_tab: reordered descendants of tab %lld", query.id.value);
    }

    // if(new_window_is_tab_group)
    // {
    //     refresh_window_num_tabs(query.window_id, window_change);
    // }

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

std::vector<TabId>
ProfilesDb::to_unique_tab_ids(const std::vector<TabId>& tab_ids)
{
    std::unordered_set<int64_t> seen;
    std::vector<TabId> unique_ids;

    unique_ids.reserve(tab_ids.size());
    for (const TabId& id : tab_ids) {
        if (seen.insert(id.value).second) { // true if not already present
            unique_ids.push_back(id);
        }
    }
    return unique_ids;
}

Error
ProfilesDb::find_invalid_tab_ids(const std::vector<TabId>& tab_ids, std::vector<TabId>* invalid_tab_ids)
{
    if (tab_ids.empty()) {
        SUCCEED(); // nothing to check
    }
    RAISE_CHECK(invalid_tab_ids);

    std::vector<TabDto> tabs;
    VERIFY(get_tabs(to_unique_tab_ids(tab_ids), &tabs));
    if (tabs.size() == tab_ids.size()) {
        SUCCEED(); // all tabs found
    }

    for (const auto& id : tab_ids) {
        auto it = std::find_if(tabs.begin(), tabs.end(), [id](const TabDto& tab) {
            return tab.id == id;
        });
        if (it == tabs.end()) {
            invalid_tab_ids->push_back(id);
        } 
    }
    SUCCEED();
}

Error
ProfilesDb::remove_tab_from_nested_hierarchy(
    TabId tab_id, const std::vector<TabId>& ignore_changes_ids, TabsChange* change)
{
    if (tab_id == TabId(0)) {
        RAISE(ErrorCode::argument, "tab_id is not set");
    }
    TabDto tab{};
    {
        std::vector<TabDto> tabs;
        VERIFY(get_tabs({tab_id}, &tabs));
        if (tabs.empty()) {
            RAISE(ErrorCode::not_found, "tab with id %lld not found", tab_id.value);
        }
        tab = tabs[0];
    }

    std::string sql = std::format(R"(
        UPDATE tabs
        SET parent_tab_id = ?2
        WHERE parent_tab_id = ?1
        RETURNING {};
    )", Db::join_strings(get_tab_columns(), ", "));

    std::vector<DbVariant> args = {
        tab_id.value,
        tab.parent_tab_id.value,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    // report updated child tabs
    while (stmt.next()) {
        TabDto tab{};
        load_tab_from_stmt(stmt, &tab);
        int64_t tab_index = 0;
        VERIFY(get_tab_index(tab.window_id, tab.seq_num, &tab_index));
        tab.index = tab_index;

        if (change) {
            if (std::find(ignore_changes_ids.begin(), ignore_changes_ids.end(), tab.id) == ignore_changes_ids.end()) {
                change->updated.push_back(tab);
            }
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::delete_tabs(
    const std::vector<TabId>& ids, TxMode tx_mode, TabsChange* change, WindowsChange* window_change)
{
    // check if each tab exists and gather their window_ids
    std::set<WindowId> window_ids;
    {
        std::vector<TabDto> tabs;
        VERIFY(get_tabs(ids, &tabs));
        if (tabs.size() != ids.size()) {
            std::vector<TabId> not_found_ids;
            for (const auto& id : ids) {
                auto it = std::find_if(tabs.begin(), tabs.end(), [id](const TabDto& tab) {
                    return tab.id == id;
                });
                if (it == tabs.end()) {
                    not_found_ids.push_back(id);
                } 
            }
            log_warning("delete_tabs: one or more tabs not found: %s", to_string(not_found_ids).c_str());
        }

        for (const auto& tab : tabs) {
            window_ids.insert(tab.window_id);
        }
    }

    // remove each tab from its potential nested hierarchy, ie., reparent its children to its parent
    for (const TabId& tab_id : ids) {
        VERIFY(remove_tab_from_nested_hierarchy(tab_id, ids, change));
    }

    auto args = to_db_variant_vector(ids);
    std::string sql = std::format(R"(
        DELETE FROM tabs WHERE id IN ({})
        RETURNING id;
    )", Db::make_placeholders(ids.size()));

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(m_db->begin(&tx));
    }

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto id = TabId(stmt.integer("id"));
        if (change) {
            change->deleted.push_back(id);
        }
    }
    VERIFY(stmt.error());

    for (const auto& window_id : window_ids) {
        VERIFY(refresh_window_num_tabs(window_id, window_change));
    }

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::duplicate_tab(DuplicateTabQuery query, TabDto* new_tab, TabsChange* change, WindowsChange* window_change)
{
    if (query.id == TabId(0)) {
        RAISE(ErrorCode::argument, "tab id is not set");
    }

    // Find the source tab
    TabDto source_tab{};
    {
        std::vector<TabDto> tabs;
        VERIFY(get_tabs({query.id}, &tabs));

        if (tabs.empty()) {
            RAISE(ErrorCode::not_found, "tab with id %lld not found", query.id.value);
        }
        source_tab = tabs[0];
    }

    DbTx tx;
    VERIFY(m_db->begin(&tx));

    // Insert new tab into the specified window directly after the source tab
    {
        CreateTabQuery create_query{
            .window_id = source_tab.window_id,
            .name = source_tab.name,
            .icon_name = source_tab.icon_name,
            .is_pinned = source_tab.is_pinned,
            .is_suspended = source_tab.is_suspended,
            .seq_num = source_tab.seq_num, // will be refined in create_tab()
            .uri = source_tab.uri,
            // do not copy session state
            .parent_tab_id = source_tab.parent_tab_id,
            .is_collapsed = source_tab.is_collapsed,
            .back_forward_list_index = 0,         // new tab starts with no history
            .webkit_back_forward_list_index = 0,  // new tab starts with no history
            .merged_back_forward_list_length = 0, // new tab starts with no history
            // do not copy snapshot image
        };

        TabId new_tab_id = TabId(0);
        VERIFY(create_tab(create_query, &new_tab_id, TxMode::no_tx, change, window_change)); // this will also refresh num_tabs in the window

        if (change->created.size() != 1) {
            RAISE(ErrorCode::assert, "expected exactly one created tab, got %zu", change->created.size());
        }
        *new_tab = change->created.back();
    }

    VERIFY(tx.commit());
    SUCCEED();
}

Error
ProfilesDb::duplicate_tab_with_new_window_id(
    DuplicateTabWithNewWindowIdQuery query, TabDto* new_tab, TabsChange* change, WindowsChange* window_change,
    TabBackForwardListEntriesChange* tab_back_forward_list_entries_change, TxMode tx_mode)
{
    if (query.id == TabId(0)) {
        RAISE(ErrorCode::argument, "tab id is not set");
    }
    if (query.window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "window id is not set");
    }

    // Find the source tab
    TabDto source_tab{};
    {
        std::vector<TabDto> tabs;
        VERIFY(get_tabs({query.id}, &tabs));

        if (tabs.empty()) {
            RAISE(ErrorCode::not_found, "tab with id %lld not found", query.id.value);
        }
        source_tab = tabs[0];
    }

    DbTx tx;
    if(tx_mode == TxMode::use_tx)
    {
        VERIFY(m_db->begin(&tx));
    }

    // Insert new tab into the specified window directly after the source tab
    TabId new_tab_id = TabId(0);
    {
        CreateTabQuery create_query{
            .window_id = query.window_id,
            .name = source_tab.name,
            .icon_name = source_tab.icon_name,
            .is_pinned = source_tab.is_pinned,
            .is_suspended = source_tab.is_suspended,
            .seq_num = source_tab.seq_num, // will be refined in create_tab()
            .uri = source_tab.uri,
            .session_state = source_tab.session_state,
            .parent_tab_id = source_tab.parent_tab_id,
            .is_collapsed = source_tab.is_collapsed,
            .back_forward_list_index = source_tab.back_forward_list_index,
            .webkit_back_forward_list_index = source_tab.webkit_back_forward_list_index,
            .merged_back_forward_list_length = source_tab.merged_back_forward_list_length,
            // do not copy snapshot image
        };
        size_t prev_created_count = change->created.size();
        VERIFY(create_tab(create_query, &new_tab_id, TxMode::no_tx, change, window_change)); // this will also refresh num_tabs in the window
        size_t new_created_count = change->created.size();
        if (new_created_count-prev_created_count != 1) {
            RAISE(ErrorCode::assert, "expected exactly one created tab, got %zu", new_created_count-prev_created_count);
        }
        *new_tab = change->created.back();
    }
    {
        std::vector<TabBackForwardListEntryDto> entries{};
        VERIFY(get_tab_back_forward_list_entries(query.id, &entries));
        for(auto& entry : entries)
        {
            CreateTabBackForwardListEntryQuery back_forward_entry_query{
                .tab_id = new_tab_id,
                .webkit_index = entry.webkit_index,
                .uri = entry.uri,
            };
            TabBackForwardListEntryDto new_entry{};
            VERIFY(create_tab_back_forward_list_entry(back_forward_entry_query, &new_entry, tab_back_forward_list_entries_change));
        }
    }
    if(tx_mode == TxMode::use_tx)
    {
        VERIFY(tx.commit());
    }
    SUCCEED();
}

Error
ProfilesDb::suspend_tabs(const std::vector<TabId>& ids, TabsChange* change)
{
    std::vector<TabId> invalid_ids;
    VERIFY(find_invalid_tab_ids(ids, &invalid_ids));
    if (!invalid_ids.empty()) {
        log_warning("suspend_tabs: one or more tabs not found: %s", to_string(invalid_ids).c_str());
    }

    if (change) {
        change->updated_is_suspended = true;
    }
    
    // Start Page cannot be suspended
    std::string sql = std::format(R"(
        UPDATE tabs
        SET is_suspended = 1
        WHERE id IN ({}) AND uri != '' AND uri != 'orion://newtab/'
        RETURNING {};
    )", Db::make_placeholders(ids.size()),
        Db::join_strings(get_tab_columns(), ", ")
    );

    auto args = to_db_variant_vector(ids);

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        TabDto tab{};
        load_tab_from_stmt(stmt, &tab);
        int64_t tab_index = 0;
        VERIFY(get_tab_index(tab.window_id, tab.seq_num, &tab_index));
        tab.index = tab_index;

        if (change) {
            change->updated.emplace_back(tab);
        }
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::can_move_tab(CanMoveTabQuery query, bool* can_move)
{
    if (query.tab_id == TabId(0)) {
        RAISE(ErrorCode::argument, "tab_id is not set");
    }
    if (query.target_window_id == WindowId(0)) {
        RAISE(ErrorCode::argument, "target_window_id is not set");
    }
    RAISE_CHECK(can_move);
    std::vector<WindowDto> windows;
    VERIFY(get_windows({query.target_window_id}, &windows));
    if (windows.empty()) {
        RAISE(ErrorCode::not_found, "window with id %lld not found", query.target_window_id.value);
    }
    const WindowDto& target_window = windows[0];

    // std::vector<TabDto> original_tabs;
    // {
    //     VERIFY(get_tabs({query.tab_id}, &original_tabs));
    //     if(!original_tabs.size())
    //     {
    //         RAISE(ErrorCode::not_found, "can_move_tab(): tab not found for id %" PRId64, query.tab_id.value);
    //     }
    // }
    // orion::TabDto& original_tab = original_tabs[0];
    // (void) original_tab;
    
    std::vector<orion::TabId> tab_ids;
    WindowId shared_pinned_tabs_window_id(0);
    if(target_window.is_tab_group)
    {
        orion::ProfileId profile_id{};
        VERIFY(window_id_get_profile_id(query.target_window_id, &profile_id));
        VERIFY(profile_id_get_shared_pinned_tabs_window_id(profile_id, &shared_pinned_tabs_window_id));
        GetTabIdsQuery tab_ids_query{
            .window_id = shared_pinned_tabs_window_id
        };
        VERIFY(get_tab_ids(tab_ids_query, &tab_ids));
    }

    GetTabIdsQuery tab_ids_query{
        .window_id = target_window.id,
    };
    VERIFY(get_tab_ids(tab_ids_query, &tab_ids)); // ordered by seq_num
    std::vector<TabDto> tabs;
    VERIFY(get_tabs(tab_ids, &tabs)); // ordered by seq_num

    if (query.target_position == 0 && tabs.empty()) {
        // moving to an empty window, always allowed
        *can_move = true;
        SUCCEED();
    }
    //TODO: maybe add logic to check pinned state?

    if (query.target_position < 0 || query.target_position > static_cast<int64_t>(tabs.size())) {
        RAISE(ErrorCode::argument, "target_position %lld is out of bounds", query.target_position);
    }

    if (query.group_with_existing) {
        const TabDto& new_parent_tab = tabs[query.target_position];
        Error err = check_tab_circular_reference(query.tab_id, new_parent_tab.id);
        if (err.code() == ErrorCode::circular_reference) {
            *can_move = false;
            SUCCEED();
        }
        VERIFY(err);
        *can_move = true;
    } else {
        if (query.target_position + 1 < static_cast<int64_t>(tabs.size())) {
            // next tab is present
            const TabDto& next_tab = tabs[query.target_position + 1];
            // next tab determines the new parent of the moving tab
            Error err = check_tab_circular_reference(query.tab_id, next_tab.parent_tab_id);
            if (err.code() == ErrorCode::circular_reference) {
                *can_move = false;
                SUCCEED();
            }
            VERIFY(err);
        } else {
            // no next tab, no parent to worry about, always allowed
        }
        *can_move = true;
    }
    SUCCEED();
}

Error
ProfilesDb::move_tab(
    MoveTabQuery query, TxMode tx_mode, TabsChange* tab_change, WindowsChange* window_change,
    ProfilesChange* profile_change, RecentlyClosedTabsChange* recently_closed_tabs_change)
{
    orion::log_verbose("move %ld parent %ld after %ld window %ld", query.tab_id.value, query.parent_tab_id.value, query.after_tab_id.value, query.target_window_id.value);
    
    TabsChange tabs_change_local{};
    WindowsChange windows_change_local{};
    ProfilesChange profiles_change_local{};
    RecentlyClosedTabsChange recently_closed_tabs_change_local{};

    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    if (query.tab_id == TabId(0)) {
        RAISE(ErrorCode::argument, "tab_id is not set");
    }
    if(query.parent_tab_id != TabId(0) && !(query.is_nested_tabs_operation))
    {
        RAISE(ErrorCode::argument, "parent tab id should only be set on nested operation");
    }

    WindowId source_window_id(0);
    {
        std::string sql = R"(
            SELECT window_id
            FROM tabs
            WHERE id = ?1;
        )";

        int64_t val = 0;
        std::vector<DbVariant> args = {query.tab_id.value};
        VERIFY(m_db->query_integer(sql, args, &val));
        source_window_id = WindowId(val);
    }
    bool is_pinned = false;
    {
        std::string sql = R"(
            SELECT is_pinned
            FROM tabs
            WHERE id = ?1;
        )";

        std::vector<DbVariant> args = {query.tab_id.value};
        VERIFY(m_db->query_bool(sql, args, &is_pinned));
    }

    if(is_pinned)
    {
        std::string sql = R"(
            SELECT is_tab_group
            FROM windows
            WHERE id = ?1;
        )";

        bool is_tab_group = false;
        std::vector<DbVariant> args = {query.target_window_id.value};
        VERIFY(m_db->query_bool(sql, args, &is_tab_group));
        if(is_tab_group)
        {
            orion::ProfileId profile_id{};
            VERIFY(window_id_get_profile_id(query.target_window_id, &profile_id));
            VERIFY(profile_id_get_shared_pinned_tabs_window_id(profile_id, &query.target_window_id));

            if(query.after_tab_id != TabId(0))
            {
                bool tab_after_is_pinned = false;
                std::string sql = R"(
                    SELECT is_pinned
                    FROM tabs
                    WHERE id = ?1;
                )";
                std::vector<DbVariant> args = {query.after_tab_id.value};
                VERIFY(m_db->query_bool(sql, args, &tab_after_is_pinned));

                if(!tab_after_is_pinned)
                {
                    query.after_tab_id = TabId(0);
                }
            }
        }
        else
        {
            //checks whether tab before us is pinned, if not, something has gone wrong
            sql = R"(
                SELECT CASE 
                    WHEN ?1 = 0 THEN COALESCE (
                        (SELECT is_pinned FROM tabs WHERE id != ?1 AND window_id = ?2 ORDER BY seq_num DESC LIMIT 1), 0
                    )
                    ELSE COALESCE (
                        (SELECT is_pinned FROM tabs WHERE seq_num < (SELECT seq_num FROM tabs WHERE id = ?3) AND id != ?1 AND window_id = ?2 ORDER BY seq_num DESC LIMIT 1), FALSE
                    )
                END;
            )";
            args = {query.tab_id.value, query.target_window_id.value, query.after_tab_id.value};

            bool valid = false;
            VERIFY(m_db->query_bool(sql, args, &valid));
            if(!valid)
            {
                RAISE(ErrorCode::invalid_state, "could not move tab %li to before %li with target parent id %li, tab before is unpinned, cannot insert pinned",
                    query.tab_id.value, query.after_tab_id.value, query.parent_tab_id.value);
            }
        }
    }
    else
    {
        if(query.after_tab_id != TabId(0))
        {
            std::string sql = R"(
                SELECT NOT is_pinned
                FROM tabs
                WHERE id = ?1;
            )";

            DbRow args = {query.after_tab_id.value};
            bool valid = false;
            VERIFY(m_db->query_bool(sql, args, &valid));
            if(!valid)
            {
                RAISE(ErrorCode::invalid_state, "could not move tab %li to before %li with target parent id %li, tab after is pinned, cannot insert unpinned",
                    query.tab_id.value, query.after_tab_id.value, query.parent_tab_id.value);
            }
        }
    }

    std::vector<TabId> invalidated_parent_ids{};
    {
        std::vector<DbVariant> args{
            query.after_tab_id.value,
            query.parent_tab_id.value,
            query.tab_id.value,
            query.target_window_id.value};

        //NOTE: this large commented part is just debug information for debugging the sqlite logic
#if defined(ENABLE_VERBOSE_LOGS) && ENABLE_VERBOSE_LOGS
        {
            std::string sql = R"(
                SELECT COALESCE (
                    (
                        SELECT parent_tab_id
                        FROM tabs
                        WHERE 
                            seq_num >= (SELECT seq_num FROM tabs WHERE id = ?1) 
                            AND id != ?3
                            AND window_id = ?4
                        ORDER BY seq_num ASC LIMIT 1
                    ), 0
                );
            )";

            int64_t val = 0;
            VERIFY(m_db->query_integer(sql, args, &val));
            orion::log_verbose("after tab's parent_tab_id: %li", val);
        }
        {
            std::string sql = R"(
                SELECT COALESCE (
                    (
                        SELECT id
                        FROM tabs 
                        WHERE
                            seq_num < (SELECT seq_num FROM tabs WHERE id = ?1)
                            AND id != ?3
                            AND window_id = ?4
                        ORDER BY seq_num DESC LIMIT 1
                    ), 0
                );
            )";

            int64_t val = 0;
            VERIFY(m_db->query_integer(sql, args, &val));
            orion::log_verbose("before tab's id: %li", val);
        }
        {
            std::string sql = R"(
                WITH RECURSIVE
                floor AS (
                    SELECT COALESCE (
                        (
                            SELECT parent_tab_id
                            FROM tabs
                            WHERE
                                seq_num >= (SELECT seq_num FROM tabs WHERE id = ?1) 
                                AND id != ?3
                                AND window_id = ?4
                            ORDER BY seq_num ASC LIMIT 1
                        ), 0
                    ) AS id
                ),
                ceiling AS (
                    SELECT CASE 
                        WHEN ?1 = 0 THEN COALESCE (
                            (
                                SELECT id 
                                FROM tabs
                                WHERE
                                    id != ?3
                                    AND window_id = ?4
                                ORDER BY seq_num DESC LIMIT 1
                            ), 0
                        )
                        ELSE COALESCE (
                            (SELECT id FROM tabs WHERE seq_num < (SELECT seq_num FROM tabs WHERE id = ?1) AND id != ?3 AND window_id = ?4 ORDER BY seq_num DESC LIMIT 1), 0
                        )
                    END AS id
                ),
                valid_ids AS (
                    SELECT id FROM ceiling
                    
                    UNION ALL
                    
                    SELECT t.parent_tab_id
                    FROM tabs t
                    JOIN valid_ids v ON t.id = v.id
                    WHERE v.id != (SELECT id FROM floor)
                        AND v.id != 0
                        AND t.parent_tab_id IS NOT NULL
                )
                SELECT id FROM valid_ids;
            )";

            DbStmt stmt{};
            VERIFY(m_db->query(sql, args, &stmt));
            log_verbose("valid parent ids:");
            while(stmt.next())
            {
                log_verbose("%li", stmt.integer("id"));
            }
        }
#endif

        //validates that we do not break the parent ids of further tabs
        //may potentially colide with itself, need to ignore
        //basically, we need to ignore the moving tab in the structure, we have already checked against circular references
        //next, we create a list of possible parent ids, those being ids going from the tab preceding us
        //down to its common ancestor with the tab succeeding us
        //ceiling includes extra logic to grab the id of the last tab in the window if after_tab_id is 0
        std::string sql = R"(
            WITH RECURSIVE
            floor AS (
                SELECT COALESCE (
                    (
                        SELECT parent_tab_id
                        FROM tabs
                        WHERE
                            seq_num >= (SELECT seq_num FROM tabs WHERE id = ?1)
                            AND id != ?3
                            AND window_id = ?4
                        ORDER BY seq_num ASC LIMIT 1
                    ), 0
                ) AS id
            ),
            ceiling AS (
                SELECT CASE 
                    WHEN ?1 = 0 THEN COALESCE (
                        (
                            SELECT id
                            FROM tabs
                            WHERE
                                id != ?3
                                AND window_id = ?4
                            ORDER BY seq_num DESC LIMIT 1
                        ), 0
                    )
                    ELSE COALESCE (
                        (
                            SELECT id
                            FROM tabs
                            WHERE
                                seq_num < (SELECT seq_num FROM tabs WHERE id = ?1)
                                AND id != ?3
                                AND window_id = ?4
                            ORDER BY seq_num DESC LIMIT 1
                        ), 0
                    )
                END AS id
            ),
            ancestor_chain AS (
                SELECT id, 1 as depth FROM ceiling
                
                UNION ALL
                
                SELECT t.parent_tab_id, a.depth + 1
                FROM tabs t
                JOIN ancestor_chain a ON t.id = a.id
                WHERE
                    a.id != 0
                    AND t.parent_tab_id IS NOT NULL
            )
            SELECT id FROM ancestor_chain
            WHERE
                depth < (SELECT depth FROM ancestor_chain WHERE id = ?2)
                AND depth >= (SELECT depth FROM ancestor_chain WHERE id = (SELECT id FROM floor));
        )";

        DbStmt stmt{};
        VERIFY(m_db->query(sql, args, &stmt));
        orion::log_verbose("invalidated parent ids:");
        while(stmt.next())
        {
            invalidated_parent_ids.push_back(TabId(stmt.integer("id")));
            orion::log_verbose("%li", invalidated_parent_ids.back().value);
        }
    }

    if(query.parent_tab_id.value) 
    {
        VERIFY(check_tab_circular_reference(query.tab_id, query.parent_tab_id));
    }
    
    if(query.is_nested_tabs_operation)
    {
        if(invalidated_parent_ids.size())
        {
            RAISE(ErrorCode::invalid_state, "failed to move tab with id %ld for new parent id %ld to before tab %ld, would break the tree structure for later tabs",
                query.tab_id.value, query.parent_tab_id.value, query.after_tab_id.value);
        }
        
#if defined(EXTRA_VALIDATION) && EXTRA_VALIDATION
        if(query.after_tab_id.value)
        {
            std::string sql = R"(
                SELECT EXISTS
                (
                    SELECT *
                    FROM tabs
                    WHERE id = ?1 AND window_id = ?2
                );
            )";

            std::vector<DbVariant> args = {query.after_tab_id.value, query.target_window_id.value};
            bool valid = false;
            VERIFY(m_db->query_bool(sql, args, &valid));
            if(!valid)
            {
                RAISE(ErrorCode::invalid_state, "tab %ld provided as tab after is not in the target window %ld",
                    query.after_tab_id.value, query.target_window_id.value);
            }
        }
        if(query.parent_tab_id.value)
        {
            std::string sql = R"(
                SELECT EXISTS
                (
                    SELECT *
                    FROM tabs
                    WHERE id = ?1 AND window_id = ?2
                );
            )";

            std::vector<DbVariant> args = {query.parent_tab_id.value, query.target_window_id.value};
            bool valid = false;
            VERIFY(m_db->query_bool(sql, args, &valid));
            if(!valid)
            {
                RAISE(ErrorCode::invalid_state, "tab %ld provided as new parent tab is not in the target window %ld",
                    query.parent_tab_id.value, query.target_window_id.value);
            }
        }
#endif
    }

    std::vector<TabId> descendant_ids{};
    VERIFY(get_descendant_tab_ids(query.tab_id, &descendant_ids));


    double prev_seq_num = 0, next_seq_num = 0;
    if(query.after_tab_id == TabId(0))
    {
        std::string sql = R"(
            SELECT MAX(seq_num) as prev
            FROM tabs
            WHERE window_id = ?1;
        )";

        std::vector<DbVariant> args = {query.target_window_id.value};
        
        DbStmt stmt{};
        VERIFY(m_db->query(sql, args, &stmt));
        int rc = SQLITE_OK;
        if(stmt.next(&rc))
        {
            auto prev = stmt.real_optional("prev");
            prev_seq_num = prev.value_or(0);
            next_seq_num = prev_seq_num+8; //arbitrary constant to give us extra space
        }
        else
        {
            RAISE(ErrorCode::db_step, "failed to execute database step, sqlite error code: %d", rc);
        }
    }
    else
    {
        std::string sql = R"(
            SELECT 
            i1.seq_num as next,
            (
                SELECT MAX(i2.seq_num) 
                FROM tabs i2 
                WHERE i2.window_id = i1.window_id 
                AND i2.seq_num < i1.seq_num
            ) as prev
            FROM tabs i1
            WHERE i1.id = ?1;
        )";

        std::vector<DbVariant> args = {query.after_tab_id.value};
        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        if(stmt.next())
        {
            next_seq_num = stmt.real("next");

            auto prev_optional = stmt.real_optional("prev");
            
            prev_seq_num = prev_optional.value_or(next_seq_num - 8);
        }
        else
        {
            RAISE(ErrorCode::argument, "failed to find seq nums for inserting before tab %ld", query.after_tab_id.value);
        }
    }

    

    if(!query.is_nested_tabs_operation)
    {
        //drops the children one level lower to avoid completely breaking the structure
        std::string sql = std::format(R"(
            UPDATE tabs
            SET parent_tab_id = (SELECT parent_tab_id FROM tabs WHERE id = ?1)
            WHERE parent_tab_id = ?1
            RETURNING {}
        )", Db::join_strings(get_tab_columns(), " ,"));

        std::vector<DbVariant> args = {query.tab_id.value};
        DbStmt stmt{};
        VERIFY(m_db->query(sql, args, &stmt));

        
        if(tx_mode == TxMode::no_tx)
        {
            while(stmt.next())
            {
                tab_change->updated.emplace_back();
                load_tab_from_stmt(stmt, &tab_change->updated.back());
                tab_change->updated_parent_tab_id = true;
            }
        }
        else
        {
            while(stmt.next())
            {
                tabs_change_local.updated.emplace_back();
                load_tab_from_stmt(stmt, &tabs_change_local.updated.back());
                tabs_change_local.updated_parent_tab_id = true;
            }
        }


        
        //drops all tabs with to be invalidated parent ids to root
        if(invalidated_parent_ids.size())
        {
            std::string sql = std::format(R"(
                UPDATE tabs
                SET parent_tab_id = 0
                WHERE
                    parent_tab_id IN json_each(?2)
                    AND seq_num >= (SELECT seq_num FROM tabs WHERE id = ?1)
                RETURNING {};
            )", Db::join_strings(get_tab_columns(), " ,"));
            
            std::ostringstream json_array_builder;
            json_array_builder << "[" << query.tab_id.value;
            for(auto& id : invalidated_parent_ids)
            {
                json_array_builder << "," << id.value;
            }
            json_array_builder << "]";
            std::string json_array = json_array_builder.str();


            std::vector<DbVariant> args = {query.after_tab_id.value, json_array};
            DbStmt stmt{};
            VERIFY(m_db->query(sql, &stmt));

            if(tx_mode == TxMode::no_tx)
            {
                while(stmt.next())
                {
                    tab_change->updated.emplace_back();
                    load_tab_from_stmt(stmt, &tab_change->updated.back());
                    tab_change->updated_parent_tab_id = true;
                }
            }
            else
            {
                while(stmt.next())
                {
                    tabs_change_local.updated.emplace_back();
                    load_tab_from_stmt(stmt, &tabs_change_local.updated.back());
                    tabs_change_local.updated_parent_tab_id = true;
                }
            }
        }
    }

    {
        int n_tabs = query.is_nested_tabs_operation?descendant_ids.size()+1:1;
        double seq_num_step = (next_seq_num-prev_seq_num)/(n_tabs+1);

        //single statement to atomically change the parent tab id together with seq num to give atomic updates to frontend, likely deeply unnecessary
        std::string sql = std::format(R"(
            WITH target_ids AS (
                SELECT ?2 AS id
                UNION
                SELECT value AS id FROM json_each(?5)
            ),
            seq_data AS (
                SELECT 
                    value AS id, 
                    ?3 + (key * ?4) AS new_seq
                FROM json_each(?5)
            )
            UPDATE tabs
            SET 
                parent_tab_id = CASE 
                    WHEN tabs.id = ?2 THEN ?1 
                    ELSE tabs.parent_tab_id 
                END,
                seq_num = COALESCE(seq_data.new_seq, tabs.seq_num),
                window_id = ?6
            FROM target_ids
            LEFT JOIN seq_data ON target_ids.id = seq_data.id
            WHERE tabs.id = target_ids.id
            RETURNING {};
        )", Db::join_strings(get_tab_columns(), " ,"));

        
        std::ostringstream json_array_builder;
        json_array_builder << "[" << query.tab_id.value;
        if(query.is_nested_tabs_operation)
        {
            for(auto& id : descendant_ids)
            {
                json_array_builder << "," << id.value;
            }
        }
        json_array_builder << "]";
        std::string json_array = json_array_builder.str();

        double starting_seq_num = prev_seq_num + seq_num_step;

        DbStmt stmt{};
        std::vector<DbVariant> args = {query.parent_tab_id.value, query.tab_id.value, starting_seq_num, seq_num_step, json_array, query.target_window_id.value};
        VERIFY(m_db->query(sql, args, &stmt));

        if(tx_mode == TxMode::no_tx)
        {
            while(stmt.next())
            {
                tab_change->updated.emplace_back();
                load_tab_from_stmt(stmt, &tab_change->updated.back());
                tab_change->updated_parent_tab_id = true;
                tab_change->updated_seq_num = true;
            }
        }
        else
        {
            while(stmt.next())
            {
                tabs_change_local.updated.emplace_back();
                load_tab_from_stmt(stmt, &tabs_change_local.updated.back());
                tabs_change_local.updated_parent_tab_id = true;
                tabs_change_local.updated_seq_num = true;
            }
        }
    }

    
    VERIFY(refresh_window_num_tabs(source_window_id, &windows_change_local));
    VERIFY(refresh_window_num_tabs(query.target_window_id, &windows_change_local));


    if(tx_mode == TxMode::use_tx)
    {
        VERIFY(ensure_window_state_after_tab_move(source_window_id, TxMode::no_tx, &windows_change_local, &tabs_change_local, &profiles_change_local, &recently_closed_tabs_change_local));
        VERIFY(tx.commit());
        //most efficient way to splice vectors afaik
        tab_change->created.insert(tab_change->created.end(),
            std::make_move_iterator(tabs_change_local.created.begin()), std::make_move_iterator(tabs_change_local.created.end()));
        tab_change->deleted.insert(tab_change->deleted.end(),
            std::make_move_iterator(tabs_change_local.deleted.begin()), std::make_move_iterator(tabs_change_local.deleted.end()));
        tab_change->updated.insert(tab_change->updated.end(),
            std::make_move_iterator(tabs_change_local.updated.begin()), std::make_move_iterator(tabs_change_local.updated.end()));
        tab_change->updated_window_id = tabs_change_local.updated_window_id || tab_change->updated_window_id;
        tab_change->updated_name = tabs_change_local.updated_name || tab_change->updated_name;
        tab_change->updated_icon_name = tabs_change_local.updated_icon_name || tab_change->updated_icon_name;
        tab_change->updated_is_pinned = tabs_change_local.updated_is_pinned || tab_change->updated_is_pinned;
        tab_change->updated_is_suspended = tabs_change_local.updated_is_suspended || tab_change->updated_is_suspended;
        tab_change->updated_seq_num = tabs_change_local.updated_seq_num || tab_change->updated_seq_num;
        tab_change->updated_uri = tabs_change_local.updated_uri || tab_change->updated_uri;
        tab_change->updated_session_state = tabs_change_local.updated_session_state || tab_change->updated_session_state;
        tab_change->updated_parent_tab_id = tabs_change_local.updated_parent_tab_id || tab_change->updated_parent_tab_id;
        tab_change->updated_is_collapsed = tabs_change_local.updated_is_collapsed || tab_change->updated_is_collapsed;
        tab_change->updated_back_forward_list_index = tabs_change_local.updated_back_forward_list_index || tab_change->updated_back_forward_list_index;
        tab_change->updated_webkit_back_forward_list_index = tabs_change_local.updated_webkit_back_forward_list_index || tab_change->updated_webkit_back_forward_list_index;
        tab_change->updated_merged_back_forward_list_length = tabs_change_local.updated_merged_back_forward_list_length || tab_change->updated_merged_back_forward_list_length;
        tab_change->updated_snapshot_image = tabs_change_local.updated_snapshot_image || tab_change->updated_snapshot_image;
    }
    else
    {
        VERIFY(ensure_window_state_after_tab_move(source_window_id, TxMode::use_tx, &windows_change_local, tab_change, &profiles_change_local, &recently_closed_tabs_change_local));
    }
    
    {
        window_change->created.insert(window_change->created.end(),
            std::make_move_iterator(windows_change_local.created.begin()), std::make_move_iterator(windows_change_local.created.end()));
        window_change->deleted.insert(window_change->deleted.end(),
            std::make_move_iterator(windows_change_local.deleted.begin()), std::make_move_iterator(windows_change_local.deleted.end()));
        window_change->updated.insert(window_change->updated.end(),
            std::make_move_iterator(windows_change_local.updated.begin()), std::make_move_iterator(windows_change_local.updated.end()));
        window_change->updated_name = windows_change_local.updated_name || window_change->updated_name;
        window_change->updated_icon_name = windows_change_local.updated_icon_name || window_change->updated_icon_name;
        window_change->updated_keyboard_shortcut = windows_change_local.updated_keyboard_shortcut || window_change->updated_keyboard_shortcut;
        window_change->updated_ui_window_id = windows_change_local.updated_ui_window_id || window_change->updated_ui_window_id;
        window_change->updated_selected_tab_id = windows_change_local.updated_selected_tab_id || window_change->updated_selected_tab_id;
        window_change->updated_is_tab_group = windows_change_local.updated_is_tab_group || window_change->updated_is_tab_group;
        window_change->updated_is_suspended = windows_change_local.updated_is_suspended || window_change->updated_is_suspended;
        window_change->updated_seq_num = windows_change_local.updated_seq_num || window_change->updated_seq_num;
        window_change->updated_has_ui_window = windows_change_local.updated_has_ui_window || window_change->updated_has_ui_window;
        window_change->updated_undo_redo_tab_step_seq_num = windows_change_local.updated_undo_redo_tab_step_seq_num || window_change->updated_undo_redo_tab_step_seq_num;
        window_change->updated_is_focus_mode = windows_change_local.updated_is_focus_mode || window_change->updated_is_focus_mode;
    }
    {
        profile_change->created.insert(profile_change->created.end(),
            std::make_move_iterator(profiles_change_local.created.begin()), std::make_move_iterator(profiles_change_local.created.end()));
        profile_change->deleted.insert(profile_change->deleted.end(),
            std::make_move_iterator(profiles_change_local.deleted.begin()), std::make_move_iterator(profiles_change_local.deleted.end()));
        profile_change->updated.insert(profile_change->updated.end(),
            std::make_move_iterator(profiles_change_local.updated.begin()), std::make_move_iterator(profiles_change_local.updated.end()));
    }
    {
        recently_closed_tabs_change->created.insert(recently_closed_tabs_change->created.end(),
            std::make_move_iterator(recently_closed_tabs_change_local.created.begin()), std::make_move_iterator(recently_closed_tabs_change_local.created.end()));
        recently_closed_tabs_change->deleted.insert(recently_closed_tabs_change->deleted.end(),
            std::make_move_iterator(recently_closed_tabs_change_local.deleted.begin()), std::make_move_iterator(recently_closed_tabs_change_local.deleted.end()));
    }
    SUCCEED();
}



} // namespace orion
