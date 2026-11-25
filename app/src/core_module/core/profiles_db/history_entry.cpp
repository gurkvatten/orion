#include <core/profiles_db/profiles_db.h>
#include <format>

namespace orion {

//-------------------------------------------------------------------------
// History entries
//-------------------------------------------------------------------------

Error
ProfilesDb::get_history_entry_ids(GetHistoryEntryIdsQuery query, std::vector<HistoryEntryId>* ids)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::vector<DbVariant> args = {
        query.profile_id.value,
        query.visited_after.value,
        query.visited_before.value,
    };
    std::vector<std::string> like_clauses;

    if (!query.search_filter.empty()) {
        like_clauses.push_back("website LIKE '%' || ? || '%' ESCAPE '\\'");
        args.push_back(Db::escape_like(query.search_filter, '\\'));

        like_clauses.push_back("address LIKE '%' || ? || '%' ESCAPE '\\'");
        args.push_back(Db::escape_like(query.search_filter, '\\'));
    }

    std::string where_clause = "WHERE profile_id = ?1 AND visited_at >= ?2 AND visited_at < ?3";

    if (!like_clauses.empty()) {
        where_clause += std::format(" AND ({})", Db::join_strings(like_clauses, " OR "));
    }

    std::string sql = std::format(R"(
        SELECT id FROM history_entries
            {}
            ORDER BY visited_at DESC;
    )", where_clause);

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        ids->push_back(HistoryEntryId(stmt.integer("id")));
    }
    VERIFY(stmt.error());
    SUCCEED();
}

static void
load_history_entry_from_stmt(DbStmt& stmt, std::vector<HistoryEntryDto>* entries)
{
    while (stmt.next()) {
        auto entry = HistoryEntryDto{};
        entry.id = HistoryEntryId(stmt.integer("id"));
        entry.profile_id = ProfileId(stmt.integer("profile_id"));
        entry.visited_at = UnixTimeUtc64(stmt.integer("_visited_at"));
        entry.website = stmt.text("website");
        entry.address = stmt.text("address");
        entries->emplace_back(entry);
    }
}

Error
ProfilesDb::get_history_entries(const std::vector<HistoryEntryId>& ids, std::vector<HistoryEntryDto>* entries)
{
    auto args = to_db_variant_vector(ids);

    //'AS _visited_at' is there to make it compatile with other queries that use 
    //load_history_entry_from_stmt but need to alias the visited_at
    std::string sql = std::format(R"(
        SELECT id, profile_id, visited_at AS _visited_at, website, address
        FROM history_entries
        WHERE id IN ({})
        ORDER BY _visited_at DESC;
    )", Db::make_placeholders(ids.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    load_history_entry_from_stmt(stmt, entries);
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_history_entries(const ProfileHistoryQuery& query, std::vector<HistoryEntryDto>* entries)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::vector<DbVariant> args = {
        query.profile_id.value,
    };
    std::vector<std::string> offset_limit_clauses;

    if (query.limit > 0) {
        offset_limit_clauses.push_back(std::format(R"( LIMIT {} )", query.limit));
    }
    if (query.offset > 0) {
        if (offset_limit_clauses.empty()) {
            offset_limit_clauses.push_back(" LIMIT -1 ");//cannot have offset without limit apparently
        }
        offset_limit_clauses.push_back(std::format(R"( OFFSET {} )", query.offset));
        args.push_back(query.offset);
    }

    std::string filter = query.search_filter.empty() ? "" : std::format(R"( AND {})", query.search_filter);
    std::string offset_limit_clause;

    if (!offset_limit_clauses.empty()) {
        offset_limit_clause = Db::join_strings(offset_limit_clauses, "");
    }
    //AND length(trim(website)) > 0 AND length(trim(address)) > 0 AND website != 'Failed to open page'
    //the tem view 'day' is needed to figure out limiting last day to display exactly limit number of days if avaiable
    //since there migh be empty days in between, hence we simply cannot set a query to > limit day back 
    std::string sql = std::format(R"(
WITH days AS (
	SELECT MIN(d._visited_at) AS min_at FROM (
        SELECT CAST(strftime('%s', datetime(strftime('%Y-%m-%d 00:00:00', datetime(visited_at, 'unixepoch', 'localtime')))) AS INTEGER)
                AS _visited_at
        FROM history_entries
            WHERE profile_id = ?1{}
		GROUP BY _visited_at
        ORDER BY _visited_at DESC {}
    ) AS d
)
SELECT id, profile_id,
    CAST(strftime('%s', datetime(strftime('%Y-%m-%d 00:00:00', datetime(visited_at, 'unixepoch', 'localtime')))) AS INTEGER)
        AS _visited_at, website, address				
FROM history_entries, days
        WHERE profile_id = ?1 AND visited_at >= days.min_at{}
	GROUP BY _visited_at, website, address
    ORDER BY _visited_at DESC;
    )", filter, offset_limit_clause, filter);

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));
        load_history_entry_from_stmt(stmt, entries);
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::get_nonempty_history_days(GetNonemptyHistoryDaysQuery query, std::vector<HistoryEntryDay>* days)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    std::vector<DbVariant> args = {
        query.profile_id.value,
    };
    std::vector<std::string> like_clauses;

    if (!query.search_filter.empty()) {
        like_clauses.push_back("website LIKE '%' || ? || '%' ESCAPE '\\'");
        args.push_back(Db::escape_like(query.search_filter, '\\'));

        like_clauses.push_back("address LIKE '%' || ? || '%' ESCAPE '\\'");
        args.push_back(Db::escape_like(query.search_filter, '\\'));
    }

    std::string where_clause = "WHERE profile_id = ?1";

    if (!like_clauses.empty()) {
        where_clause += std::format(" AND ({})", Db::join_strings(like_clauses, " OR "));
    }
    
    std::string sql = std::format(R"(
        SELECT DISTINCT
            CAST(strftime('%s', datetime(strftime('%Y-%m-%d 00:00:00', datetime(visited_at, 'unixepoch', 'localtime')), 'utc')) AS INTEGER)
                AS utc_day_start,
            COUNT(*) AS visit_count
        FROM history_entries
        {}
        GROUP BY utc_day_start
        ORDER BY utc_day_start DESC;
    )", where_clause);

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        HistoryEntryDay day{};
        day.start_of_day = UnixTimeUtc64(stmt.integer("utc_day_start"));
        day.visit_count = stmt.integer("visit_count");
        days->emplace_back(day);
    }
    VERIFY(stmt.error());
    SUCCEED();
}

