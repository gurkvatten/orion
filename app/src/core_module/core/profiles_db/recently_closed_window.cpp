#include <core/profiles_db/profiles_db.h>
#include <format>

namespace orion {

//-------------------------------------------------------------------------
// Recently Closed Windows
//-------------------------------------------------------------------------

std::vector<std::string>
ProfilesDb::get_recently_closed_window_columns() const
{
    return {
        "id",
        "profile_id",
        "name",
        "closed_at",
        "icon_name",
    };
}

void
ProfilesDb::load_recently_closed_window_from_stmt(const DbStmt& stmt, RecentlyClosedWindowDto* window) const
{
    window->id = RecentlyClosedWindowId(stmt.integer("id"));
    window->profile_id = ProfileId(stmt.integer("profile_id"));
    window->name = stmt.text("name");
    window->closed_at = UnixTimeUtc64(stmt.integer("closed_at"));
    window->icon_name = stmt.text("icon_name");
}

Error
ProfilesDb::create_recently_closed_window(CreateRecentlyClosedWindowQuery query, RecentlyClosedWindowId* id)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::string sql = std::format(R"(
        INSERT INTO recently_closed_windows ({})
        VALUES ({})
        RETURNING {};
    )", Db::join_strings(get_recently_closed_window_columns(), ", "),
        Db::make_placeholders(get_recently_closed_window_columns().size()),
        Db::join_strings(get_recently_closed_window_columns(), ", ")
    );

    std::vector<DbVariant> args = {
        nullptr, // id will be auto-generated
        query.profile_id.value,
        query.name,
        query.closed_at.value,
        query.icon_name,
    };

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    RecentlyClosedWindowDto new_window{};
    load_recently_closed_window_from_stmt(stmt, &new_window);

    if (id) {
        *id = new_window.id;
    }

    // delete all but the most recent N windows for this profile
    {
        int64_t max_windows = 20; // make this configurable if needed
        std::string sql = R"(
            DELETE FROM recently_closed_windows
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
            max_windows,
        };
        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));
        while (stmt.next()) {
            [[maybe_unused]] auto deleted_id = RecentlyClosedTabId(stmt.integer("id"));
        }
        VERIFY(stmt.error());
    }
    SUCCEED();
}

Error
ProfilesDb::get_recently_closed_window_ids(ProfileId profile_id, std::vector<RecentlyClosedWindowId>* ids)
{
    if (profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    DbStmt stmt;
    VERIFY(m_db->query(R"(
        SELECT id FROM recently_closed_windows
        WHERE profile_id = ?1
        ORDER BY closed_at DESC;
    )", {profile_id.value}, &stmt));

    while (stmt.next()) {
        ids->push_back(RecentlyClosedWindowId(stmt.integer("id")));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_recently_closed_windows(
    const std::vector<RecentlyClosedWindowId>& ids, std::vector<RecentlyClosedWindowDto>* windows)
{
    auto args = to_db_variant_vector(ids);

    std::string sql = std::format(R"(
        SELECT {}
        FROM recently_closed_windows
        WHERE id IN ({})
        ORDER BY profile_id, closed_at DESC;
    )", Db::join_strings(get_recently_closed_window_columns(), ", "),
        Db::make_placeholders(ids.size())
    );

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        RecentlyClosedWindowDto window{};
        load_recently_closed_window_from_stmt(stmt, &window);
        windows->emplace_back(window);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::delete_recently_closed_windows(const std::vector<RecentlyClosedWindowId>& ids)
{
    auto args = to_db_variant_vector(ids);

    std::string sql = std::format(R"(
        DELETE FROM recently_closed_windows
        WHERE id IN ({});
    )", Db::make_placeholders(ids.size())
    );

    VERIFY(m_db->exec(sql, args));
    SUCCEED();
}

} // namespace orion