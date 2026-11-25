#include <core/profiles_db/profiles_db.h>
#include <format>

namespace orion {

//-------------------------------------------------------------------------
// Recently Closed Tabs
//-------------------------------------------------------------------------

std::vector<std::string>
ProfilesDb::get_recently_closed_tab_columns() const
{
    return {
        "id",
        "profile_id",
        "name",
        "uri",
        "closed_at",
        "icon_name",
        "window_id",
    };
}

void
ProfilesDb::load_recently_closed_tab_from_stmt(const DbStmt& stmt, RecentlyClosedTabDto* tab) const
{
    tab->id = RecentlyClosedTabId(stmt.integer("id"));
    tab->profile_id = ProfileId(stmt.integer("profile_id"));
    tab->name = stmt.text("name");
    tab->uri = stmt.text("uri");
    tab->closed_at = UnixTimeUtc64(stmt.integer("closed_at"));
    tab->icon_name = stmt.text("icon_name");
    tab->window_id = stmt.integer_optional("window_id");
}

Error
ProfilesDb::get_recently_closed_tab_ids(GetRecentlyClosedTabIdsQuery query, std::vector<RecentlyClosedTabId>* ids)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }
    RAISE_CHECK(ids);

    std::vector<DbVariant> args;
    std::vector<std::string> conditions;

    conditions.push_back("profile_id = ?1");
    args.push_back(query.profile_id.value);

    if (query.window_id.has_value()) {
        if (query.window_id.value().value == 0) {
            conditions.push_back("window_id IS NULL");
        } else {
            conditions.push_back("window_id = ?"+std::to_string(args.size() + 1));
            args.push_back(query.window_id->value);
        }
    }

    std::string sql = std::format(R"(
        SELECT id FROM recently_closed_tabs
        WHERE {}
        ORDER BY closed_at DESC;
    )", Db::join_strings(conditions, " AND "));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        RecentlyClosedTabId id = RecentlyClosedTabId(stmt.integer("id"));
        ids->push_back(id);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_recently_closed_tabs(
    const std::vector<RecentlyClosedTabId>& ids, std::vector<RecentlyClosedTabDto>* tabs)
{
    RAISE_CHECK(tabs);

    auto args = to_db_variant_vector(ids);

    std::string sql = std::format(R"(
        SELECT {}
        FROM recently_closed_tabs
        WHERE id IN ({})
        ORDER BY closed_at DESC;
    )", Db::join_strings(get_recently_closed_tab_columns(), ", "),
        Db::make_placeholders(ids.size())
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        RecentlyClosedTabDto tab{};
        load_recently_closed_tab_from_stmt(stmt, &tab);
        tabs->push_back(tab);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::create_recently_closed_tab(
    CreateRecentlyClosedTabQuery query, RecentlyClosedTabId* id, RecentlyClosedTabsChange* change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::string sql = std::format(R"(
        INSERT INTO recently_closed_tabs ({})
        VALUES ({})
        RETURNING {};
    )", Db::join_strings(get_recently_closed_tab_columns(), ", "),
        Db::make_placeholders(get_recently_closed_tab_columns().size()),
        Db::join_strings(get_recently_closed_tab_columns(), ", ")
    );

    std::vector<DbVariant> args = {
        nullptr, // id will be auto-generated
        query.profile_id.value,
        query.name,
        query.uri,
        query.closed_at.value,
        query.icon_name,
        query.window_id.has_value() ? DbVariant(query.window_id->value) : DbVariant(nullptr),
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    RecentlyClosedTabDto new_tab{};
    load_recently_closed_tab_from_stmt(stmt, &new_tab);

    if (change) {
        change->created.push_back(new_tab);
    }

    if (id) {
        *id = new_tab.id;
    }

    // delete all but the most recent N tabs for this profile
    {
        int64_t max_tabs = 20; // make this configurable if needed
        std::string sql = R"(
            DELETE FROM recently_closed_tabs
            WHERE id IN (
                SELECT id FROM recently_closed_tabs
                WHERE profile_id = ?1
                ORDER BY closed_at DESC
                LIMIT -1 OFFSET ?2
            )
            RETURNING id;
        )";
        std::vector<DbVariant> args = {
            query.profile_id.value,
            max_tabs,
        };
        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        while (stmt.next()) {
            auto deleted_id = RecentlyClosedTabId(stmt.integer("id"));
            if (change) {
                change->deleted.push_back(deleted_id);
            }
        }
        VERIFY(stmt.error());
    }
    SUCCEED();
}

// does not report changes of other tabs that had their window_id reset
Error
ProfilesDb::delete_recently_closed_tabs(
    DeleteRecentlyClosedTabsQuery query, TxMode tx_mode, RecentlyClosedTabsChange* change)
{
    DbTx tx;
    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.begin(m_db));
    }

    if (query.reset_window_ids) {
        // for each deleted tab, reset window_id to NULL of other tabs that had the same window_id
        // and delete the window with this window_id
        std::vector<DbVariant> window_ids;
        // first, get all window_id's of associated windows
        {
            std::string sql = std::format(R"(
                SELECT window_id FROM recently_closed_tabs
                WHERE id IN ({}) AND window_id IS NOT NULL;
            )", Db::make_placeholders(query.ids.size()));

            auto args = to_db_variant_vector(query.ids);

            DbStmt stmt;
            VERIFY(m_db->query(sql, args, &stmt));

            while (stmt.next()) {
                window_ids.push_back(DbVariant(stmt.integer("window_id")));
            }
            VERIFY(stmt.error());
        }
        // then, reset window_id to NULL of other tabs that had the same window_id
        {
            std::string sql = std::format(R"(
                UPDATE recently_closed_tabs
                SET window_id = NULL
                WHERE window_id IN ({});
            )", Db::make_placeholders(window_ids.size()));

            // we do not report these changes at the moment => simple exec is sufficient
            VERIFY(m_db->exec(sql, window_ids));
        }
        // finally, delete the windows themselves
        {
            std::string sql = std::format(R"(
                DELETE FROM recently_closed_windows
                WHERE id IN ({});
            )", Db::make_placeholders(window_ids.size()));

            // we do not report these changes at the moment => simple exec is sufficient
            VERIFY(m_db->exec(sql, window_ids));
        }
    }

    auto args = to_db_variant_vector(query.ids);

    std::string sql = std::format(R"(
        DELETE FROM recently_closed_tabs
        WHERE id IN ({})
        RETURNING id;
    )", Db::make_placeholders(query.ids.size()),
        Db::join_strings(get_recently_closed_tab_columns(), ", ")
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto id = RecentlyClosedTabId(stmt.integer("id"));
        if (change) {
            change->deleted.push_back(id);
        }
    }
    VERIFY(stmt.error());

    if (tx_mode == TxMode::use_tx) {
        VERIFY(tx.commit());
    }

    SUCCEED();
}

} // namespace orion