Error
ProfilesDb::create_history_entry(
    CreateHistoryEntryQuery query, HistoryEntryId* entry_id, HistoryEntriesChange* entries_change,
    HistoryEntryDaysChange* days_change)
{
    if (query.profile_id == ProfileId(0)) {
        RAISE(ErrorCode::argument, "profile_id is not set");
    }

    DbStmt stmt;
    VERIFY(m_db->query(R"(
        INSERT INTO history_entries (profile_id, visited_at, website, address)
        VALUES (?, ?, ?, ?)
        RETURNING id;
    )", {
        query.profile_id.value,
        query.visited_at.value,
        query.website,
        query.address
    }, &stmt));

    bool has_row = stmt.next();
    VERIFY(stmt.error());
    RAISE_CHECK(has_row);

    HistoryEntryDto entry{};
    entry.id = HistoryEntryId(stmt.integer("id"));
    entry.profile_id = query.profile_id;
    entry.visited_at = query.visited_at;
    entry.website = query.website;
    entry.address = query.address;
    if (entries_change) {
        entries_change->created.emplace_back(entry);
    }

    if (entry_id) {
        *entry_id = entry.id;
    }

    {
        // notify about the change in the number of history entries for the day
        std::string sql = R"(
            SELECT COUNT(*) AS count,
                -- local start of the day converted to UTC
                CAST(strftime('%s', datetime(strftime('%Y-%m-%d 00:00:00', datetime(?2, 'unixepoch', 'localtime')), 'utc')) AS INTEGER)
                    AS utc_day_start
            FROM history_entries
            WHERE profile_id = ?1 AND
                -- local start of the day converted to UTC
                visited_at >= CAST(strftime('%s', datetime(strftime('%Y-%m-%d 00:00:00', datetime(?2, 'unixepoch', 'localtime')), 'utc')) AS INTEGER)
                AND
                -- local end of the day converted to UTC (exclusive)
                visited_at < CAST(strftime('%s', datetime(strftime('%Y-%m-%d 00:00:00', datetime(?2, 'unixepoch', 'localtime')), '+1 day', 'utc')) AS INTEGER);
        )";

        DbStmt stmt;
        VERIFY(m_db->query(sql, {
            query.profile_id.value,
            query.visited_at.value,
        }, &stmt));

        bool has_row = stmt.next();
        VERIFY(stmt.error());

        if (!has_row) {
            RAISE(ErrorCode::assert, "count query returned no rows");
        }

        HistoryEntryDay day{};
        day.start_of_day = UnixTimeUtc64(stmt.integer("utc_day_start"));
        day.visit_count = stmt.integer("count");

        if (days_change) {
            if (day.visit_count == 1) {
                // if this is the first entry for the day, we need to create a new day record
                days_change->created.push_back(day);
            } else {
                // otherwise, we just update the existing day record
                days_change->updated.push_back(day);
            }
        }
    }
    SUCCEED();
}

Error
ProfilesDb::delete_history_entries(
    const std::vector<HistoryEntryId>& ids, HistoryEntriesChange* entries_change, HistoryEntryDaysChange* days_change)
{
    // prepare notification about the change in the number of history entries for the day
    std::vector<UnixTimeUtc64> affected_utc_day_starts;
    auto args = to_db_variant_vector(ids);
    {
        std::string sql = std::format(R"(
            SELECT DISTINCT             
                -- local start of the day converted to UTC
                CAST(strftime('%s', datetime(strftime('%Y-%m-%d 00:00:00', datetime(visited_at, 'unixepoch', 'localtime')), 'utc')) AS INTEGER)
                    AS utc_day_start
            FROM history_entries
            WHERE id IN ({});
        )", Db::make_placeholders(ids.size()));

        DbStmt stmt;
        VERIFY(m_db->query(sql, args, &stmt));

        while (stmt.next()) {
            affected_utc_day_starts.push_back(UnixTimeUtc64(stmt.integer("utc_day_start")));
        }
        VERIFY(stmt.error());
    }

    std::string in_clause = Db::make_placeholders(ids.size());
    std::string sql = std::format(R"(
        DELETE FROM history_entries WHERE id IN ({})
        RETURNING id;
    )", Db::make_placeholders(ids.size()));

    DbStmt stmt;
    VERIFY(m_db->query(sql, args, &stmt));

    while (stmt.next()) {
        auto id = HistoryEntryId(stmt.integer("id"));
        if (entries_change) {
            entries_change->deleted.push_back(id);
        }
    }
    VERIFY(stmt.error());

    {
        // notify about the change in the number of history entries for the day
        std::string sql = std::format(R"(
            SELECT DISTINCT
                CAST(strftime('%s', datetime(strftime('%Y-%m-%d 00:00:00', datetime(visited_at, 'unixepoch', 'localtime')), 'utc')) AS INTEGER)
                    AS utc_day_start,
                COUNT(*) AS visit_count
            FROM history_entries
            WHERE utc_day_start IN ({})
            GROUP BY utc_day_start
            ORDER BY utc_day_start DESC;
        )", Db::make_placeholders(affected_utc_day_starts.size()));

        DbStmt stmt;
        VERIFY(m_db->query(sql, to_db_variant_vector(affected_utc_day_starts), &stmt));

        std::set<UnixTimeUtc64> deleted_days(affected_utc_day_starts.begin(), affected_utc_day_starts.end());

        while (stmt.next()) {
            auto day_start = UnixTimeUtc64(stmt.integer("utc_day_start"));
            
            deleted_days.erase(day_start); // remove the day from the deleted set because it was only updated

            HistoryEntryDay day{};
            day.start_of_day = day_start;
            day.visit_count = stmt.integer("visit_count");

            if (days_change) {
                days_change->updated.push_back(day);
            }
        }
        VERIFY(stmt.error());

        if (days_change) {
            for (const auto& day_start : deleted_days) {
                days_change->deleted.push_back(day_start);
            }
        }
    }
    SUCCEED();
}

Error
ProfilesDb::clear_history_entries(const ProfileHistoryQuery& query, HistoryEntryDaysChange* days_change)
{
    std::string sql = "DELETE FROM history_entries WHERE profile_id = ?";

    int64_t rows_affected = 0;
    VERIFY(m_db->exec(sql, {query.profile_id.value}, &rows_affected, nullptr));
    if (days_change) {
        HistoryEntryDay day{};
        day.start_of_day = UnixTimeUtc64(0);
        day.visit_count = rows_affected;
        if (day.visit_count > 0){
            days_change->deleted.push_back(day.start_of_day);
        }
    }
    
    SUCCEED();    
}


} // namespace orion